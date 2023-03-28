#include "manager.h"

#define __MY_TRY__ 	 try
#define __MY_CATCH__ catch(...)

using std::pair;

int manager :: _epollfd = -1;

manager :: manager(int epollfd, const Host& server) : realServer(server) {

	_epollfd = epollfd;
	struct sockaddr_in address;
	bzero(&address, sizeof(address));
	address.sin_family = AF_INET;
	inet_pton(AF_INET, server.hostName, &address.sin_addr);
	address.sin_port = htons(server.port);
	log(LOG_INFO, __FILE__, __LINE__, "Logcial Server Host info:(%s:%d)", server.hostName, server.port);

	for (int i = 0; i < server.connectCount; ++i) {
		sleep(1);
		int sockfd = connectServer(address);
		if (sockfd < 0) {
			log(LOG_ERR, __FILE__, __LINE__, "Build %dth connection failed!!", i);
		} else {
			log(LOG_INFO, __FILE__, __LINE__, "Build %dth connection to logcial server success!", i);
			Conn* temp = NULL;
			__MY_TRY__
			{
				temp = new Conn;
			} 
			__MY_CATCH__
			{
				close(sockfd);
				continue;
			}
			temp->srvInit(sockfd, address);
			conns.insert(pair<int, Conn*>(sockfd, temp));
		}
	}
}

manager :: ~manager() {
}

int manager :: connectServer(const sockaddr_in& address) {

	int sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		return -1;
	}

	if (connect(sockfd, (struct sockaddr*)&address, sizeof(address)) != 0) {
		close(sockfd);
		return -1;
	}

	return sockfd;
}

Conn* manager :: chooseConn(int cltfd, const sockaddr_in& cltAddr) {

	if (conns.empty()) {
		log(LOG_ERR, __FILE__, __LINE__, "%s", "The connections to logcial server is not enough!");
		return NULL;
	}

	map<int, Conn*> :: iterator iter = conns.begin();
	int srvfd = iter->first;
	Conn* temp = iter->second;
	if (!temp) {
		log(LOG_ERR, __FILE__, __LINE__, "%s", "The logcial server connection object is empty!");
		return NULL;
	}

	conns.erase(iter);
	used.insert(pair<int, Conn*>(cltfd, temp));
	used.insert(pair<int, Conn*>(srvfd, temp));
	add_read_fd(_epollfd, cltfd);
	add_read_fd(_epollfd, srvfd);
	temp->cltInit(cltfd, cltAddr);
	log(LOG_INFO, __FILE__, __LINE__, "Bind the client sock %d with server sock %d success!", cltfd, srvfd);
	return temp;
}

void manager :: freeConn(Conn* connection) {

	int cltfd = connection->cltfd;
	int srvfd = connection->srvfd;
	closefd(_epollfd, cltfd);
	closefd(_epollfd, srvfd);
	used.erase(cltfd);
	used.erase(srvfd);
	connection->init();
	freed.insert(pair<int, Conn*>(srvfd, connection));
}

void manager :: recycleFreedConns() {

	if (freed.empty()) {
		return;
	}

	for (map<int, Conn*> :: iterator iter = freed.begin(); iter != freed.end(); iter++) {
		sleep(1);
		Conn* temp = iter->second;
		int srvfd = connectServer(temp->srvAddress);
		if (srvfd < 0) {
			log(LOG_ERR, __FILE__, __LINE__, "%s", "Fix connection failed!");
		} else {
			log(LOG_INFO, __FILE__, __LINE__, "%s", "Fix connection success!");
			temp->srvInit(srvfd, temp->srvAddress);
			conns.insert(pair<int, Conn*>(srvfd, temp));
		}
	}
	freed.clear();
}

RET_CODE manager :: process(int fd, OP_TYPE type) {

	Conn* conn = used[fd];
	if (!conn) {
		return NOTHING;
	}

	if (conn->cltfd == fd) {
		// client task
		int srvfd = conn->srvfd;
		switch (type) {
			case READ: {
				RET_CODE result = conn->cltRead();
				switch (result) {
					case OK: {
						log(LOG_DEBUG, __FILE__, __LINE__, "Content read from client: %s", conn->cltBuf);
						// 如果想要缓冲区满再让服务端读则注释该行
						modfd(_epollfd, srvfd, EPOLLOUT);
						break;
					}
					case BUFFER_FULL: {
						modfd(_epollfd, srvfd, EPOLLOUT);
						break;
					}
					case IOERR: {
						break;
					}
					case CLOSED: {
						freeConn(conn);
						return CLOSED;
					}
					default: {
						break;
					}
				}

				if (conn->isSrvClosed) {
					freeConn(conn);
					log(LOG_ERR, __FILE__, __LINE__, "%s", "The connection is close!");
					return CLOSED;
				}
				break;
			}
			case WRITE: {
				RET_CODE result = conn->cltWrite();
				switch (result) {
					case TRY_AGAIN: {
						modfd(_epollfd, fd, EPOLLOUT);
						break;
					}
					case BUFFER_EMPTY: {
						modfd(_epollfd, fd, EPOLLIN);
						modfd(_epollfd, srvfd, EPOLLIN);
						break;
					}
					case IOERR: {
						break;
					}
					case CLOSED: {
						freeConn(conn);
						return CLOSED;
					}
					default: {
						break;
					}
				}

				if (conn->isSrvClosed) {
					freeConn(conn);
					return CLOSED;
				}
				break;
			}
			default: {
				log(LOG_ERR, __FILE__, __LINE__, "%s", "Other operation not support yet!");
				break;
			}
		}
	} else if (conn->srvfd == fd) {
		int cltfd = conn->cltfd;
		switch (type) {
			case READ: {
				RET_CODE result = conn->srvRead();
				switch (result) {
					case OK: {
						log(LOG_DEBUG, __FILE__, __LINE__, "Content read from server: %s", conn->srvBuf);
						// 如果想要缓冲区满再让客户端读则注释该行
						modfd(_epollfd, cltfd, EPOLLOUT);
						break;
					}
					case BUFFER_FULL: {
						modfd(_epollfd, cltfd, EPOLLOUT);
						break;
					}
					case IOERR: {
						break;
					}
					case CLOSED: {
						modfd(_epollfd, cltfd, EPOLLOUT);
						conn->isSrvClosed = true;
						break;
					}
					default: {
						break;
					}
				}
				break;
			}
			case WRITE: {
				RET_CODE result = conn->srvWrite();
				switch (result) {
					case TRY_AGAIN: {
						modfd(_epollfd, fd, EPOLLOUT);
						break;
					}
					case BUFFER_EMPTY: {
						modfd(_epollfd, fd, EPOLLIN);
						modfd(_epollfd, cltfd, EPOLLIN);
						break;
					}
					case IOERR: {
						break;
					}
					case CLOSED: {
						modfd(_epollfd, cltfd, EPOLLOUT);
						conn->isSrvClosed = true;
						break;
					}
					default: {
						break;
					}
				}
				break;
			}
			default: {
				log(LOG_ERR, __FILE__, __LINE__, "%s", "Other operation not support yet!");
				break;
			}
		}
	} else {
		return NOTHING;
	}

	return OK;
}
