#ifndef __CONN_H__
#define __CONN_H__

#include <arpa/inet.h>
#include <exception>
#include <errno.h>

#include "log.h"
#include "fdwrapper.h"

class Conn {
public:
	Conn();
	~Conn();
	void cltInit(int sockfd, const sockaddr_in& cltAddr);
	void srvInit(int sockfd, const sockaddr_in& srvAddr);
	void init();

	RET_CODE cltRead();
	RET_CODE srvRead();
	RET_CODE cltWrite();
	RET_CODE srvWrite();

public:
	static const int BUF_SIZE = 2048;

	char* cltBuf;
	int cltReadIndex;
	int cltWriteIndex;
	sockaddr_in cltAddress;
	int cltfd;

	char* srvBuf;
	int srvReadIndex;
	int srvWriteIndex;
	sockaddr_in srvAddress;
	int srvfd;

	bool isSrvClosed;
};

#endif 
