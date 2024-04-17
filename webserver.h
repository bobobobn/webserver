#ifndef WEBSERVER
#define WEBSERVER

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cassert>
#include <sys/epoll.h>
#include <cstring>
#include <string>
#include "util.h"
#include "threadPool.h"
#include "httpConn.h"
#include "heapTimer.h"
#include "log.h"
#include "connectionPool.h"
#include <string>
using std::string;

const int MAX_FD = 65536;           //最大文件描述符
const int MAX_EVENT_NUMBER = 10000; //最大事件数
const int TIMESLOT = 5;             //最小超时单位

class WebServer{
public:
    WebServer(int port, time_t delay, string user, string password, string dbName):m_port(port), m_delay(delay){
    users = new http_conn[MAX_FD];
    timers = new timer[MAX_FD];
    m_heap = new timerHeap(MAX_FD);
    Log::get_instance()->init("weblog", 8192, 10000, 1000);
    Log::get_instance()->write_log(0, "%s", "server start");

    m_connPool = connectionPool::get_instance();
    connectionPool::get_instance()->init("localhost", user, password, "websvDB", 3306, 100);
    users->initmysql_result(m_connPool);
    //root文件夹路径
    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path) + strlen(root) + 1);
    strcpy(m_root, server_path);
    strcat(m_root, root);
    m_pool = new threadPool<http_conn>(8, 10000);
    utils.sig_pipe = m_pipefd;
    m_OPT_LINGER = 1;
    }
    ~WebServer(){
        close(m_epollfd);
        close(m_listenfd);
        close(m_pipefd[1]);
        close(m_pipefd[0]);
        delete[] users;
        delete[] timers;
        delete[] m_root;
        delete m_heap;
        // delete[] users_timer;
        delete m_pool;
    };

    // void init(int port , string user, string passWord, string databaseName,
    //           int log_write , int opt_linger, int trigmode, int sql_num,
    //           int thread_num, int close_log, int actor_model);
    void eventLoop();
    void eventListen();
    bool dealclientdata();
    bool dealwithsignal(bool& stop_server, bool& time_out);
    void dealwithread(int sockfd);
    void dealwithwrite(int sockfd);
    void sql_pool();
    void thread_pool();

public:
    connectionPool *m_connPool;
    //基础
    int m_port;
    char *m_root;
    int m_log_write;
    int m_close_log;
    int m_actormodel;

    int m_pipefd[2];
    int m_epollfd;
    http_conn *users;
    timer* timers;
    time_t m_delay;
    timerHeap* m_heap;

    // //数据库相关
    // string m_user;         //登陆数据库用户名
    // string m_passWord;     //登陆数据库密码
    // string m_databaseName; //使用数据库名
    // int m_sql_num;

    // //线程池相关
    threadPool<http_conn> *m_pool;
    int m_thread_num;

    //epoll_event相关
    epoll_event events[MAX_EVENT_NUMBER];

    int m_listenfd;
    int m_OPT_LINGER;
    int m_TRIGMode;
    int m_LISTENTrigmode;
    int m_CONNTrigmode;

    //定时器相关
    // client_data *users_timer;
    Utils utils;
};

#endif