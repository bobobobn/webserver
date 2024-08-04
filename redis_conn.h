#pragma once 
#include <hiredis/hiredis.h>
#include <memory>
#include "log.h"

class RedisConn {
public:
    using ReplyPtr = std::unique_ptr<redisReply, void(*)(void*)>;
    RedisConn(const char* ip, int port, const char* passwd);
    ~RedisConn();
    ReplyPtr set(const char* key, const char* value);
    ReplyPtr get(const char* key);
    ReplyPtr del(const char* key);
    ReplyPtr smembers(const char* key);
    ReplyPtr sadd(const char* key, const char* member);
    ReplyPtr srem(const char* key, const char* member); 
    ReplyPtr hset(const char* key, const char* field, const char* value);

    ReplyPtr hget(const char* key, const char* field);
    ReplyPtr hgetall(const char* key);
    ReplyPtr send_command(const char* format, ...);
    ReplyPtr send_command(const char* format, va_list ap);

private:
    redisContext* redis_context;
    const char* ip;
    int port;
};
