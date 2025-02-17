#include "json.hpp"
#include "user.hpp"
#include "group.hpp"
#include "public.hpp"

#include <iostream>
#include <thread>
#include <string>
#include <string.h>
#include <vector>
#include <chrono>
#include <ctime>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>

using namespace std;
using json = nlohmann::json;

//记录当前系统登录的用户信息
User g_currentUser;
//记录当前登录用户的好友列表信息
vector<User> g_currentUserFriendList;
//记录当前登录用户的群组列表信息
vector<Group> g_currentUserGroupList;
//显示当前登录成功用户的基本信息
void showCurrentUserData();
//控制主菜单程序
bool isMainMenuRunning = false;

//接收线程
void readTaskHandler(int clientfd);
//获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime();
//主聊天页面程序
void mainMenu(int clientfd);

//聊天客户端程序实现，main线程用作发送线程，子线程用作接收线程
int main(int argc, char **argv){
    if(argc < 3){
        cerr<<"command invalid! example: ./ChatClient 127.0.0.1 9190"<<endl;
        exit(-1);
    }

    //解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    int port = atoi(argv[2]);

    //创建client端的socket
    int clientfd = socket(PF_INET, SOCK_STREAM, 0);
    if(clientfd == -1){
        cerr<<"socket create error"<<endl;
        exit(-1);
    }

    //填写client需要连接的server信息ip+port
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server.sin_addr.s_addr);

    //client和server进行连接
    if(connect(clientfd, (struct sockaddr*)&server, sizeof(server)) == -1){
        cerr<<"connect server error"<<endl;
        exit(-1);
    }

    //main线程用于接收用户输入，负责发送数据
    for(;;){
        //显示首页菜单 登录、注册、退出
        cout<<"===================="<<endl;
        cout<<"1. login"<<endl;
        cout<<"2. register"<<endl;
        cout<<"3. quit"<<endl;
        cout<<"===================="<<endl;
        cout<<"choice:";
        int choice=0;
        cin>>choice;
        cin.get();//读取缓冲区残留的回车

        switch(choice){
            case 1:{//login业务
                int id = 0;
                char pwd[50] = {0};
                cout<<"userid:";
                cin>>id;
                cin.get();//读取缓冲区残留的回车
                cout<<"userpassword:";
                cin.getline(pwd, 50);

                json js;
                js["msgid"] = LOGIN_MSG;
                js["id"] = id;
                js["password"] = pwd;
                string request = js.dump();

                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                if(len == -1){
                    cerr<<"send login msg error:"<<request<<endl;
                }else{
                    char buffer[1024] = {0};
                    
                    len = recv(clientfd, buffer, 1024, 0);
                    if(len == -1){
                        cerr<<"recv login response error:"<<endl;
                    }else{
                        json responsejs = json::parse(buffer);
                        if(responsejs["errno"].get<int>() != 0){//登录失败
                            cerr<<responsejs["errmsg"]<<endl;
                        }else{
                            //记录当前用户的id和name
                            g_currentUser.setId(responsejs["id"].get<int>());
                            g_currentUser.setName(responsejs["name"]);
                            
                            //记录当前用户的好友列表信息
                            if(responsejs.contains("friends")){
                                //初始化
                                g_currentUserFriendList.clear();

                                vector<string> vec = responsejs["friends"];
                                for(int i=0;i<vec.size();i++){
                                    json js = json::parse(vec[i]);
                                    User user;
                                    user.setId(js["id"].get<int>());
                                    user.setName(js["name"]);
                                    user.setState(js["state"]);
                                    g_currentUserFriendList.push_back(user);
                                }
                            }

                            //记录当前用户的群组列表
                            if(responsejs.contains("groups")){
                                //初始化
                                g_currentUserGroupList.clear();

                                vector<string> vec1 = responsejs["groups"];
                                for(int i=0;i<vec1.size();i++){
                                    json grpjs = json::parse(vec1[i]);
                                    Group group;
                                    group.setId(grpjs["id"].get<int>());
                                    group.setName(grpjs["groupname"]);
                                    group.setDesc(grpjs["groupdesc"]);

                                    vector<string> vec2 = grpjs["users"];
                                    for(int j=0;j<vec2.size();j++){
                                        GroupUser user;
                                        json js = json::parse(vec2[j]);
                                        user.setId(js["id"].get<int>());
                                        user.setName(js["name"]);
                                        user.setState(js["state"]);
                                        user.setRole(js["role"]);
                                        group.getUsers().push_back(user);
                                    }

                                    g_currentUserGroupList.push_back(group);
                                }
                            }
                            //显示登录用户信息
                            showCurrentUserData();
                            //显示当前用户的离线消息 个人聊天消息或者群组消息
                            if(responsejs.contains("offlinemsg")){
                                vector<string> vec = responsejs["offlinemsg"];
                                cout << "===============offlinemsg=================" << endl;
                                for(int i=0;i<vec.size();i++){
                                    json js = json::parse(vec[i]);
                                    if(js["msgid"].get<int>() == ONE_CHAT_MSG){
                                        cout<<js["time"].get<string>()<<" ["<<js["id"]<<"]"<<js["name"].get<string>()<<" said: "<<js["msg"].get<string>()<<endl;
                                    } 
                                    if(js["msgid"].get<int>() == GROUP_CHAT_MSG){
                                        cout<<"群消息["<<js["groupid"]<<"]: "<<js["time"].get<string>()<<" ["<<js["id"]<<"]"<<js["name"].get<string>()<<" said: "<<js["msg"].get<string>()<<endl;
                                    }
                                }
                            }
                            //登录成功，启动接收线程负责接收数据
                            //启动一次就行
                            static int readthreadnumber = 0;
                            if(readthreadnumber == 0){
                                thread readTask(readTaskHandler, clientfd);
                                readTask.detach();
                                readthreadnumber++;
                            }

                            //进入聊天主菜单界面
                            isMainMenuRunning = true;
                            mainMenu(clientfd);
                        }
                    }
                }
                break;
            }
            case 2:{//register业务
                char name[50] = {0};
                char pwd[50] = {0};
                cout<<"username:";
                cin.getline(name, 50);
                cout<<"password:";
                cin.getline(pwd, 50);
                
                json js;
                js["msgid"] = REG_MSG;
                js["name"] = name;
                js["password"] = pwd;
                string request = js.dump();

                int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
                if(len == -1){
                    cerr<<"send reg msg error:"<<request<<endl;
                }else{
                    char buffer[1024] = {0};
                    len = recv(clientfd, buffer, sizeof(buffer), 0);
                    if(len == -1){
                        cerr<<"recv reg response error:"<<endl;
                    }else{
                        json responsejs = json::parse(buffer);
                        if(responsejs["errno"].get<int>() != 0){//注册失败
                            cerr<<name<<"is already exists, register error!"<<endl;
                        }else{//注册成功
                            cout<<name<<" register success, userid is "<<responsejs["id"]<<", don't forget it!"<<endl;
                        }
                    }
                }
                break;
            }
            case 3://quit业务
            {
                close(clientfd);
                exit(0);
            }
            default:{
                cerr<<"invalid input!"<<endl;
                break;
            }
        }
    }

    return 0;
}

//显示当前登录成功用户的基本信息
void showCurrentUserData(){
    cout << "===============login user===============" << endl;
    cout << "current login user => id:" << g_currentUser.getId() << endl;
    cout << "---------------friend list---------------" << endl;
    if (!g_currentUserFriendList.empty())
    {
        for (int i = 0; i < g_currentUserFriendList.size(); i++)
        {
            cout << g_currentUserFriendList[i].getId() << " " << g_currentUserFriendList[i].getName()
                 << " " << g_currentUserFriendList[i].getState() << endl;
        }
    }
    cout << "---------------group list---------------" << endl;
    if(!g_currentUserGroupList.empty()){
        for(int i=0;i<g_currentUserGroupList.size();i++){
            cout<<g_currentUserGroupList[i].getId()<<" "<<g_currentUserGroupList[i].getName()
            <<" "<<g_currentUserGroupList[i].getDesc()<<endl;
            for(int j=0;j<g_currentUserGroupList[i].getUsers().size();j++){
                cout<<g_currentUserGroupList[i].getUsers()[j].getId()<<" "<<g_currentUserGroupList[i].getUsers()[j].getName()
                <<" "<<g_currentUserGroupList[i].getUsers()[j].getState()<<" "<<g_currentUserGroupList[i].getUsers()[j].getRole()<<endl;
            }
        }
    }
    cout << "=======================================" << endl;
}

//接收线程
void readTaskHandler(int clientfd){
    for(;;){
        char buffer[1024] = {0};
        int len = recv(clientfd, buffer, 1024, 0);
        if(len == -1 || len == 0){
            close(clientfd);
            exit(-1);
        }
        //接收ChatServer转发的数据，反序列化生成json数据对象
        json js = json::parse(buffer);
        if(js["msgid"].get<int>() == ONE_CHAT_MSG){
            cout<<js["time"].get<string>()<<" ["<<js["id"]<<"]"<<js["name"].get<string>()<<" said: "<<js["msg"].get<string>()<<endl;
        } 
        if(js["msgid"].get<int>() == GROUP_CHAT_MSG){
            cout<<"群消息["<<js["groupid"]<<"]: "<<js["time"].get<string>()<<" ["<<js["id"]<<"]"<<js["name"].get<string>()<<" said: "<<js["msg"].get<string>()<<endl;
        }
    }
}

//获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime(){
    auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&tt);
    char date[60] = {0};
    sprintf(date, "%d-%02d-%02d %02d:%02d:%02d",
            (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return std::string(date);
}

void help(int fd = 0, string str = "");
void chat(int, string);
void addfriend(int, string);
void creategroup(int, string);
void addgroup(int, string);
void groupchat(int, string);
void loginout(int, string);

//系统支持的客户端命令列表
unordered_map<string, string> commandMap = {
    {"help", "显示所有支持的命令, 格式help"},
    {"chat", "一对一聊天, 格式chat:friendid:message"},
    {"addfriend", "添加好友, 格式addfriend:friendid"},
    {"creategroup", "创建群组, 格式creategroup:groupname:groupdesc"},
    {"addgroup", "加入群组, 格式addgroup:groupid"},
    {"groupchat", "群聊, 格式groupchat:groupid:message"},
    {"loginout", "注销, 格式loginout"}
};
//注册系统支持的客户端命令处理
unordered_map<string, function<void(int, string)>> commandHandler = {
    {"help", help},
    {"chat", chat},
    {"addfriend", addfriend},
    {"creategroup", creategroup},
    {"addgroup", addgroup},
    {"groupchat", groupchat},
    {"loginout", loginout}
};
//主聊天页面程序
void mainMenu(int clientfd){
    help();

    char buffer[1024] = {0};
    while(isMainMenuRunning){
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        string command;//存储命令
        int idx = commandbuf.find(":");
        if(idx == -1){
            command = commandbuf;//help和loginout没有:
        } else {
            command = commandbuf.substr(0, idx);
        }
        auto it = commandHandler.find(command);
        if(it == commandHandler.end()){
            cerr<<"invalid input command!"<<endl;
            continue;
        }
        
        //调用相应命令的事件处理回调，mainMenu对修改封闭，添加性能不需要修改该函数
        it->second(clientfd, commandbuf.substr(idx + 1, commandbuf.size() - idx));//调用命令处理方法
    }
}

void help(int fd, string str){
    cout << "show command list >>>" << endl;
    for(auto &p : commandMap){
        cout<< p.first << " : " << p.second<<endl;
    }
    cout << endl;
}
void chat(int clientfd, string str){
    int idx = str.find(":");
    if(idx == -1){
        cerr<<"chat command invalid!"<<endl;
        return;
    }
    int friendid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.length() - idx);
    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["toid"] = friendid;
    js["msg"] = message;
    js["time"] = getCurrentTime();
    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(len == -1){
        cerr << "send chat msg error -> " << buffer << endl;
    }
}
void addfriend(int clientfd, string str){
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = g_currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(len == -1){
        cerr<<"send addfriend msg error -> "<<buffer<<endl;
    }
}
void creategroup(int clientfd, string str){
    int idx = str.find(":");
    if(idx == -1){
        cerr<<"creategroup command invalid!"<<endl;
        return;
    }
    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.length() - idx);

    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(len == -1){
        cerr<<"send creategroup msg error -> "<<buffer<<endl;
    }
}
void addgroup(int clientfd, string str){
    int groupid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = g_currentUser.getId();
    js["groupid"] = groupid;

    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(len == -1){
        cerr<<"send addgroup msg error -> "<<buffer<<endl;
    }
}
void groupchat(int clientfd, string str){
    int idx = str.find(":");
    if(idx == -1){
        cerr<<"groupchat command invalid!"<<endl;
        return;
    }
    int groupid = atoi(str.substr(0, idx).c_str());
    string message = str.substr(idx + 1, str.length() - idx);

    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = g_currentUser.getId();
    js["name"] = g_currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = message;
    js["time"] = getCurrentTime();

    string buffer = js.dump();
    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(len == -1){
        cerr << "send groupchat msg error -> " << buffer << endl;
    }
}
void loginout(int clientfd, string str){
    json js;
    js["msgid"] = LOGINOUT_MSG;
    js["id"] = g_currentUser.getId();
    string buffer = js.dump();

    int len = send(clientfd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if(len == -1){
        cerr<<"send loginout msg error -> "<<buffer<<endl;
    }
    isMainMenuRunning = false;
}