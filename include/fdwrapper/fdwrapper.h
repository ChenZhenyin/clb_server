#ifndef __FDWRAPPER_H__
#define __FDWRAPPER_H__

#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>

// 通信读写结果枚举类
enum RET_CODE {
	OK = 0,
	NOTHING = 1,
	TRY_AGAIN = 2,
	IOERR = -1,
	CLOSED = -2,
	BUFFER_FULL = -3,
	BUFFER_EMPTY = -4
};

enum OP_TYPE {READ, WRITE, ERROR};

// 封装一些epoll操作
int setnonblocking(int fd);
void add_read_fd(int epollfd, int fd);
void add_write_fd(int epollfd, int fd);
void removefd(int epollfd, int fd);
void closefd(int epollfd, int fd);
void modfd(int epollfd, int fd, int ev);

#endif
