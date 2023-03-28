#include "conn.h"

Conn :: Conn() {

	srvfd = -1;
	cltBuf = new char[BUF_SIZE];
	if (!cltBuf) {
		throw std::exception();
	}
	srvBuf = new char[BUF_SIZE];
	if (!srvBuf) {
		throw std::exception();
	}
	init();
}

Conn :: ~Conn() {

	delete []cltBuf;
	delete []srvBuf;
}

void Conn :: cltInit(int sockfd, const sockaddr_in& cltAddr) {

	cltfd = sockfd;
	cltAddress = cltAddr;
}

void Conn :: srvInit(int sockfd, const sockaddr_in& srvAddr) {

	srvfd = sockfd;
	srvAddress = srvAddr;
}

void Conn :: init() {

	cltfd = -1;
	cltReadIndex = 0;
	srvReadIndex = 0;
	cltWriteIndex = 0;
	srvWriteIndex = 0;
	isSrvClosed = false;
	memset(cltBuf, '\0', BUF_SIZE);
	memset(srvBuf, '\0', BUF_SIZE);
}

RET_CODE Conn :: cltRead() {

	int readBytes = 0;
	while (true) {
		if (cltReadIndex >= BUF_SIZE) {
			log(LOG_ERR, __FILE__, __LINE__, "%s", "The client read buffer is full!");
			return BUFFER_FULL;
		}

		readBytes = recv(cltfd, cltBuf + cltReadIndex, BUF_SIZE - cltReadIndex, 0);
		if (readBytes == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				break;
			}
			return IOERR;
		} else if (readBytes == 0) {
			return CLOSED;
		}

		cltReadIndex += readBytes;
	}

	return ((cltReadIndex - cltWriteIndex) > 0) ? OK : NOTHING;
}

RET_CODE Conn :: srvRead() {

	int readBytes = 0;
	while (true) {
		if (srvReadIndex >= BUF_SIZE) {
			log(LOG_ERR, __FILE__, __LINE__, "%s", "The server read buffer is full!");
			return BUFFER_FULL;
		}

		readBytes = recv(srvfd, srvBuf + srvReadIndex, BUF_SIZE - srvReadIndex, 0);
		if (readBytes == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				break;
			}
			return IOERR;
		} else if (readBytes == 0) {
			log(LOG_ERR, __FILE__, __LINE__, "%s", "The server should not close the connection!");
			return CLOSED;
		}

		srvReadIndex += readBytes;
	}

	return ((srvReadIndex - srvWriteIndex) > 0) ? OK : NOTHING;
}

RET_CODE Conn :: cltWrite() {

	int writeBytes = 0;
	while (true) {
		if (srvReadIndex <= srvWriteIndex) {
			srvReadIndex = 0;
			srvWriteIndex = 0;
			return BUFFER_EMPTY;
		}

		writeBytes = send(cltfd, srvBuf + srvWriteIndex, srvReadIndex - srvWriteIndex, 0);
		if (writeBytes == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return TRY_AGAIN;
			}
			log(LOG_ERR, __FILE__, __LINE__, "Write client socket failed!, errno: %s", strerror(errno));
			return IOERR;
		} else if (writeBytes == 0) {
			return CLOSED;
		}

		srvWriteIndex += writeBytes;
	}
}

RET_CODE Conn :: srvWrite() {

	int writeBytes = 0;
	while (true) {
		if (cltReadIndex <= cltWriteIndex) {
			cltReadIndex = 0;
			cltWriteIndex = 0;
			return BUFFER_EMPTY;
		}

		writeBytes = send(srvfd, cltBuf + cltWriteIndex, cltReadIndex - cltWriteIndex, 0);
		if (writeBytes == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return TRY_AGAIN;
			}
			log(LOG_ERR, __FILE__, __LINE__, "Write server socket failed!, errno: %s", strerror(errno));
			return IOERR;
		} else if (writeBytes == 0) {
			return CLOSED;
		}

		cltWriteIndex += writeBytes;
	}
}
