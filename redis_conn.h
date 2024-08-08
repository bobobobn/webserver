#pragma once 
#include <hiredis/hiredis.h>
#include <memory>
#include <vector>
#include "log.h"

class RedisConn {
public:
    using ReplyPtr = std::unique_ptr<redisReply, void(*)(void*)>;
    RedisConn(const char* ip, int port, const char* passwd);
    ~RedisConn();
    ReplyPtr set(const char* key, const char* value);
    ReplyPtr set(const char* key, const int& value);
    ReplyPtr get(const char* key);
    ReplyPtr del(const char* key);
    ReplyPtr setnx(const char* key, const char* value, int timeout);
    ReplyPtr unlock(const char* key, const char* value);
    ReplyPtr decr_stock(const char* key, int amount, uint64_t order_id);
    ReplyPtr xreadgroup(const char* group, const char* consumer, const char* stream, const char* id, int timeout);
    ReplyPtr xack(const char* stream, const char* group, const char* id);
    ReplyPtr smembers(const char* key);
    ReplyPtr sadd(const char* key, const char* member);
    ReplyPtr srem(const char* key, const char* member); 
    ReplyPtr hset(const char* key, const char* field, const char* value);
    ReplyPtr eval(const char* script, int numkeys, const std::vector<const char*>& args);
    ReplyPtr evalsha(const char* script_sha, int numkeys, const std::vector<const char*>& args);
    ReplyPtr hget(const char* key, const char* field);
    ReplyPtr hgetall(const char* key);
    ReplyPtr send_command(const char* format, ...);
    ReplyPtr send_command(const char* format, va_list& ap);
    ReplyPtr send_command(const char* cmd, const std::vector<const char*>& args);
    uint32_t get_incremental_id(const char* prefix);

private:
    redisContext* redis_context;
    static std::string decr_stock_script;
    static std::string unlock_script;
    const char* ip;
    int port;
};
