#ifndef __MANAGER_H__
#define __MANAGER_H__

#include <map>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <assert.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "conn.h"

#define HOST_NAME_SIZE 1024

using std::map;

class Host {
public:
	char hostName[HOST_NAME_SIZE];
	int port;
	int connectCount;
};

class manager {
public:
	manager(int epollfd, const Host& server);
	~manager();
	int connectServer(const sockaddr_in& address);
	Conn* chooseConn(int cltfd, const sockaddr_in& cltAddr);
	void freeConn(Conn* connection);
	void recycleFreedConns();
	RET_CODE process(int fd, OP_TYPE type);

	int getUsedConnCount() {return used.size();};

private:
	static int _epollfd;
	map<int, Conn*> conns;
	map<int, Conn*> used;
	map<int, Conn*> freed;
	Host realServer;
};

#endif
