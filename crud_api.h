#pragma once
#include <string>
#include <map>
#include <functional>
#include <nlohmann/json.hpp>
#include "connectionPool.h"
#include "log.h"
#include "redis_pool.h"

namespace crud_api {
    using ApiFun = std::function<std::string(char*, int)>;
    enum class Status{ Failed = 0, Success = 1 };
    std::string get_all_merchants_name(char* content, int content_size){
        // 先在redis中查询
        RedisConnRAII redis_conn;
        const char* redis_key = "all_merchants_name";
        auto reply = redis_conn.get(redis_key);
        // 若redis中没有，则从数据库中查询，并将结果存入redis
        if(reply == nullptr || reply->type == REDIS_REPLY_NIL){  
            auto mysql = connectionRAII();
            auto res = mysql.query("SELECT id, name FROM merchant_name");
            if( res.success ) {
                nlohmann::json result;
                result["success"] = Status::Success;
                nlohmann::json json_data = nlohmann::json::array();
                for(auto row : res.result){
                    nlohmann::json merchant = { {"id", std::string(row[0])}, {"name", row[1]} };
                    json_data.push_back(merchant);
                }
                result["merchants"] = json_data;
                std::string result_str = result.dump();
                redis_conn.set(redis_key, result_str.c_str());
                return result_str;
            }            
        }
        else{
            return reply->str;
        }
    }
    std::string get_merchant_products(char* content, int content_size){
        nlohmann::json json_data = nlohmann::json::parse(content);
        std::string merchant_name = json_data["merchantName"];
        // 先在redis中查询
        RedisConnRAII redis_conn;
        std::string redis_key = merchant_name + ":merchant_products";
        auto reply = redis_conn.get(redis_key.c_str());
        // 若redis中没有，则从数据库中查询，并将结果存入redis
        nlohmann::json result;
        auto mysql = connectionRAII();
        if(reply == nullptr || reply->type == REDIS_REPLY_NIL){  
            // 查询普通商品信息
            auto res = mysql.query("SELECT product_id, product_name, product_quantity FROM merchant_products \
            INNER JOIN merchant_name ON merchant_products.merchant_id = merchant_name.id \
            WHERE merchant_name.name = '"+merchant_name+"'");            
            if( res.success ) {
                nlohmann::json json_products = nlohmann::json::array();
                for(auto row : res.result){
                    std::string product_id = row[0];
                    std::string product_name = row[1];
                    int product_quantity = std::stoi(row[2]);
                    nlohmann::json product = { {"id", product_id}, {"name", product_name}, {"quantity", product_quantity} };
                    json_products.push_back(product);
                }
                result["products"] = json_products;
                redis_conn.set(redis_key, json_products.dump());
            }
            else {
                result["success"] = Status::Failed;
                return result.dump();
            }           
        } 
        else{
            result["products"] = nlohmann::json::parse(reply->str);
        }
        // 查询秒杀商品信息
        std::string redis_key_kill = merchant_name + ":merchant_kill_products";
        auto reply_kill = redis_conn.get(redis_key_kill);
        if(reply_kill == nullptr || reply_kill->type == REDIS_REPLY_NIL){
            // 查询秒杀商品信息
            auto res = mysql.query("SELECT kill_id, kill_name, kill_quantity, start_time, end_time FROM merchant_kill_products \
            INNER JOIN merchant_name ON merchant_kill_products.merchant_id = merchant_name.id \
            WHERE merchant_name.name = '"+merchant_name+"'");            
            if( res.success ) {
                nlohmann::json json_kill_products = nlohmann::json::array();
                for(auto row : res.result){
                    std::string kill_id = row[0];
                    std::string kill_name = row[1];
                    int kill_quantity = std::stoi(row[2]);
                    std::string start_time = row[3];
                    std::string end_time = row[4];
                    nlohmann::json kill_product = { {"id", kill_id}, {"name", kill_name}, {"quantity", kill_quantity}, {"startTime", start_time}, {"endTime", end_time} };
                    json_kill_products.push_back(kill_product);
                }
                result["kill_products"] = json_kill_products;
                redis_conn.set(redis_key_kill, json_kill_products.dump());
            }
            else {
                result["success"] = Status::Failed;
                result["error"] = res.error;                                                 
                return result.dump();
            }
            result["success"] = Status::Success;
            std::string result_str = result.dump();
            return result_str;
        }
        else{            
            result["success"] = Status::Success;
            result["kill_products"] = nlohmann::json::parse(reply_kill->str);
            std::string result_str = result.dump();
            return result_str;
        }
    }

    std::string edit_product(char* content, int content_size){
        nlohmann::json json_data = nlohmann::json::parse(content);
        std::string merchantName = json_data["merchantName"];
        std::string productId = json_data["productId"];
        std::string newName = json_data["newName"];
        std::string newQuantity = json_data["newQuantity"];
        // TODO: update product in database
        auto mysql = connectionRAII();
        auto res = mysql.query("UPDATE merchant_products SET product_name = '"+newName+"', product_quantity = "+newQuantity+" \
         WHERE product_id = '"+productId+"' AND merchant_id = (SELECT id FROM merchant_name WHERE name = '"+merchantName+"')");

        nlohmann::json result;
        if(res.success == 0) {
            result["success"] = Status::Success;
            // 删除redis缓存
            RedisConnRAII redis_conn;
            std::string redis_key = merchantName + ":merchant_products";
            redis_conn.del(redis_key);
        }
        else{
            result["success"] = Status::Failed;
            result["error"] = res.error;
        }
        std::string result_str = result.dump();
        return result_str;
    }

    std::string edit_kill_product(char* content, int content_size){
        nlohmann::json json_data = nlohmann::json::parse(content);
        std::string merchantName = json_data["merchantName"];
        std::string killId = json_data["killId"];
        std::string newName = json_data["newName"];
        std::string newQuantity = json_data["newQuantity"];
        // TODO: update product in database
        auto mysql = connectionRAII();
        auto res = mysql.query("UPDATE merchant_kill_products SET kill_name = '"+newName+"', kill_quantity = "+newQuantity+" \
         WHERE kill_id = '"+killId+"' AND merchant_id = (SELECT id FROM merchant_name WHERE name = '"+merchantName+"')");

        nlohmann::json result;
        if( res.success ) {
            result["success"] = Status::Success;
            // 删除redis缓存
            RedisConnRAII redis_conn;
            std::string redis_key = merchantName + ":merchant_kill_products";
            redis_conn.del(redis_key);
        }
        else{
            result["success"] = Status::Failed;
            result["error"] = res.error;
        }
        std::string result_str = result.dump();
        return result_str;
    }

    std::string delete_product(char* content, int content_size){
        nlohmann::json json_data = nlohmann::json::parse(content);
        std::string merchantName = json_data["merchantName"];
        std::string productId = json_data["productId"];
        // delete product in database
        auto mysql = connectionRAII();
        auto res = mysql.query("DELETE FROM merchant_products WHERE product_id = '"+productId+"' AND merchant_id = (SELECT id FROM merchant_name WHERE name = '"+merchantName+"')");
        nlohmann::json result;
        if( res.success ) {
            result["success"] = Status::Success;
            // 删除redis缓存
            RedisConnRAII redis_conn;
            std::string redis_key = merchantName + ":merchant_products";
            redis_conn.del(redis_key);
        }
        else{
            result["success"] = Status::Failed;
            result["error"] = res.error;
        }
        std::string result_str = result.dump();
        return result_str;
    }

    std::string delete_kill_product(char* content, int content_size){
        nlohmann::json json_data = nlohmann::json::parse(content);
        std::string merchantName = json_data["merchantName"];
        std::string killId = json_data["killId"];
        // TODO: delete product in database
        auto mysql = connectionRAII();
        auto res = mysql.query("DELETE FROM merchant_kill_products WHERE kill_id = '"+killId+"' AND merchant_id = (SELECT id FROM merchant_name WHERE name = '"+merchantName+"')");
        nlohmann::json result;
        if( res.success ) {
            result["success"] = Status::Success;
            // 删除redis缓存
            RedisConnRAII redis_conn;
            std::string redis_key = merchantName + ":merchant_kill_products";
            redis_conn.del(redis_key.c_str());
        }
        else{
            result["success"] = Status::Failed;
            result["error"] = res.error;
        }
        std::string result_str = result.dump();
        return result_str;
    }

    std::string add_product(char* content, int content_size){
        nlohmann::json json_data = nlohmann::json::parse(content);
        std::string merchantName = json_data["merchantName"];
        nlohmann::json product = json_data["product"];
        std::string productName = product["name"];  
        std::string productQuantity = product["quantity"];
        // TODO: add product to database
        auto mysql = connectionRAII();
        auto res = mysql.query("INSERT INTO merchant_products (product_id, product_name, product_quantity, merchant_id) \
         VALUES (null, '"+productName+"', "+productQuantity+", (SELECT id FROM merchant_name WHERE name = '"+merchantName+"'))");
        nlohmann::json result;
        if( res.success ) {
            result["success"] = Status::Success;
            result["productId"] = mysql_insert_id(&*mysql);
            // 删除redis缓存
            RedisConnRAII redis_conn;
            std::string redis_key = merchantName + ":merchant_products";
            redis_conn.del(redis_key.c_str());
        }
        else{
            result["success"] = Status::Failed;
            result["error"] = res.error;
        }
        std::string result_str = result.dump();
        return result_str;
    }

    std::string add_kill_product(char* content, int content_size){
        nlohmann::json json_data = nlohmann::json::parse(content);
        std::string merchantName = json_data["merchantName"];
        nlohmann::json product = json_data["product"];
        std::string productName = product["name"];  
        std::string productQuantity = product["quantity"];
        std::string startTime = product["startTime"];
        std::string endTime = product["endTime"];
        // TODO: add product to database
        auto mysql = connectionRAII();
        auto res = mysql.query("INSERT INTO merchant_kill_products (kill_id, kill_name, kill_quantity, merchant_id, start_time, end_time) \
         VALUES (null, '"+productName+"', "+productQuantity+", (SELECT id FROM merchant_name WHERE name = '"+merchantName+"'), '"+startTime+"', '"+endTime+"')");
        nlohmann::json result;
        if( res.success ) {
            result["success"] = Status::Success;
            result["productId"] = mysql_insert_id(&*mysql);
            // 删除redis缓存
            RedisConnRAII redis_conn;
            std::string redis_key = merchantName + ":merchant_kill_products";
            redis_conn.del(redis_key);
        }
        else{
            result["success"] = Status::Failed;
            result["error"] = res.error;
        }
        std::string result_str = result.dump();
        return result_str;
    }

    std::string orders_submit(char* content, int content_size){
        nlohmann::json json_data = nlohmann::json::parse(content);
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

    std::string make_kill(char* content, int content_size){
        nlohmann::json json_data = nlohmann::json::parse(content);
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
                query.emplace_back("UPDATE merchant_kill_products SET kill_quantity = kill_quantity - "+productQuantity+\
                " WHERE kill_id = "+productId+" AND kill_quantity >= "+productQuantity+"; ");
                // 插入订单详情
                query.emplace_back("INSERT INTO kill_products_of_orders (kill_id, order_id, kill_quantity) \
                 VALUES ("+productId+", "+std::string(result["orderId"])+", "+productQuantity+"); ");
            }
            res = mysql.transaction(query);
            if(res.success) {
                result["success"] = Status::Success;
                // 删除redis缓存
                RedisConnRAII redis_conn;
                std::string redis_key = merchantName + ":merchant_kill_products";
                redis_conn.del(redis_key);
            }
        }
        std::string result_str = result.dump();
        return result_str;
    }
    std::map<std::string, ApiFun> api_map = {
        { "/api/merchants/name", get_all_merchants_name },
        { "/api/merchants/products", get_merchant_products },
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