#include "redis_conn.h"
#include <fstream>
#include <vector>

std::string RedisConn::decr_stock_script;
std::string RedisConn::unlock_script;
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
    if(decr_stock_script.empty()){
        std::ifstream file("/home/bob/webserver/decr_stock.lua");
        std::string script((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        std::vector<const char *> args {"LOAD", script.c_str()};
        auto reply = send_command("SCRIPT", args);
        decr_stock_script = reply->str;
    }
    if(unlock_script.empty()){
        std::ifstream file("/home/bob/webserver/unlock.lua");
        std::string script((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        std::vector<const char *> args {"LOAD", script.c_str()};
        auto reply = send_command("SCRIPT", args);
        unlock_script = reply->str;
    }
}

RedisConn::~RedisConn() {
    redisFree(redis_context);
}
void DoNotFree(void* reply){
    return;
}
RedisConn::ReplyPtr RedisConn::send_command(const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    redisReply* reply = (redisReply*)redisvCommand(redis_context, format, ap);
    va_end(ap);
    return ReplyPtr(reply, freeReplyObject);
}

RedisConn::ReplyPtr RedisConn::send_command(const char* format, va_list& ap) {
    redisReply* reply = (redisReply*)redisvCommand(redis_context, format, ap);
    if (reply == NULL) {
        LOG_ERROR("redisCommand error: %s", redis_context->errstr);
        return {nullptr, nullptr};
    }
    return ReplyPtr(reply, freeReplyObject);
}
RedisConn::ReplyPtr RedisConn::send_command(const char* cmd, const std::vector<const char*>& args) {
    std::vector<const char*> argv;
    argv.push_back(cmd);
    for(auto arg : args){
        argv.push_back(arg);
    }
    redisReply* reply = (redisReply*)redisCommandArgv(redis_context, argv.size(), argv.data(), nullptr);
    if (reply == NULL) {
        LOG_ERROR("redisCommand error: %s", redis_context->errstr);
        return {nullptr, nullptr};
    }
    return ReplyPtr(reply, freeReplyObject);
}

RedisConn::ReplyPtr RedisConn::set(const char* key, const char* value) {
    return send_command("SET %s %s", key, value);
}

RedisConn::ReplyPtr RedisConn::set(const char* key, const int& value) {
    return send_command("SET %s %d", key, value);
}

RedisConn::ReplyPtr RedisConn::get(const char* key) {
    return send_command("GET", {key}); 
}

RedisConn::ReplyPtr RedisConn::del(const char* key) {
    return send_command("DEL %s", key);
}
RedisConn::ReplyPtr RedisConn::setnx(const char* key, const char* value, int timeout){
    return send_command("SETNX %s %s %d", key, value, timeout);
}
RedisConn::ReplyPtr RedisConn::unlock(const char* key, const char* value){
    std::vector<const char*> args;
    args.push_back(key);
    args.push_back(value);
    return evalsha(unlock_script.c_str(), 1, args);
}

RedisConn::ReplyPtr RedisConn::eval(const char* script, int numkeys, const std::vector<const char*>& args){
    std::vector<const char*> argv;
    argv.push_back("EVAL");
    argv.push_back(script);
    argv.push_back(std::to_string(numkeys).c_str());
    for(auto arg : args){
        argv.push_back(arg);
    }
    redisReply* reply = (redisReply*)redisCommandArgv(redis_context, argv.size(), argv.data(), nullptr);
    return ReplyPtr(reply, freeReplyObject);
}

RedisConn::ReplyPtr RedisConn::evalsha(const char* script_sha, int numkeys, const std::vector<const char*>& args){
    std::vector<const char*> argv;
    argv.push_back("EVALSHA");
    argv.push_back(script_sha);
    argv.push_back(std::to_string(numkeys).c_str());
    for(auto arg : args){
        argv.push_back(arg);
    }
    redisReply* reply = (redisReply*)redisCommandArgv(redis_context, argv.size(), argv.data(), nullptr);
    return ReplyPtr(reply, freeReplyObject);
}

RedisConn::ReplyPtr RedisConn::decr_stock(const char* kill_id, int amount, uint64_t order_id){
    std::vector<const char*> args;
    args.push_back(kill_id);
    std::string amount_str = std::to_string(amount);
    args.push_back(amount_str.c_str());
    std::string order_id_str = std::to_string(order_id);
    args.push_back(order_id_str.c_str());
    return evalsha(decr_stock_script.c_str(), 0, args);
}

uint32_t RedisConn::get_incremental_id(const char* prefix){
    
    auto reply = send_command("INCR incr:%s ", prefix);
    return static_cast<uint32_t>(reply->integer);
}
RedisConn::ReplyPtr RedisConn::xreadgroup(const char* group, const char* consumer, const char* stream, const char* id, int timeout){
    return send_command("XREADGROUP GROUP %s %s COUNT 1 BLOCK %d STREAMS %s %s ", group, consumer, timeout, stream, id);
}
RedisConn::ReplyPtr RedisConn::xack(const char* stream, const char* group, const char* id){
    return send_command("XACK %s %s %s", stream, group, id);
}