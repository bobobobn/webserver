#include<iostream>
#include<sys/socket.h>
#include<unistd.h>
#include<cstdlib>
#include<cstring>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/signal.h>
#include<assert.h>
#include<fcntl.h>
#include <poll.h>
#include <sys/epoll.h>
#include <map>
#include <unordered_map>

#define MAX_USER 5
#define BUF_SIZE 64
#define MAX_FD 65535
#define MAX_EVENTS 10

struct user_data{
    sockaddr_in user_addr;
    char* write_buf;
    char read_buf[BUF_SIZE];
};
int stop = false;
void handle_term(int sig){
    stop = true;
}

int setNonBlocking(int fd){
    int old_opt = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, old_opt|O_NONBLOCK);
    return old_opt;
}

void add_fd(int epoll_fd, int fd){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP|EPOLLERR;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    setNonBlocking(fd);
}

int sig_pipe[2];
void sig_handle(int sig){
    int old_errno = errno;
    send(sig_pipe[1], (char*) &sig, 1, 0);
    errno = old_errno;
}

void add_sig(int sig){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handle;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

int main(int argc, char** argv){
    if(argc<=2){
        printf("usage:%s ipaddr portnum\n",basename(argv[0]));
        return 1;
    }
    signal(SIGTERM, handle_term);
    const char * ip = argv[1];
    int port = atoi(argv[2]);
    int ret;
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = PF_INET;
    addr.sin_port = htons(port);
    ret = inet_aton(ip, &addr.sin_addr);
    assert(ret!=-1);

    int sock = socket(PF_INET,SOCK_STREAM,0);
    user_data *users = new user_data[MAX_FD];
    std::unordered_map<int, int> fd_index;

    ret = bind(sock,(sockaddr*) &addr, sizeof(addr));
    assert(ret!=-1);
    ret = listen(sock, 5);
    assert(ret!=-1);
    int epoll_fd = epoll_create(5);
    add_fd(epoll_fd, sock);
    add_fd(epoll_fd, sig_pipe[0]);

    add_sig(SIGTERM);
    add_sig(SIGINT);
    add_sig(SIGCHLD);
    signal(SIGPIPE, SIG_IGN);

    
    int user_count = 0;
    epoll_event events[MAX_EVENTS];
    while(!stop){
        ret = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if(ret<0){
            printf("epoll failed \n");
            break;
        }
        for(int i = 0; i < ret; i++){
            if(events[i].data.fd == sock && events[i].events & POLLIN){
                struct sockaddr_in user_addr;
                socklen_t sock_len = sizeof(user_addr);
                int connfd = accept(sock, (sockaddr *)&user_addr, &sock_len);
                if(connfd < 0){
                    printf("error is:%d\n", errno);
                    continue;
                }
                printf("get a call when users count is %d\n", user_count);
                if(user_count >= MAX_USER){
                    const char* msg = "too many users\n";
                    printf("%s", msg);
                    send(connfd, msg, strlen(msg), 0);
                    close(events[i].data.fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, 0);
                    continue;
                }
                fd_index[connfd] = user_count;
                users[user_count].user_addr = user_addr;
                user_count++;
                add_fd(epoll_fd, connfd);                
            }
            else if(events[i].data.fd == sig_pipe[0] && events[i].events & POLLIN){
                char sig[MAX_EVENTS];
                ret = recv(sig_pipe[0], &sig, sizeof(sig), -1);
                if(ret > 0){
                    for(int j = 0; j < ret; j++){
                        switch(sig[j]){
                            case SIGCHLD:
                            case SIGINT:
                            case SIGTERM:

                            default:
                                stop = true;
                                break;
                        }
                    }
                }

            }
            else if(events[i].events & POLLERR){
                printf("get an error from %d\n", events[i].data.fd);
                char error[100];
                socklen_t error_len = sizeof(error);
                if(getsockopt(events[i].data.fd, SOL_SOCKET, SO_ERROR, error, &error_len) < 0){
                    printf("get error message failed\n");
                }
                continue;
            }
            else if(events[i].events & POLLRDHUP){
                printf("user %d closed the connection\n", events[i].data.fd);
                close(events[i].data.fd);
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, 0);
                users[fd_index[events[i].data.fd]] = users[user_count];
                fd_index.erase(events[i].data.fd);
                user_count--;
            }
            else if(events[i].events & POLLIN){
                int connfd = events[i].data.fd;
                memset(users[connfd].read_buf,'\0', BUF_SIZE);
                while(1){
                    ret = recv(connfd, users[connfd].read_buf, BUF_SIZE-1, 0);
                    printf("get %d Bytes client data from %d \n", ret, connfd);
                    if(ret < 0){
                        if(errno == EAGAIN || errno == EWOULDBLOCK){
                            break;
                        }
                        close(events[i].data.fd);
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, 0);
                        users[fd_index[events[i].data.fd]] = users[user_count];
                        fd_index.erase(events[i].data.fd);
                        user_count--;
                    }
                    else if( ret == 0){
                        printf("user %d closed the connection\n", events[i].data.fd);
                        close(events[i].data.fd);
                        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, 0);
                        users[fd_index[events[i].data.fd]] = users[user_count];
                        fd_index.erase(events[i].data.fd);
                        user_count--;
                    }
                    else{                
                            printf("%s\n", users[connfd].read_buf);                       
                        }
                    }               

            }        
        }
    }
    close(sock);
    delete [] users;
    return 0;
}