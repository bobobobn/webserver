#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/socket.h>
#include <errno.h>
#include <cstdlib>
#include <unistd.h>
#include <cstring>
#include <cassert>
#include <time.h>
#include <sys/time.h>

#ifndef UTIL_H
#define UTIL_H

class Utils{
public:
    void add_fd(int epoll_fd, int fd, bool one_shot);
    static void sig_handle(int sig);
    void add_sig(int sig, void (sig_handler) (int));

public:
    static int *sig_pipe;    
};
inline int64_t gettimeofday_us() {
    timeval now;
    ::gettimeofday(&now, NULL);
    return now.tv_sec * 1000000L + now.tv_usec;
}
//对文件描述符设置非阻塞
int setNonBlocking(int fd);

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot, bool ET);

//从内核时间表删除描述符
void removefd(int epollfd, int fd);

void modfd(int epollfd, int fd, int ev);
#define DISALLOW_COPY_MOVE_AND_ASSIGN(TypeName) TypeName(const TypeName&) = delete; TypeName(const TypeName&&) = delete;  TypeName& operator=(const TypeName&) = delete
#endif