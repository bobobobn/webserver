#ifndef CONNECTIONPOOL_H
#define CONNECTIONPOOL_H

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include <string>
#include "lock/locker.h"
#include "log.h"

using std::string;

class connectionPool{
public:
    static connectionPool* get_instance();

    void DestroyPool();
    void init(string url, string User, string PassWord, string DBName, int Port, unsigned int MaxConn){
        freeConn = 0;
        m_url = url;
        m_user = User;
        m_password = PassWord;
        m_DBName = DBName;
        m_max_conn = MaxConn;
        for(int i = 0; i < MaxConn; i++){
            MYSQL* conn = nullptr;
            conn = mysql_init(nullptr);
            if(!conn){
                LOG_ERROR("%s", "an error occur when connect to the DB");
                exit(1);
            }
            mysql_real_connect(conn, url.c_str(), User.c_str(), PassWord.c_str(), DBName.c_str(), Port, NULL, 0);
            if(!conn){
                LOG_ERROR("%s", "an error occur when connect to the DB");
                exit(1);
            }
            connList.push_back(conn);
            freeConn++;
        }
        m_sem = new sem(freeConn);
    }
    MYSQL* getConn(){
        MYSQL* ret = nullptr;
        m_sem->wait();
        m_lock.lock();
        if(connList.empty()){
            m_lock.unlock();
            throw std::exception();
        }
        ret = connList.front();
        connList.pop_front();
        freeConn--;
        m_lock.unlock();
        return ret;
    }
    bool releaseConn(MYSQL* conn){
        if(!conn){
            LOG_ERROR("%s", "releasing a null MYSQL CONN");
            return false;
        }
        m_lock.lock();
        if(connList.size() >= m_max_conn){
            m_lock.unlock();
            throw std::exception();
        }
        freeConn++;
        m_lock.unlock();
        m_sem->post();
        return true;
    }

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
	connectionRAII(MYSQL **con, connectionPool *connPool){
        *con = connPool->getConn();	
        conRAII = *con;
        poolRAII = connPool;
    }
	~connectionRAII(){
        poolRAII->releaseConn(conRAII);
    }
	
private:
	MYSQL *conRAII;
	connectionPool *poolRAII;
};
#endif