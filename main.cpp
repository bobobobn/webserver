#include "webserver.h"
#include <stdlib.h>


int main(int argc, char *argv[])
{
    if(argc<=1){
        printf("usage:%s portnum\n",basename(argv[0]));
        return 1;
    }
    int port = atoi(argv[1]);

    WebServer server(port);

    //监听
    server.eventListen();

    //运行
    server.eventLoop();

    return 0;
}