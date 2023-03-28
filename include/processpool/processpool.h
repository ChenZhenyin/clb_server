#ifndef __PROCESSPOOL_H__
#define __PROCESSPOOL_H__

#include <vector>

#include "manager.h"

#define MAX_PROCESS_NUMBER 16
#define USER_PER_PROCESS 65536
#define MAX_EVENT_NUMBER 10000
#define EPOLL_WAIT_TIME 5000

using std::vector;

class process {
public:
	process() : pid(-1) {}

public:
	int busyConnCount;
	pid_t pid;
	int pipefd[2];
};

class processpool {
private:
	processpool(int listenfd, int processNumber = 8);
public:
	static processpool* create(int listenfd, int processNumber = 8) {
		if (!instance) {
			instance = new processpool(listenfd, processNumber);
		}
		return instance;
	}

	~processpool() {
		delete []processList;
	}

	void run(const vector<Host>& arg);

private:
	int getMostFreeServer();
	void setupSigPipe();
	void runChild(const vector<Host>& arg);
	void runParent();
	void sendBusyConnCountNotify(int pipefd, manager* mgr);

private:
	int _processNumber;
	int index;
	int epollfd;
	int _listenfd;
	int isStop;
	process* processList;
	static processpool* instance;
};

static int sig_pipefd[2];
extern processpool* g_processpool;

#endif
