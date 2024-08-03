#pragma once

#include "redis_conn.h"
#include "lock/locker.h"
#include <queue>

class RedisPool {
public:
    static RedisPool* get_instance();
    void init(int max_conn, const std::string& host, int port, const std::string& password);
    ~RedisPool();

    RedisConn* get_conn();
    void release_conn(RedisConn* conn);

private:
    std::queue<RedisConn*> pool_;
    int max_conn_;
    std::string host_;
    int port_;
    std::string password_;
    sem sem_;
};


class RedisConnRAII{
public:
    RedisConnRAII(){
        pool_ = RedisPool::get_instance();
        conn_ = pool_->get_conn();
    }
    ~RedisConnRAII() {
        pool_->release_conn(conn_);
    }    
    RedisConn::ReplyPtr send_command(const char* format, ...) {
        va_list args;
        va_start(args, format);
        auto ret = conn_->send_command(format, args);
        va_end(args);
        return ret;
    }
private:
    RedisPool* pool_;
    RedisConn* conn_;
};

