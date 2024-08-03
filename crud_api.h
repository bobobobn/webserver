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
        auto reply = redis_conn.send_command("GET %s", redis_key);
        // 若redis中没有，则从数据库中查询，并将结果存入redis
        if(reply == nullptr || reply->type == REDIS_REPLY_NIL){  
            auto mysql = connectionRAII();
            mysql_query(&*mysql, "SELECT id, name FROM merchant_name");
            LOG_ERROR("SELECT error:%s\n", mysql_error(&*mysql));
            MYSQL_RES* res = mysql_store_result(&*mysql);
            int num_rows = mysql_num_rows(res);
            MYSQL_ROW row;
            nlohmann::json result;        
            result["success"] = Status::Success;
            nlohmann::json json_data = nlohmann::json::array();
            while ((row = mysql_fetch_row(res))) {
                std::string name = row[1];
                nlohmann::json merchant = { {"id", std::string(row[0])}, {"name", name} };
                json_data.push_back(merchant);
            }
            mysql_free_result(res);
            result["merchants"] = json_data;
            std::string result_str = result.dump();
            redis_conn.send_command("SET %s %s", redis_key, result_str.c_str());
            return result_str;
        }
        else{
            return reply->str;
        }
    }
    std::string get_merchant_products(char* content, int content_size){
        nlohmann::json json_data = nlohmann::json::parse(content);
        std::string merchant_name = json_data["merchantName"];
        printf("merchant_name: %s\n", merchant_name.c_str());
        // 先在redis中查询
        RedisConnRAII redis_conn;
        std::string redis_key = merchant_name + ":merchant_products";
        auto reply = redis_conn.send_command("GET %s", redis_key.c_str());
        // 若redis中没有，则从数据库中查询，并将结果存入redis
        if(reply == nullptr || reply->type == REDIS_REPLY_NIL){  
            auto mysql = connectionRAII();
            mysql_query(&*mysql, 
            ("SELECT product_id, product_name, product_quantity FROM merchant_products \
            INNER JOIN merchant_name ON merchant_products.merchant_id = merchant_name.id \
            WHERE merchant_name.name = '"+merchant_name+"'").c_str());
            const char* query_info = mysql_error(&*mysql);
            LOG_ERROR("SELECT error:%s\n", query_info);
            MYSQL_RES* res = mysql_store_result(&*mysql);
            int num_rows = mysql_num_rows(res);
            MYSQL_ROW row;
            nlohmann::json result;
            result["success"] = Status::Success;
            nlohmann::json json_products = nlohmann::json::array();
            while ((row = mysql_fetch_row(res))) {
                std::string product_id = row[0];
                std::string product_name = row[1];
                int product_quantity = std::stoi(row[2]);
                nlohmann::json product = { {"id", product_id}, {"name", product_name}, {"quantity", product_quantity} };
                json_products.push_back(product);
            }
            mysql_free_result(res);
            result["products"] = json_products;
            std::string result_str = result.dump();
            redis_conn.send_command("SET %s %s", redis_key.c_str(), result_str.c_str());
            return result_str;
        }
        else{
            return reply->str;
        }
    }

    std::string edit_product(char* content, int content_size){
        nlohmann::json json_data = nlohmann::json::parse(content);
        std::string merchantName = json_data["merchantName"];
        std::string productId = json_data["productId"];
        std::string newName = json_data["newName"];
        std::string newQuantity = json_data["newQuantity"];
        LOG_INFO("edit_product: merchantName: %s, productId: %s, newName: %s, newQuantity: %s\n", merchantName.c_str(), productId.c_str(), newName.c_str(), newQuantity.c_str());
        // TODO: update product in database
        auto mysql = connectionRAII();
        mysql_query(&*mysql, 
        ("UPDATE merchant_products SET product_name = '"+newName+"', product_quantity = "+newQuantity+" \
         WHERE product_id = '"+productId+"' AND merchant_id = (SELECT id FROM merchant_name WHERE name = '"+merchantName+"')").c_str());

        const char* query_info = mysql_error(&*mysql);
        LOG_ERROR("UPDATE error:%s\n", query_info);
        nlohmann::json result;
        if(strlen(query_info) == 0) {
            result["success"] = Status::Success;
            // 删除redis缓存
            RedisConnRAII redis_conn;
            std::string redis_key = merchantName + ":merchant_products";
            redis_conn.send_command("DEL %s", redis_key);
        }
        else{
            result["success"] = Status::Failed;
            result["error"] = query_info;
        }
        std::string result_str = result.dump();
        return result_str;
    }

    std::string delete_product(char* content, int content_size){
        nlohmann::json json_data = nlohmann::json::parse(content);
        std::string merchantName = json_data["merchantName"];
        std::string productId = json_data["productId"];
        // TODO: delete product in database
        auto mysql = connectionRAII();
        mysql_query(&*mysql, 
        ("DELETE FROM merchant_products WHERE product_id = '"+productId+"' AND merchant_id = (SELECT id FROM merchant_name WHERE name = '"+merchantName+"')").c_str());
        const char* query_info = mysql_error(&*mysql);
        LOG_ERROR("DELETE error:%s\n", query_info);
        nlohmann::json result;
        if(strlen(query_info) == 0) {
            result["success"] = Status::Success;
            // 删除redis缓存
            RedisConnRAII redis_conn;
            std::string redis_key = merchantName + ":merchant_products";
            redis_conn.send_command("DEL %s", redis_key.c_str());
        }
        else{
            result["success"] = Status::Failed;
            result["error"] = query_info;
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
        LOG_INFO("add_product: merchantName: %s, productName: %s, productQuantity: %s\n", merchantName.c_str(), productName.c_str(), productQuantity.c_str());
        // TODO: add product to database
        auto mysql = connectionRAII();
        mysql_query(&*mysql, 
        ("INSERT INTO merchant_products (product_id, product_name, product_quantity, merchant_id) \
         VALUES (null, '"+productName+"', "+productQuantity+", (SELECT id FROM merchant_name WHERE name = '"+merchantName+"'))").c_str());
        const char* query_info = mysql_error(&*mysql);
        LOG_ERROR("INSERT error:%s\n", query_info);
        nlohmann::json result;
        if(strlen(query_info) == 0) {
            result["success"] = Status::Success;
            result["productId"] = mysql_insert_id(&*mysql);
            // 删除redis缓存
            RedisConnRAII redis_conn;
            std::string redis_key = merchantName + ":merchant_products";
            redis_conn.send_command("DEL %s", redis_key.c_str());
        }
        else{
            result["success"] = Status::Failed;
            result["error"] = query_info;
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
        mysql_query(&*mysql, 
        ("INSERT INTO merchant_orders (order_id, delivery_address, order_time, id) \
         VALUES (null, '"+deliveryAddress+"', CURRENT_TIMESTAMP(), (SELECT id FROM merchant_name WHERE name = '"+merchantName+"'))").c_str());
        const char* query_info = mysql_error(&*mysql);
        LOG_ERROR("INSERT error:%s\n", query_info);
        nlohmann::json result;
        if(strlen(query_info) == 0) {
            std::vector<std::string> query = {"START TRANSACTION; "};
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
            query.emplace_back("COMMIT;");
            for(auto& q : query){
                mysql_query(&*mysql, q.c_str());
                query_info = mysql_error(&*mysql);
                if(strlen(query_info) != 0){
                    result["success"] = Status::Failed;
                    result["error"] = query_info;
                    break;
                }
            }
            result["success"] = Status::Success;
            // 删除redis缓存
            RedisConnRAII redis_conn;
            std::string redis_key = merchantName + ":merchant_products";
            redis_conn.send_command("DEL %s", redis_key);
        }
        else{
            result["success"] = Status::Failed;
        }
        std::string result_str = result.dump();
        return result_str;
    }

    std::map<std::string, ApiFun> api_map = {
        { "/api/merchants/name", get_all_merchants_name },
        { "/api/merchants/products", get_merchant_products },
        { "/api/merchants/products/edit", edit_product },
        { "/api/merchants/products/delete", delete_product },
        { "/api/merchants/products/add", add_product },
        { "/api/orders/submit", orders_submit }
    };
}