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

#define MAX_USER 5
#define BUF_SIZE 64
#define MAX_FD 65535

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
    
    ret = bind(sock,(sockaddr*) &addr, sizeof(addr));
    assert(ret!=-1);
    ret = listen(sock, 5);
    assert(ret!=-1);

    int user_count = 0;
    pollfd fds[MAX_USER+1];
    fds[0].fd = sock;
    fds[0].events = POLL_IN | POLL_ERR;
    fds[0].revents = 0;

    while(!stop){
        ret = poll(fds, MAX_USER+1, -1);
        if(ret<0){
            printf("poll failed \n");
            break;
        }
        for(int i = 0; i <= user_count; i++){
            if(fds[i].fd == sock && fds[i].revents == POLLIN){
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
                    close(connfd);
                    continue;
                }
                user_count++;
                fds[user_count].fd = connfd;
                fds[user_count].events = POLLIN | POLLERR | POLLRDHUP;
                fds[user_count].revents = 0;
                
            }
            else if(fds[i].revents & POLLERR){
                printf("get an error from %d\n", fds[i].fd);
                char error[100];
                socklen_t error_len = sizeof(error);
                if(getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, error, &error_len) < 0){
                    printf("get error message failed\n");
                }
                continue;
            }
            else if(fds[i].revents & POLLRDHUP){
                printf("user %d closed the connection\n", fds[i].fd);
                close(fds[i].fd);
                fds[i] = fds[user_count];
                user_count--;
            }
            else if(fds[i].revents & POLLIN){
                int connfd = fds[i].fd;
                memset(users[connfd].read_buf,'\0', BUF_SIZE);
                ret = recv(fds[i].fd, users[connfd].read_buf, BUF_SIZE-1, 0);
                printf("get %d Bytes client data from %d \n", ret, fds[i].fd);
                if(ret < 0){
                    if(errno == EAGAIN){
                        close(fds[i].fd);
                        fds[i] = fds[user_count];
                        user_count--;
                    }
                }
                else if( ret == 0){}
                else{                
                    for(int j = 1; j <= user_count; j++){
                        if(fds[j].fd == connfd){
                            continue;
                        }
                        fds[j].events &= ~POLLIN;
                        fds[j].events |= POLLOUT;
                        users[fds[j].fd].write_buf = users[connfd].read_buf;                        
                    }
                }

            }
            else if(fds[i].revents & POLLOUT){
                int connfd = fds[i].fd;
                if(!users[connfd].write_buf){
                    continue;
                }
                send(connfd, users[connfd].write_buf, strlen(users[connfd].write_buf), 0);
                users[connfd].write_buf = nullptr;
                fds[i].events &= ~POLLOUT;
                fds[i].events |= POLLIN;
            }
        }
    }
    close(sock);
    delete [] users;
    return 0;
}