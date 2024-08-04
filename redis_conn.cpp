#include "redis_conn.h"

RedisConn::RedisConn(const char* ip, int port, const char* passwd) : ip(ip), port(port) {
    redis_context = redisConnect(ip, port);
    if (redis_context == NULL || redis_context->err) {
        if (redis_context) {
            fprintf(stderr, "Connection error: %s\n", redis_context->errstr);
            redisFree(redis_context);
        }
        exit(1);
    }
    send_command(("AUTH " + std::string(passwd)).c_str()); 
}

RedisConn::~RedisConn() {
    redisFree(redis_context);
}

RedisConn::ReplyPtr RedisConn::send_command(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    auto reply = send_command(format, ap);
    va_end(ap);
    return reply;
}
RedisConn::ReplyPtr RedisConn::send_command(const char* format, va_list ap) {
    redisReply* reply = (redisReply*)redisvCommand(redis_context, format, ap);
    if (reply == NULL) {
        LOG_ERROR("redisCommand error: %s", redis_context->errstr);
        return {nullptr, nullptr};
    }
    return ReplyPtr(reply, freeReplyObject);
}

RedisConn::ReplyPtr RedisConn::set(const char* key, const char* value) {
    return send_command("SET %s %s", key, value);
}

RedisConn::ReplyPtr RedisConn::get(const char* key) {
    return send_command("GET %s", key); 
}

RedisConn::ReplyPtr RedisConn::del(const char* key) {
    return send_command("DEL %s", key);
}