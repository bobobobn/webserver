#include "webserver.h"
#include "mq_processor.h"
#include <stdlib.h>
#include <string>
#include <gperftools/profiler.h>
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
    Log::get_instance()->set_level(5);
    // MqProcessor processor;
    //监听
    server.eventListen();
    // ProfilerStart("test_capture.prof");
    //运行
    server.eventLoop();
    // ProfilerStop();
    return 0;
}