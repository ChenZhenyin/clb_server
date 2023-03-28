#include "processpool.h"

processpool* g_processpool = NULL;
processpool* processpool :: instance = NULL;

static void sig_handler(int sig) {

	int save_errno = errno;
	int msg = sig;
	send(sig_pipefd[1], (char*)&msg, 1, 0);
	errno = save_errno;
}

static void addsig(int sig, void(handler)(int), bool restart = true) {

	struct sigaction sa;
	memset(&sa, '\0', sizeof(sa));
	sa.sa_handler = handler;
	if (restart) {
		sa.sa_flags |= SA_RESTART;
	}
	sigfillset(&sa.sa_mask);
	assert(sigaction(sig, &sa, NULL) != -1);
}

processpool :: processpool(int listenfd, int processNumber) 
	: _listenfd(listenfd), _processNumber(processNumber), index(-1), isStop(false) {

	assert((processNumber > 0) && (processNumber <= MAX_PROCESS_NUMBER));

	processList = new process[processNumber];
	assert(processList);

	for (int i = 0; i < processNumber; ++i) {
		int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, processList[i].pipefd);
		assert(ret == 0);

		processList[i].pid = fork();
		if (processList[i].pid > 0) {
			processList[i].busyConnCount = 0;
			close(processList[i].pipefd[1]);
			continue;
		} else {
			index = i;
			close(processList[i].pipefd[0]);
			break;
		}
	}
}

int processpool :: getMostFreeServer() {

	int idx = 0;
	int min = processList[0].busyConnCount;
	log(LOG_DEBUG, __FILE__, __LINE__, "show 0th child busyConnCount : %d", min);
	for (int i = 1; i < _processNumber; ++i) {
		log(LOG_DEBUG, __FILE__, __LINE__, "show %dth child busyConnCount : %d", i, processList[i].busyConnCount);
		if (processList[i].busyConnCount < min) {
			idx = i;
			min = processList[i].busyConnCount;
		}
	}
	return idx;
}

void processpool :: setupSigPipe() {

	epollfd = epoll_create(5);
	assert(epollfd != -1);

	int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
	assert(ret != -1);

	setnonblocking(sig_pipefd[1]);
	add_read_fd(epollfd, sig_pipefd[0]);

	addsig(SIGCHLD, sig_handler);
	addsig(SIGTERM, sig_handler);
	addsig(SIGINT, sig_handler);
	addsig(SIGPIPE, SIG_IGN);
}

void processpool :: run(const vector<Host>& arg) {

	if (index != -1) {
		runChild(arg);
		return;
	}
	runParent();
}

void processpool :: sendBusyConnCountNotify(int pipefd, manager* mgr) {

	int msg = mgr->getUsedConnCount();
	send(pipefd, (char*)&msg, 1, 0);
	log(LOG_DEBUG, __FILE__, __LINE__, "Send Conn Count to mgr : %d", msg);
}

void processpool :: runParent() {

	setupSigPipe();
	add_read_fd(epollfd, _listenfd);
	for (int i = 0; i < _processNumber; ++i) {
		add_read_fd(epollfd, processList[i].pipefd[0]);
	}

	epoll_event events[MAX_EVENT_NUMBER];
	int newConn = 1;
	int number = 0;
	int ret = -1;

	while (!isStop) {
		number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, EPOLL_WAIT_TIME);
		if ((number < 0) && (errno != EINTR)) {
			log(LOG_ERR, __FILE__, __LINE__, "%s", "Epoll failure!");
			break;
		}

		for (int i = 0; i < number; ++i) {
			int sockfd = events[i].data.fd;
			if (sockfd == _listenfd) {
				int idx = getMostFreeServer();
				send(processList[idx].pipefd[0], (char*)&newConn, sizeof(newConn), 0);
				log(LOG_DEBUG, __FILE__, __LINE__, "Send request to %dth child success!", idx);
			} else if ((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN)) {
				int sig;
				char signals[1024];
				ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
				if (ret <= 0) {
					continue;
				} else {
					for (int i = 0; i < ret; ++i) {
						switch (signals[i]) {
							case SIGCHLD: {
								pid_t childPid;
								int stat;
								while ((childPid = waitpid(-1, &stat, WNOHANG)) > 0) {
									for (int i = 0; i < _processNumber; ++i) {
										if (processList[i].pid == childPid) {
											log(LOG_INFO, __FILE__, __LINE__, "Child %d leave!", i);
											processList[i].pid = -1;
											close(processList[i].pipefd[0]);
											break;
										}
									}
								}

								isStop = true;
								for (int i = 0; i < _processNumber; ++i) {
									if (processList[i].pid != -1) {
										isStop = false;
										break;
									}
								}

								break;
							}
							case SIGTERM: {
								break;
							}
							case SIGINT: {
								log(LOG_INFO, __FILE__, __LINE__, "Kill all the child now!: %d", _processNumber);
								for (int i = 0; i < _processNumber; ++i) {
									int childPid = processList[i].pid;
									if (childPid != -1) {
										kill(childPid, SIGTERM);
									}
								}
								break;
							}
							default: {
								break;
							}
						}
					}
				}
			} else if (events[i].events & EPOLLIN) {
				int busyConnCount = 0;
				ret = recv(sockfd, (char*)&busyConnCount, sizeof(busyConnCount), 0);
				if (((ret < 0) && (errno != EAGAIN)) || ret == 0) {
					continue;
				}
				for (int i = 0; i < _processNumber; ++i) {
					if (sockfd == processList[i].pipefd[0]) {
						log(LOG_DEBUG, __FILE__, __LINE__, "%dth child busyConnCount : %d", i, busyConnCount);
						processList[i].busyConnCount = busyConnCount;
						break;
					}
				}
			}
		}
	}

	for (int i = 0; i < _processNumber; ++i) {
		closefd(epollfd, processList[i].pipefd[0]);
	}
	close(epollfd);
}

void processpool :: runChild(const vector<Host>& arg) {

	setupSigPipe();
	int pipefd = processList[index].pipefd[1];
	add_read_fd(epollfd, pipefd);

	manager* mgr = new manager(epollfd, arg[index]);
	assert(mgr);

	epoll_event events[MAX_EVENT_NUMBER];
	int number = 0;
	int ret = -1;

	while (!isStop) {
		number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, EPOLL_WAIT_TIME);
		if ((number < 0) && (errno != EINTR)) {
			log(LOG_ERR, __FILE__, __LINE__, "%s", "Epoll failure!");
			break;
		}

		if (number == 0) {
			mgr->recycleFreedConns();
			continue;
		}

		for (int i = 0; i < number; ++i) {
			int sockfd = events[i].data.fd;
			if ((sockfd == pipefd) && (events[i].events & EPOLLIN)) {
				int client = 0;
				ret = recv(sockfd, (char*)&client, sizeof(client), 0);
				if (((ret < 0) && (errno != EAGAIN)) || ret == 0) {
					continue;
				} else {
					struct sockaddr_in clientAddress;
					socklen_t clientAddressLen = sizeof(clientAddress);
					int connfd = accept(_listenfd, (struct sockaddr*)&clientAddress, &clientAddressLen);
					if (connfd < 0) {
						log(LOG_ERR, __FILE__, __LINE__, "errno: %s", strerror(errno));
						continue;
					}
					add_read_fd(epollfd, connfd);
					Conn* conn = mgr->chooseConn(connfd, clientAddress);
					if (!conn) {
						closefd(epollfd, connfd);
						continue;
					}
					sendBusyConnCountNotify(pipefd, mgr);
				}
			} else if ((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN)) {
				int sig;
				char signals[1024];
				ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
				if (ret <= 0) {
					continue;
				} else {
					for (int i = 0; i < ret; ++i) {
						switch (signals[i]) {
							case SIGCHLD: {
								pid_t _pid;
								int stat;
								while ((_pid = waitpid(-1, &stat, WNOHANG)) > 0) {
									continue;
								}
								break;
							}
							case SIGTERM: {
								break;
							}
							case SIGINT: {
								isStop = true;
								break;
							}
							default: {
								break;
							}
						}
					}
				}
			} else if (events[i].events & EPOLLIN) {
				log(LOG_DEBUG, __FILE__, __LINE__, "%dth child get input", index);
				RET_CODE result = mgr->process(sockfd, READ);
				switch (result) {
					case CLOSED: {
						sendBusyConnCountNotify(pipefd, mgr);
						break;
					}
					default: {
						break;
					}
				}
			} else if (events[i].events & EPOLLOUT) {
				log(LOG_DEBUG, __FILE__, __LINE__, "%d child get output", index);
				RET_CODE result = mgr->process(sockfd, WRITE);
				switch (result) {
					case CLOSED: {
						sendBusyConnCountNotify(pipefd, mgr);
						break;
					}
					default: {
						break;
					}
				}
			} else {
				// TODO
			}
		}
	}

	close(pipefd);
	close(epollfd);
}
