#include "webserver.h"



void WebServer::eventListen(){
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    if (0 == m_OPT_LINGER)
    {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }
    else if (1 == m_OPT_LINGER)
    {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    int ret = 0;
    sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(m_port);

    int flag = 1;
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    ret = bind(m_listenfd, (sockaddr *) &addr, sizeof(addr));
    assert(ret >= 0);
    ret = listen(m_listenfd, 5);
    assert(ret >= 0);

    // epoll_event events[MAX_EVENT_NUMBER];
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    setNonBlocking(m_pipefd[1]);
    addfd(m_epollfd, m_listenfd, false);
    utils.add_fd(m_epollfd, m_pipefd[0], false);
    utils.add_sig(SIGPIPE, SIG_IGN);
    utils.add_sig(SIGTERM, Utils::sig_handle);
    utils.add_sig(SIGALRM, Utils::sig_handle);

}

void WebServer::eventLoop(){
    bool stop = false;
    int ret = 0;
    bool time_out = false;
    while(!stop){
        ret = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        if(ret < 0 && errno != EINTR){            
            // LOG_ERROR("%s", "epoll failure");
            printf("epollfd:%d\n", m_epollfd);
            printf("err:%d\n", errno);
            printf("epoll failure\n");
            break;
        }
        for(int i = 0; i < ret; i++){
            {
            int sockfd = events[i].data.fd;

            //处理新到的客户连接
            printf("get a call\n");
            if (sockfd == m_listenfd)
            {
                bool flag = dealclientdata();
                if (false == flag)
                    continue;
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                //服务器端关闭连接，移除对应的定时器
                users[sockfd].close_conn(true);          
                m_heap->del_timer(timers+sockfd);
            }
            //处理信号
            else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN))
            {
                bool flag = dealwithsignal(stop, time_out);
                // if (false == flag)
                //     LOG_ERROR("%s", "dealclientdata failure");
            }
            //处理客户连接上接收到的数据
            else if (events[i].events & EPOLLIN)
            {   
                printf("get client request\n");
                dealwithread(sockfd);
            }
            else if (events[i].events & EPOLLOUT)
            {
                dealwithwrite(sockfd);
            }
        }
        }
        if(time_out){
            m_heap->tick();
            time_out = false;
        }
    }
}

bool WebServer::dealclientdata(){
    sockaddr_in user_addr;
    socklen_t len = sizeof(user_addr);
    while(1){
        int connfd = accept(m_listenfd, (sockaddr*) &user_addr, &len);
        if(connfd < 0){
            break;
        }
        if(http_conn::m_user_count >= MAX_FD){
            close(connfd);
            return false;
        }
        setNonBlocking(connfd);
        addfd(m_epollfd, connfd, true);
        users[connfd].init(connfd, user_addr, m_epollfd);
        timers[connfd].init(m_delay, users + connfd);
        if(m_heap->empty()){
            alarm(m_delay);
        }
        m_heap->add_timer(timers+connfd);
    }
    return true;
}

bool WebServer::dealwithsignal(bool &stop_server, bool& timeout)
{
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1)
    {
        return false;
    }
    else if (ret == 0)
    {
        return false;
    }
    else
    {
        for (int i = 0; i < ret; ++i)
        {
            switch (signals[i])
            {
            case SIGALRM:
            {
                timeout = true;
                break;
            }
            case SIGTERM:
            {
                stop_server = true;
                break;
            }
            }
        }
    }
    return true;
}

void WebServer::dealwithread(int sockfd){
    if(users[sockfd].read_once()){
        timers[sockfd].init(m_delay);
        m_pool->append(&users[sockfd]);
    }
    else
        printf("read_once failed\n");
}

void WebServer::dealwithwrite(int sockfd){
    users[sockfd].write();
}
