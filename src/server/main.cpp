#include "chatserver.hpp"
#include "chatservice.hpp"

#include <iostream>
#include <signal.h>
using namespace std;

//处理服务器ctrl+c结束后，重置user的状态信息
void resetHandler(int){
    ChatService::instance()->reset();
    exit(0);
}

int main(int argc, char *argv[]){

    if(argc < 3){
        cerr<<"command invalid! example: ./ChatClient 127.0.0.1 9190"<<endl;
        exit(-1);
    }
    signal(SIGINT, resetHandler);

    char *ip = argv[1];
    int port = atoi(argv[2]);

    EventLoop loop;
    InetAddress addr(ip, port);
    ChatServer server(&loop, addr, "ChatServer");

    server.start();
    loop.loop();
    return 0;
}