#pragma once 
#include <hiredis/hiredis.h>
#include <memory>
#include "log.h"

class RedisConn {
public:
    using ReplyPtr = std::unique_ptr<redisReply, void(*)(void*)>;
    RedisConn(const char* ip, int port, const char* passwd);
    ~RedisConn();
    ReplyPtr send_command(const char* cmd);

private:
    redisContext* redis_context;
    const char* ip;
    int port;
};
