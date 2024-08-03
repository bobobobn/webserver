#include "webserver.h"
#include <stdlib.h>
#include <string>
using std::string;


int main(int argc, char *argv[])
{
    if(argc<=1){
        printf("usage:%s portnum\n",basename(argv[0]));
        return 1;
    }
    int port = atoi(argv[1]);
    time_t delay = 15;
    string user = "root";
    string passwd = "root";
    string databasename = "websvDB";
    string redisIp = "127.0.0.1";
    int redisPort = 6379;
    string redisPasswd = "";
    WebServer server(port, delay, user, passwd, databasename, redisIp, redisPort, redisPasswd);
    //监听
    server.eventListen();

    //运行
    server.eventLoop();

    return 0;
}