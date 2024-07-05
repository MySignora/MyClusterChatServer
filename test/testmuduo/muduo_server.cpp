// /*
// muduo给用户提供了两个主要类
// Tcpserver：服务器程序
// Tcpclient：客户端程序
// */

// #include <muduo/net/TcpServer.h>
// #include <muduo/net/EventLoop.h>
// #include <iostream>
// #include <functional>
// #include <string>
// using namespace std;

// //基于muduo开发服务器程序
// //1.组合TcpServer对象
// //2.创建EventLoop事件循环对象的指针
// //3.明确TcpServer构造函数需要什么参数
// //4.在当前服务器类的构造函数中，注册处理连接和处理时间的回调函数
// //5.设置合适的服务端线程数量，muduo自动划分I/O线程和worker线程
// class ChatServer{
// public:
//     ChatServer(muduo::net::EventLoop *loop, const muduo::net::InetAddress& listenAddr, const string& nameArg)
//         :_server(loop, listenAddr, nameArg), _loop(loop){
//             //给服务器注册用户连接的创建和断开回调
//             _server.setConnectionCallback(std::bind(&ChatServer::onConnection, this, placeholders::_1));
//             //给服务器注册用户读写事件回调
//             _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, placeholders::_1, placeholders::_2, placeholders::_3));
//             //设置服务器端的线程数量
//             _server.setThreadNum(4);
//     }
//     //开启时间循环
//     void start(){
//         _server.start();
//     }
// private:
//     //专门处理用户的连接创建和断开
//     void onConnection(const muduo::net::TcpConnectionPtr& conn){
//         if(conn->connected()){
//             cout << conn->peerAddress().toIpPort()<< "->" <<
//                     conn->localAddress().toIpPort() << "state:online" << endl;
//         }else{
//             cout << conn->peerAddress().toIpPort()<< "->" <<
//                     conn->localAddress().toIpPort() << "state:offline" << endl;
//             conn->shutdown();//close(fd)
//             // _loop->quit();
//         }
        
//     }
//     //专门处理用户的读写事件
//     void onMessage(const muduo::net::TcpConnectionPtr& conn,//连接
//                     muduo::net::Buffer *buffer,//缓冲区
//                     muduo::Timestamp time)//接收数据的时间信息
//     {
//         string buf = buffer->retrieveAllAsString();
//         cout << " recv data:" << buf << " time:" << time.toString() << endl;
//         conn->send(buf);
//     }

//     muduo::net::TcpServer _server;
//     muduo::net::EventLoop *_loop;
// };

// int main(){
//     muduo::net::EventLoop loop;//epoll
//     muduo::net::InetAddress addr("127.0.0.1", 9190);
//     ChatServer server(&loop, addr, "ChatServer");

//     server.start();
//     loop.loop();//epoll_wait以阻塞的方式等待 新用户连接，已连接用户的读写事件

//     return 0;
// }