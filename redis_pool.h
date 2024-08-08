#pragma once

#include "redis_conn.h"
#include "lock/locker.h"
#include "util.h"
#include <queue>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <sstream>  
#include <iomanip>  
#include <nlohmann/json.hpp>
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
    locker locker_;
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
    DISALLOW_COPY_MOVE_AND_ASSIGN(RedisConnRAII);
    RedisConn::ReplyPtr send_command(const char* format, ...) {
        va_list args;
        va_start(args, format);
        auto ret = conn_->send_command(format, args);
        va_end(args);
        return ret;
    }
    RedisConn::ReplyPtr set(const std::string& key, const std::string& value) {
        return conn_->set(key.c_str(), value.c_str());
    }
    RedisConn::ReplyPtr set(const std::string& key, const int& value) {
        return conn_->set(key.c_str(), value);
    }
    RedisConn::ReplyPtr get(const std::string& key) {
        return conn_->get(key.c_str());
    }
    RedisConn::ReplyPtr del(const std::string& key) {
        return conn_->del(key.c_str());
    }
    RedisConn::ReplyPtr setnx(const std::string& key, const std::string& value, int timeout) {
        return conn_->setnx(key.c_str(), value.c_str(), timeout);
    }
    RedisConn::ReplyPtr unlock(const std::string& key, const std::string& value) {
        return conn_->unlock(key.c_str(), value.c_str());
    }
    RedisConn::ReplyPtr decr_stock(const std::string& kill_id, int num, uint64_t order_id) {
        return conn_->decr_stock(kill_id.c_str(), num, order_id);
    }
    RedisConn::ReplyPtr xack(const std::string& stream_name, const std::string& group_name, const std::string& id) {
        return conn_->xack(stream_name.c_str(), group_name.c_str(), id.c_str());
    }

    // 每次只读一条消息
    nlohmann::json xreadgroup(const std::string& group_name, const std::string& consumer_name, const std::string& stream_name, const std::string& id, int timeout) {
        auto reply = conn_->xreadgroup(group_name.c_str(), consumer_name.c_str(), stream_name.c_str(), id.c_str(), timeout);
        if(reply == nullptr || reply->type == REDIS_REPLY_NIL){
            return {};
        }
        nlohmann::json ret;
        auto& element = reply->element[0];
        auto stream_id = element->element[0]->str;
        ret["stream_id"] = stream_id;
        auto& message_list = element->element[1];

        if(message_list->elements == 0){
            return {};
        }
        auto& message = element->element[1]->element[0];
        if(message->elements == 0){
            return {};
        }
        auto message_id = message->element[0]->str;
        auto message_data = message->element[1];
        for(int j = 0; j < message_data->elements; j+=2){
            auto key = message_data->element[j]->str;
            auto value = message_data->element[j+1]->str;
            ret[key] = value;
        }
        ret["message_id"] = message_id;
        return ret;
    }
    uint64_t get_unique_id(const std::string& prefix) {
        // 获取当前时间点
        auto now = std::chrono::system_clock::now();
        // 转换为time_t类型
        time_t now_c = std::chrono::system_clock::to_time_t(now);
        // 转换为32位时间戳
        uint32_t timestamp32 = static_cast<uint32_t>(now_c);
        // 将time_t转换为tm结构
        std::tm now_tm = *std::localtime(&now_c);

        // 格式化日期为字符串
        std::ostringstream oss;
        oss << std::put_time(&now_tm, "%Y-%m-%d");
        
        // 随机生成64位唯一ID
        uint64_t unique_id = (static_cast<uint64_t>(timestamp32) << 32 | static_cast<uint64_t>(get_incremental_id((prefix + oss.str()).c_str())));
        return unique_id;
    }
private:
    uint32_t get_incremental_id(const char* prefix) {
        return conn_->get_incremental_id(prefix);
    }
    RedisPool* pool_;
    RedisConn* conn_;
};

