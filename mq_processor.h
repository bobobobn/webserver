#pragma once
#include <pthread.h>
#include "redis_pool.h"
#include "connectionPool.h"

class MqProcessor {
public:
    MqProcessor();
    ~MqProcessor();
private:
    static void* thread_func(void* arg);
    pthread_t thread;
    bool running;
    RedisConnRAII redis_conn;
    connectionRAII mysql_conn;
};
MqProcessor::MqProcessor() {
    pthread_create(&thread, NULL, thread_func, this);
}

MqProcessor::~MqProcessor() {
    running = false;
    pthread_join(thread, NULL);
}
static connectionRAII::QueryResult make_kill_order_to_mysql(const std::string& productId, const std::string& productQuantity, const uint64_t& order_id){
        connectionRAII mysql;
        std::vector<std::string> query;
        // 减库存
        query.emplace_back("UPDATE merchant_kill_products SET kill_quantity = kill_quantity - "+productQuantity+\
        " WHERE kill_id = "+productId+" AND kill_quantity >= "+productQuantity+"; ");
        // 插入订单详情
        query.emplace_back("INSERT INTO kill_products_of_orders (kill_id, order_id, kill_quantity) \
            VALUES ("+productId+", "+std::to_string(order_id)+", "+productQuantity+"); ");
        auto res = mysql.transaction(query);
        return res;
}
void* MqProcessor::thread_func(void* arg) {
    MqProcessor* processor = (MqProcessor*)arg;
    processor->running = true;
    while (processor->running) {
        // 消费属于该消费者的pending消息
        auto json = processor->redis_conn.xreadgroup("mygroup", "consumer1", "order.stream", "0", 0);
        if(json.empty())
        {
            // 读取新消息
            json = processor->redis_conn.xreadgroup("mygroup", "consumer1", "order.stream", ">", 10000);     
            if(json.empty()){
                continue;
            }
        }
        // 1. 解析消息
        std::string kill_id = json["kill_id"].get<std::string>();
        std::string order_id = json["order_id"].get<std::string>();
        std::string kill_quantity = json["kill_quantity"].get<std::string>();
        // 2.写数据库
        auto res = make_kill_order_to_mysql(kill_id, kill_quantity, std::stoull(order_id));
        // 3.XACK
        if(res.success){
            static int success_count = 0;
            success_count++;
            processor->redis_conn.xack("order.stream", "mygroup", json["message_id"].get<std::string>());
        }
    }
}