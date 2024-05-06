#include "util.h"

int* Utils::sig_pipe = nullptr;
void Utils::add_fd(int epoll_fd, int fd, bool one_shot){
    epoll_event event;
    event.data.fd = fd;
    if(one_shot){
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLERR | EPOLLONESHOT;
    }
    else{
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLERR;
    }
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    setNonBlocking(fd);
}

void Utils::sig_handle(int sig){
    int old_errno = errno;
    send(sig_pipe[1], (char*) &sig, 1, 0);
    errno = old_errno;
}

void Utils::add_sig(int sig, void (sig_handler) (int)){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

//对文件描述符设置非阻塞
int setNonBlocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

//将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot, bool ET = true)
{
    epoll_event event;
    event.data.fd = fd;
    if(ET){
        event.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLERR;
    }
    else
    {
        event.events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    }
    if(one_shot){
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setNonBlocking(fd);
}

//从内核时间表删除描述符
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

void modfd(int epollfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}