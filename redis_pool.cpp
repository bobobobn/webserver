#include "redis_pool.h"


RedisPool* RedisPool::get_instance() {    
    static RedisPool instance;
    return &instance;
}

void RedisPool::init(int max_conn, const std::string& host, int port, const std::string& password) {
    max_conn_ = max_conn;
    host_ = host;
    port_ = port;
    password_ = password;
    for (int i = 0; i < max_conn_; i++) {
        RedisConn* conn = new RedisConn(host_.c_str(), port_, password_.c_str());
        pool_.push(conn);
    }
    sem_ = sem(max_conn_);
    printf("pool has been initialized with %d connections\n", pool_.size());
}

RedisConn* RedisPool::get_conn() {
    sem_.wait();
    locker_.lock();
    RedisConn* conn = pool_.front();
    pool_.pop();
    locker_.unlock();
    return conn;
}

void RedisPool::release_conn(RedisConn* conn) {
    locker_.lock();
    pool_.push(conn);
    locker_.unlock();
    sem_.post();
}

RedisPool::~RedisPool() {
    while (!pool_.empty()) {
        RedisConn* conn = pool_.front();
        pool_.pop();
        delete conn;
    }
}