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
#include "lock/locker.h"
#include "log.h"

using std::string;

class connectionPool{
public:
    static connectionPool* get_instance();

    void DestroyPool();
    void init(string url, string User, string PassWord, string DBName, int Port, unsigned int MaxConn);
    MYSQL* getConn();
    bool releaseConn(MYSQL* conn);

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