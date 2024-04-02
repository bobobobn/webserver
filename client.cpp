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
#include<poll.h>

#define BUF_SIZE 64

int main(int argc, char** argv){
    if(argc<=2){
        printf("usage:%s ipaddr portnum\n",basename(argv[0]));
        return 1;
    }
    // signal(SIGTERM, handle_term);
    const char * ip = argv[1];
    int port = atoi(argv[2]);
    int ret;
    struct sockaddr_in sock_addr;
    bzero(&sock_addr, sizeof(sockaddr_in));
    sock_addr.sin_family = PF_INET;
    ret = inet_aton(ip, &sock_addr.sin_addr);
    assert(ret!=-1);
    sock_addr.sin_port = htons(port);
    int sock_fd = socket(PF_INET, SOCK_STREAM, 0);
    assert(sock_fd>=0);
    ret = connect(sock_fd, (struct sockaddr *) &sock_addr, sizeof(sock_addr));
    assert(ret!=-1);

    pollfd fds[2];
    fds[0].fd = 0;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    fds[1].fd = sock_fd;
    fds[1].events = POLLIN | POLLRDHUP;
    fds[1].revents = 0;

    char read_buf[BUF_SIZE];
    int pipefd[2];
    ret = pipe(pipefd);
    assert(ret!=-1);

    while(1){
        ret = poll(fds, 2, -1);
        if(ret<0){
            printf("poll failed\n");
            break;
        }
        if(fds[0].revents & POLLIN){
            // memset(read_buf, '\0', sizeof(read_buf));
            // read(0, read_buf, BUF_SIZE);
            // send(sock_fd, read_buf, strlen(read_buf), 0);
            // fds[0].revents &= ~POLLIN;
            ret = splice(0, NULL, pipefd[0], NULL, 32768, SPLICE_F_MOVE | SPLICE_F_MORE);
            ret = splice(pipefd[1], NULL, sock_fd, NULL, 32768, SPLICE_F_MOVE | SPLICE_F_MORE);
        }
        else if(fds[1].revents & POLLRDHUP){
            printf("server closed the connection\n");
            fds[1].revents &= ~POLLRDHUP;
            break;
        }
        else if(fds[1].revents & POLLIN){
            memset(read_buf,'\0', BUF_SIZE);
            ret = recv(sock_fd, read_buf, BUF_SIZE-1, 0);
            printf("%d bytes: '%s'\n", ret, read_buf);
            fds[1].revents &= ~POLLIN;
        }

    }

    close(sock_fd);
    close(pipefd[0]);
    close(pipefd[1]);
    
}