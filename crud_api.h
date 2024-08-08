#pragma once
#include <string>
#include <map>
#include <functional>
#include <nlohmann/json.hpp>
#include "connectionPool.h"
#include "log.h"
#include "redis_pool.h"
#include "distributed_lock.h"
#include "util.h"

namespace crud_api {
    using ApiFun = std::function<std::string(char*, int)>;
    enum class Status{ Failed = 0, Success = 1 };
    
    static bool parse_json(const char* content, int content_size, nlohmann::json& json_data){
        try{
            json_data = nlohmann::json::parse(std::string(content, content_size));
            return true;
        }
        catch(const std::exception& e){
            LOG_ERROR(e.what());
            return false;
        }
    }

    std::string get_all_merchants_name(char* content, int content_size){
        // 先在redis中查询
        RedisConnRAII redis_conn;
        const char* redis_key = "all_merchants_name";
        auto reply = redis_conn.get(redis_key);
        // 若redis中没有，则从数据库中查询，并将结果存入redis
        while(reply == nullptr || reply->type == REDIS_REPLY_NIL){  
            DistributedLock lock(redis_key, 10000);
            if(lock.try_lock()){
                // 再次查询redis，防止其他线程已经查询并将结果存入redis
                reply = redis_conn.get(redis_key);
                if(reply == nullptr || reply->type == REDIS_REPLY_NIL){
                    auto mysql = connectionRAII();
                    auto res = mysql.select("SELECT id, name FROM merchant_name", {"id", "name"} , "merchants");
                    std::string result = res.dump();
                    if(res["success"].dump() == "1"){
                        redis_conn.set(redis_key, result);
                    }
                    return result;
                }
                else{
                    return reply->str;
                }
            }
            else{
                // 等待0.1秒
                usleep(100000);
                reply = redis_conn.get(redis_key);
            }            
        }
        return reply->str;
    }
    std::string get_merchant_products(char* content, int content_size){
        nlohmann::json json_data;
        if(!parse_json(content, content_size, json_data)){
            return "Invalid request";
        }
        std::string merchant_name = json_data["merchantName"];
        // 先在redis中查询
        RedisConnRAII redis_conn;
        std::string redis_key = merchant_name + ":merchant_products";
        auto reply = redis_conn.get(redis_key);
        // 若redis中没有，则从数据库中查询，并将结果存入redis
        auto mysql = connectionRAII();
        if(reply == nullptr || reply->type == REDIS_REPLY_NIL){  
            // 查询普通商品信息
            auto res = mysql.select("SELECT product_id, product_name, product_quantity FROM merchant_products \
            INNER JOIN merchant_name ON merchant_products.merchant_id = merchant_name.id \
            WHERE merchant_name.name = '"+merchant_name+"'", {"product_id", "product_name", "product_quantity"} , "products");           

            std::string result = res.dump();
            if(res["success"].dump() == "1"){
                redis_conn.set(redis_key, result);
            }
            return result;
        } 
        else{
            return reply->str;
        }
    }

        // 查询秒杀商品信息
    std::string get_merchant_kill_products(char* content, int content_size){
        nlohmann::json json_data;
        if(!parse_json(content, content_size, json_data)){
            return "Invalid request";
        }
        std::string merchant_name = json_data["merchantName"];
        std::string redis_key_kill = merchant_name + ":merchant_kill_products";
        RedisConnRAII redis_conn;
        connectionRAII mysql;
        auto reply_kill = redis_conn.get(redis_key_kill);
        if(reply_kill == nullptr || reply_kill->type == REDIS_REPLY_NIL){
            // 查询秒杀商品信息
            auto res = mysql.select("SELECT kill_id, kill_name, kill_quantity, start_time, end_time FROM merchant_kill_products \
            INNER JOIN merchant_name ON merchant_kill_products.merchant_id = merchant_name.id \
            WHERE merchant_name.name = '"+merchant_name+"'", {"kill_id", "kill_name", "kill_quantity", "start_time", "end_time"} , "kill_products");            
            std::string result = res.dump();
            if(res["success"].dump() == "1"){
                redis_conn.set(redis_key_kill, result);
            }
            return result;
        }
        else{            
            return reply_kill->str;
        }
    }

    std::string edit_product(char* content, int content_size){
        nlohmann::json json_data;
        if(!parse_json(content, content_size, json_data)){
            return "Invalid request";
        }
        std::string merchantName = json_data["merchantName"];
        std::string productId = json_data["productId"];
        std::string newName = json_data["newName"];
        std::string newQuantity = json_data["newQuantity"];
        // TODO: update product in database
        auto mysql = connectionRAII();
        nlohmann::json result = mysql.update_or_delete("UPDATE merchant_products SET product_name = '"+newName+"', product_quantity = "+newQuantity+" \
         WHERE product_id = '"+productId+"' AND merchant_id = (SELECT id FROM merchant_name WHERE name = '"+merchantName+"')");
        std::string result_str = result.dump();
        if( result["success"].dump() == "1" ){
            // 删除redis缓存
            RedisConnRAII redis_conn;
            std::string redis_key = merchantName + ":merchant_products";
            redis_conn.del(redis_key);
        }
        return result_str;
    }

    std::string edit_kill_product(char* content, int content_size){
        nlohmann::json json_data;
        if(!parse_json(content, content_size, json_data)){
            return "Invalid request";
        }
        std::string merchantName = json_data["merchantName"];
        std::string killId = json_data["killId"];
        std::string newName = json_data["newName"];
        std::string newQuantity = json_data["newQuantity"];
        // TODO: update product in database
        auto mysql = connectionRAII();
        nlohmann::json result = mysql.update_or_delete("UPDATE merchant_kill_products SET kill_name = '"+newName+"', kill_quantity = "+newQuantity+" \
         WHERE kill_id = '"+killId+"' AND merchant_id = (SELECT id FROM merchant_name WHERE name = '"+merchantName+"')");
        std::string result_str = result.dump();
        if( result["success"].dump() == "1" ){
            // 删除redis缓存
            RedisConnRAII redis_conn;
            std::string redis_key = merchantName + ":merchant_kill_products";
            redis_conn.del(redis_key);
        }
        return result_str;
    }

    std::string delete_product(char* content, int content_size){
        nlohmann::json json_data;
        if(!parse_json(content, content_size, json_data)){
            return "Invalid request";
        }
        std::string merchantName = json_data["merchantName"];
        std::string productId = json_data["productId"];
        // delete product in database
        auto mysql = connectionRAII();
        nlohmann::json result = mysql.update_or_delete("DELETE FROM merchant_products WHERE product_id = '"+productId+"' AND merchant_id = (SELECT id FROM merchant_name WHERE name = '"+merchantName+"')");
        std::string result_str = result.dump();
        if( result["success"].dump() == "1" ){
            // 删除redis缓存
            RedisConnRAII redis_conn;
            std::string redis_key = merchantName + ":merchant_products";
            redis_conn.del(redis_key.c_str());
        }
        return result_str;
    }

    std::string delete_kill_product(char* content, int content_size){
        nlohmann::json json_data;
        if(!parse_json(content, content_size, json_data)){
            return "Invalid request";
        }
        std::string merchantName = json_data["merchantName"];
        std::string killId = json_data["killId"];
        // TODO: delete product in database
        auto mysql = connectionRAII();
        nlohmann::json result = mysql.update_or_delete("DELETE FROM merchant_kill_products WHERE kill_id = '"+killId+"' AND merchant_id = (SELECT id FROM merchant_name WHERE name = '"+merchantName+"')");
        std::string result_str = result.dump();
        if( result["success"].dump() == "1" ){
            // 删除redis缓存
            RedisConnRAII redis_conn;
            std::string redis_key = merchantName + ":merchant_kill_products";
            redis_conn.del(redis_key.c_str());
        }
        return result_str;
    }

    std::string add_product(char* content, int content_size){
        nlohmann::json json_data;
        if(!parse_json(content, content_size, json_data)){
            return "Invalid request";
        }
        std::string merchantName = json_data["merchantName"];
        nlohmann::json product = json_data["product"];
        std::string productName = product["product_name"];  
        std::string productQuantity = product["product_quantity"];
        // TODO: add product to database
        auto mysql = connectionRAII();
        nlohmann::json result = mysql.insert("INSERT INTO merchant_products (product_id, product_name, product_quantity, merchant_id) \
         VALUES (null, '"+productName+"', "+productQuantity+", (SELECT id FROM merchant_name WHERE name = '"+merchantName+"'))");

        std::string result_str = result.dump();
        if( result["success"].dump() == "1" ){
            // 删除redis缓存
            RedisConnRAII redis_conn;
            std::string redis_key = merchantName + ":merchant_products";
            redis_conn.del(redis_key.c_str());
        }
        return result_str;
    }

    std::string add_kill_product(char* content, int content_size){
        nlohmann::json json_data;
        if(!parse_json(content, content_size, json_data)){
            return "Invalid request";
        }
        std::string merchantName = json_data["merchantName"];
        nlohmann::json product = json_data["product"];
        std::string productName = product["kill_name"];  
        std::string productQuantity = product["kill_quantity"];
        std::string startTime = product["start_time"];
        std::string endTime = product["end_time"];
        // TODO: add product to database
        auto mysql = connectionRAII();
        nlohmann::json result = mysql.insert("INSERT INTO merchant_kill_products (kill_id, kill_name, kill_quantity, merchant_id, start_time, end_time) \
         VALUES (null, '"+productName+"', "+productQuantity+", (SELECT id FROM merchant_name WHERE name = '"+merchantName+"'), '"+startTime+"', '"+endTime+"')");
        std::string result_str = result.dump();
        if( result["success"].dump() == "1" ){
            // 删除redis缓存
            RedisConnRAII redis_conn;
            std::string redis_key = merchantName + ":merchant_kill_products";
            redis_conn.del(redis_key.c_str());            
        }
        return result_str;
    }

    std::string orders_submit(char* content, int content_size){
        nlohmann::json json_data;
        if(!parse_json(content, content_size, json_data)){
            return "Invalid request";
        }
        std::string merchantName = json_data["merchantName"];
        nlohmann::json products = json_data["products"];
        std::string deliveryAddress = json_data["deliveryAddress"];
        // TODO: add order to database
        auto mysql = connectionRAII();
        auto res = mysql.query("INSERT INTO merchant_orders (order_id, delivery_address, order_time, id) \
         VALUES (null, '"+deliveryAddress+"', CURRENT_TIMESTAMP(), (SELECT id FROM merchant_name WHERE name = '"+merchantName+"'))");
        nlohmann::json result;
        std::vector<std::string> transactions;
        result["success"] = Status::Failed;
        if( res.success ) {
            std::vector<std::string> query;
            result["orderId"] = std::to_string(mysql_insert_id(&*mysql));
            for(auto& product : products){
                std::string productId = product["id"];
                std::string productQuantity = product["quantity"];
                // 减库存
                query.emplace_back("UPDATE merchant_products SET product_quantity = product_quantity - "+productQuantity+\
                " WHERE product_id = "+productId+" AND product_quantity >= "+productQuantity+"; ");
                // 插入订单详情
                query.emplace_back("INSERT INTO products_of_orders (order_id, product_id, product_quantity) \
                 VALUES ("+std::string(result["orderId"])+", "+productId+", "+productQuantity+"); ");
            }
            res = mysql.transaction(query);
            if(res.success) {
                result["success"] = Status::Success;
                // 删除redis缓存
                RedisConnRAII redis_conn;
                std::string redis_key = merchantName + ":merchant_products";
                redis_conn.del(redis_key.c_str());
            }
        }
        std::string result_str = result.dump();
        return result_str;
    }
    static bool check_kill_product_quantity(std::string productId, std::string productQuantity){
        auto redis_conn = RedisConnRAII();
        // 先在redis中查询
        std::string redis_key = "kill:stock:" + productId;
        auto reply = redis_conn.get(redis_key);
        // 若redis中没有，则从数据库中查询，并将结果存入redis
        while(reply == nullptr || reply->type == REDIS_REPLY_NIL){
            auto mysql = connectionRAII();
            DistributedLock lock(redis_key+":lock", 10000);
            if(lock.try_lock()){
                // 再次查询redis，防止其他线程已经查询并将结果存入redis
                reply = redis_conn.get(redis_key);
                if(reply == nullptr || reply->type == REDIS_REPLY_NIL){
                    auto res = mysql.select("SELECT kill_quantity FROM merchant_kill_products WHERE kill_id = "+productId, {"kill_quantity"} , "result");
                    if(res["success"].get<int>() == 1){
                        std::string quantity = res["result"][0]["kill_quantity"].get<std::string>();
                        int kill_quantity = std::stoi(quantity);
                        redis_conn.set(redis_key, kill_quantity);
                        return kill_quantity >= std::stoi(productQuantity);
                    }
                    else{
                        return false;
                    }
                }
                else{
                    return true;
                }
            }
            else{
                // 等待0.1秒
                usleep(100000);
                reply = redis_conn.get(redis_key);
            }
        }
        return true;
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
    std::string make_kill(char* content, int content_size){
        nlohmann::json json_data;
        if(!parse_json(content, content_size, json_data)){
            nlohmann::json result;
            result["success"] = Status::Failed;
            result["message"] = "Invalid request";
            return result.dump();
        }
        std::string merchantName = json_data["merchantName"];
        nlohmann::json products = json_data["products"];
        std::string deliveryAddress = json_data["deliveryAddress"];
        // 1.查询秒杀商品信息，判断是否有库存
        
        RedisConnRAII redis_conn;
        // 2.减库存，生成订单id，插入订单详情到消息队列
        // 2.1 生成订单id
        uint64_t order_id = redis_conn.get_unique_id("prefix");
        // 2.2 减库存 + 插入订单详情到消息队列
        std::string redis_key = "kill:stock:" + products[0]["id"].get<std::string>();
        // auto start_time = gettimeofday_us();
        auto reply = redis_conn.decr_stock(products[0]["id"].get<std::string>(), std::stoi(products[0]["quantity"].get<std::string>()), order_id);
        // auto end_time = gettimeofday_us();
        // printf("decr_stock cost %lu us\n", (end_time - start_time));
        if (reply->integer == -3)
        {
            // 库存缓存不存在
            // 1.1 读入库存缓存
            if(!check_kill_product_quantity(products[0]["id"], products[0]["quantity"])){
                nlohmann::json result;
                result["success"] = Status::Failed;
                result["message"] = "Not enough stock";
                return result.dump();
            }
            reply = redis_conn.decr_stock(products[0]["id"].get<std::string>(), std::stoi(products[0]["quantity"].get<std::string>()), order_id);
        }
        
        if(reply->integer == -1){
            // 库存不足
            nlohmann::json result;
            result["success"] = Status::Failed;
            result["message"] = "Not enough stock";
            return result.dump();
        }
        else if(reply->integer == -2){
            // 订单已存在
            nlohmann::json result;
            result["success"] = Status::Failed;
            result["message"] = "Order already exists";
            return result.dump();
        }

        // // 库存足够，插入订单详情到mysql
        json_data["orderId"] = std::to_string(order_id);
        std::string message = json_data.dump();
        // todo: 处理订单详情
        auto res = make_kill_order_to_mysql(products[0]["id"].get<std::string>(), products[0]["quantity"].get<std::string>(), order_id);
        if(!res.success){
            // 订单详情插入失败
            return "Order detail insert failed";
        }
        // 3.返回订单id
        nlohmann::json result;
        result["success"] = Status::Success;
        result["orderId"] = std::to_string(order_id);
        return result.dump();
    }
    std::map<std::string, ApiFun> api_map = {
        { "/api/merchants/name", get_all_merchants_name },
        { "/api/merchants/products/get", get_merchant_products },
        { "/api/merchants/products/getKill", get_merchant_kill_products },
        { "/api/merchants/products/edit", edit_product },
        { "/api/merchants/products/editKill", edit_kill_product },
        { "/api/merchants/products/delete", delete_product },
        { "/api/merchants/products/deleteKill", delete_kill_product },
        { "/api/merchants/products/add", add_product },
        { "/api/merchants/products/addKill", add_kill_product },
        { "/api/orders/submit", orders_submit },
        { "/api/orders/makeKill", make_kill },
    };
}