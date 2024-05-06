# webserver
参考[qinguoyi/TinyWebServer](https://github.com/qinguoyi/TinyWebServer) 与 Linux高性能服务器编程，游双著.搭建了Linux下C++轻量级Web服务器.
* 使用线程池+非阻塞socket+epoll（ET）+Proactor事件处理的并发模型
* 使用有限状态机解析HTTP请求报文，支持解析GET和POST请求
* 访问mysql数据库实现web端用户注册、登录功能，可以请求服务器图片和视频文件
* 实现异步日志系统，记录服务器运行状态
* 经webbench压力测试，可同时处理10000个并发连接数据交换
  ![image](https://github.com/bobobobn/webserver/assets/145976151/e0406d89-19f3-4b48-9e36-87d51b490882)  
# 编译
g++ -pg -g -Wall main.cpp util.cpp httpConn.cpp webserver.cpp threadPool.h heapTimer.cpp connectionPool.cpp -L /usr/lib/mysql -l mysqlclient -o main
# 代码概述
* 线程同步类定义在lock/locker.h，包括互斥锁、信号量、条件变量
* 数据库连接池定义在connectionPool.h，单例模式的数据库连接池对象以及RAII机制的连接获取和释放
* 时间堆定时器定义在heapTimer.h
* http请求文本处理（状态转移）定义在httpConn.h
* 线程池定义在threadPool.h
* 单例模式异步日志定义在log.h
* 文件描述符操作和epoll事件操作定义在util.h
* 服务器主循环定义在webserver.h，包括服务器初始化、处理客户请求、处理信号事件、处理读写事件
