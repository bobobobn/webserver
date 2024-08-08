#ifndef CONNECTIONPOOL_H
#define CONNECTIONPOOL_H

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <mysql/mysql.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <list>
#include <pthread.h>
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include "lock/locker.h"
#include "log.h"
#include "util.h"

using std::string;

class connectionPool{
public:
    static connectionPool* get_instance();

    void DestroyPool();
    void init(string url, string User, string PassWord, string DBName, int Port, unsigned int MaxConn);
    MYSQL* getConn();
    bool releaseConn(MYSQL* conn);
    DISALLOW_COPY_MOVE_AND_ASSIGN(connectionPool);
private:
    std::list<MYSQL*> connList;
    string m_url;
    string m_port;
    string m_user;
    string m_password;
    string m_DBName;
    int freeConn;
    int m_max_conn;
    sem* m_sem;
    locker m_lock;

private:
    connectionPool(){}
    ~connectionPool();

};

class connectionRAII{

public:
	using ResPtr = std::unique_ptr<MYSQL_RES, void(*)(MYSQL_RES*)>;
    using Result = std::vector<std::vector<string>>;
    struct QueryResult{
        Result result;
        const char* error = nullptr;
        bool success = false;
    };
    connectionRAII(){
        poolRAII = connectionPool::get_instance();
        conRAII = poolRAII->getConn();
    }
	~connectionRAII(){
        poolRAII->releaseConn(conRAII);
    }
    QueryResult query(const string& sql){
        QueryResult result;
        if(mysql_query(conRAII, sql.c_str()) != 0){
            result.error = mysql_error(conRAII);
            LOG_ERROR("mysql_query error: %s", result.error);
            return result;
        }
        auto res = ResPtr(mysql_store_result(conRAII), mysql_free_result);
        result.error = mysql_error(conRAII);
        if(res == nullptr && strlen(result.error) != 0){
            LOG_ERROR("mysql_store_result error: %s", result.error);
            return result;
        }
        if( res != nullptr){
            result.result = fetch(res);
        }
        result.success = true;
        return result;
    }
    nlohmann::json select(const string& sql, const std::vector<string>& fields,const string& key){
        QueryResult result = query(sql);
        nlohmann::json j;
        if(!result.success){
            j["error"] = result.error;
            j["success"] = 0;
            return j;
        }
        j["success"] = 1;
        nlohmann::json data = nlohmann::json::array();
        for(const auto& row : result.result){
            nlohmann::json row_data;
            for(int i = 0; i < row.size(); i++){
                row_data[fields[i]] = row[i];
            }
            data.push_back(row_data);
        }
        j[key] = data;
        return j;
    }
    nlohmann::json update_or_delete(const string& sql){
        QueryResult result = query(sql);
        nlohmann::json j;
        if(!result.success){
            j["error"] = result.error;
            j["success"] = 0;
            return j;
        }
        j["success"] = 1;        
        return j;
    }
    nlohmann::json insert(const string& sql){
        QueryResult result = query(sql);
        nlohmann::json j;
        if(!result.success){
            j["error"] = result.error;
            j["success"] = 0;
            return j;
        }
        j["success"] = 1;        
        j["productId"] = mysql_insert_id(conRAII);
        return j;
    }

    QueryResult transaction(const std::vector<string>& sqls){
        QueryResult result;
        if(mysql_query(conRAII, "START TRANSACTION") != 0){
            result.error = mysql_error(conRAII);
            LOG_ERROR("mysql_query error: %s", result.error);
            return result;
        }
        for(const auto& sql : sqls){
            if(mysql_query(conRAII, sql.c_str()) != 0){
                result.error = mysql_error(conRAII);
                LOG_ERROR("mysql_query error: %s", result.error);
                mysql_query(conRAII, "ROLLBACK");
                return result;
            }
        }
        if(mysql_query(conRAII, "COMMIT") != 0){
            result.error = mysql_error(conRAII);
            LOG_ERROR("mysql_query error: %s", result.error);
            return result;
        }
        result.success = true;
        return result;
    }
    Result fetch(ResPtr& res){
        Result result;
        MYSQL_ROW row;
        while((row = mysql_fetch_row(res.get())) != nullptr){
            std::vector<string> row_data;
            for(int i = 0; i < mysql_num_fields(res.get()); i++){
                row_data.push_back(row[i]? row[i] : "");
            }
            result.push_back(row_data);
        }
        return result;
    }
    const char* error(){
        return mysql_error(conRAII);
    }
	MYSQL& operator*(){
        return *conRAII;
    }

	MYSQL* operator->(){
        return conRAII;
    }
private:
	MYSQL *conRAII;
	connectionPool *poolRAII;
};
#endif