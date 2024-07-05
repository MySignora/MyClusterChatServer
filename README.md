# MyClusterChatServer
Nginx Tcp负载均衡环境中的集群聊天服务器和客户端源码 基于muduo库 redis mysql

编译方式
cd build
rm -rf *
cmake ..
make

需要启动nginx(cd /usr/local/nginx/sbin ./nginx)、redis-server

cd ../bin
./ChatServer ip port
./ChatServer ip port（使用了nginx，通用8000，或者指定6000/6002...）
