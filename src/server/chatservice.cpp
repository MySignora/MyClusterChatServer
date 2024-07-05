#include "chatservice.hpp"
#include "public.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include <muduo/base/Logging.h>

#include <vector>
using namespace std;
using namespace muduo;

//获取单例对象的接口函数
ChatService* ChatService::instance(){
    static ChatService service;
    return &service;
}
//注册消息以及对应的Handler回调操作
ChatService::ChatService(){
    //用户基本业务管理相关事件处理回调函数
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});

    //群组业务管理相关事件处理回调函数
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    //连接redis服务器
    if(_redis.connect()){
        //设置上报消息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

//获取消息对应的处理器
Msghandler ChatService::getHandler(int msgid){

    //记录错误日志，msgid没有对应的事件处理回调
    if(_msgHandlerMap.find(msgid)==_msgHandlerMap.end()){
        //返回一个默认的处理器，空操作
        return [=](const TcpConnectionPtr& conn, json& js, Timestamp time){
            LOG_ERROR << "msgid:" << msgid << " can not find handler!";
        };
    }else{
        return _msgHandlerMap[msgid];
    }
}

//登录
void ChatService::login(const TcpConnectionPtr& conn, json& js, Timestamp time){
    int id = js["id"].get<int>();
    string pwd = js["password"];
    User user = _userModel.query(id);
    if(user.getId() != -1 && user.getPwd() == pwd){
        if(user.getState() == "online"){
            //该用户已经登录，不允许重复登录
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "This count is using, input other!";
            conn->send(response.dump());
        }else{
            //登录成功，记录用户连接信息
            {
                lock_guard<mutex> lock(_connMutex);
                _userConnmap.insert({id, conn});
            }

            //id用户登录成功后，向redis订阅channel(id)
            _redis.subscribe(id);
            
            //登录成功，更新用户状态信息state offline-》online
            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();
            //查询该用户是否有离线消息
            vector<string> vec = _offlineMsgmodel.query(id);
            if(!vec.empty()){
                response["offlinemsg"] = vec;
                //读取该用户的这条离线消息，把这条消息删除掉
                _offlineMsgmodel.remove(id);
            } 
            //查询该用户的好友信息并返回
            vector<User> userVec = _friendModel.query(id);
            if(!userVec.empty()){
                vector<string> vec2;
                for(int i=0;i<userVec.size();i++){
                    json js;
                    js["id"] = userVec[i].getId();
                    js["name"] = userVec[i].getName();
                    js["state"] = userVec[i].getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }
            //查询该用户的群组信息并返回
            vector<Group> groupVec = _groupModel.queryGroups(id);
            if(!groupVec.empty()){
                vector<string> vec3;
                for(int i=0;i<groupVec.size();i++){
                    json js;
                    js["id"] = groupVec[i].getId();
                    js["groupname"] = groupVec[i].getName();
                    js["groupdesc"] = groupVec[i].getDesc();
                    vector<string> userV;
                    for(int j=0;j<groupVec[i].getUsers().size();j++){
                        json js1;
                        js1["id"] = groupVec[i].getUsers()[j].getId();
                        js1["name"] = groupVec[i].getUsers()[j].getName();
                        js1["state"] = groupVec[i].getUsers()[j].getState();
                        js1["role"] = groupVec[i].getUsers()[j].getRole();
                        userV.push_back(js1.dump());
                    }
                    js["users"] = userV;
                    vec3.push_back(js.dump());
                }
                response["groups"] = vec3;
            }

            conn->send(response.dump());
        }
    }else{
        //该用户不存在，用户存在但是密码错误，登录失败
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "id or password is invalid!";
        conn->send(response.dump());
    }
}
//注册 name password
void ChatService::reg(const TcpConnectionPtr& conn, json& js, Timestamp time){
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    
    bool state = _userModel.insert(user);
    if(state){
        //注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump());
    }else{
        //注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        conn->send(response.dump());
    }
}
//处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr& conn){
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for(auto it=_userConnmap.begin();it!=_userConnmap.end();it++){
            if(it->second == conn){
                //从_userConnmap删除用户的连接信息
                _userConnmap.erase(it);
                user.setId(it->first);
                break;
            }
        }
    }
    //用户注销，相当于是下线，在redis中取消订阅
    _redis.unsubscribe(user.getId());

    //更新用户的状态信息
    if(user.getId() != -1){
        user.setState("offline");
        _userModel.updateState(user);
    }
}
//一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr& conn, json& js, Timestamp time){
    int toid = js["toid"].get<int>();
    
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnmap.find(toid);
        if(it != _userConnmap.end()){
            //toid在线，转发消息   服务器主动推送消息给toid用户
            //(说明两个人在同一台服务器上，直接转发)
            it->second->send(js.dump());
            return;
        }
    }
    //查询toid是否在线，可能对方在另一台服务器上
    User user = _userModel.query(toid);
    if(user.getState() == "online"){
        _redis.publish(toid, js.dump());
        return;
    }
    //toid不在线，存储离线消息
    _offlineMsgmodel.insert(toid, js.dump());
}
//服务器异常，业务重置方法
void ChatService::reset(){
    //把online状态的用户，设置为offline
    _userModel.resetState();
}
//添加好友业务 
void ChatService::addFriend(const TcpConnectionPtr& conn, json& js, Timestamp time){
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();
    //存储好友信息
    _friendModel.insert(userid, friendid);
}
//创建群组业务
void ChatService::createGroup(const TcpConnectionPtr& conn, json& js, Timestamp time){
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];
    //存储新创建的群组消息
    Group group(-1, name, desc);
    if(_groupModel.createGroup(group)){
        //存储群组创建人信息
        _groupModel.addGroup(userid, group.getId(), "creator");
    }
}
//加入群组业务
void ChatService::addGroup(const TcpConnectionPtr& conn, json& js, Timestamp time){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");
}
//群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr& conn, json& js, Timestamp time){
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);
    lock_guard<mutex> lock(mutex);
    for(int i=0;i<useridVec.size();i++){
        if(_userConnmap.find(useridVec[i]) != _userConnmap.end()){
            //给useridVec[i]转发消息
            _userConnmap[useridVec[i]]->send(js.dump());
        }else{
            //查询toid是否在线，可能在另一台服务器上
            User user = _userModel.query(useridVec[i]);
            if(user.getState() == "online"){
                _redis.publish(useridVec[i], js.dump());
            }else{
                //存储离线消息
                _offlineMsgmodel.insert(useridVec[i], js.dump());
            }
        }
    }
}
//处理注销业务
void ChatService::loginout(const TcpConnectionPtr& conn, json& js, Timestamp time){
    int userid = js["id"].get<int>();
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnmap.find(userid);
        if(it != _userConnmap.end()){
            _userConnmap.erase(it);
        }
    }
    //用户注销，相当于是下线，在redis中取消订阅通道
    _redis.unsubscribe(userid);

    User user(userid, "", "", "offline");
    _userModel.updateState(user);
}
//从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg){
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnmap.find(userid);
    if(it != _userConnmap.end()){
        it->second->send(msg);
        return;
    }
    //存储该用户的离线消息
    _offlineMsgmodel.insert(userid, msg);
}