#include "json.hpp"
using json = nlohmann::json;

#include <iostream>
#include <vector>
#include <map>
#include <string>
using namespace std;

//json序列化实例1
string func1(){
    json js;
    js["msg_type"] = 2;
    js["from"] = "zhang san";
    js["to"] = "li si";
    js["msg"] = "hello, what are you doing now?";

    string sendBuf = js.dump();
    return sendBuf;
}
//json序列化实例2
string func2(){
    json js;
    //添加数组
    js["id"] = {1,2,3,4,5};
    //添加key-value
    js["name"] = "zhang san";
    //添加对象
    js["msg"]["zhang san"] = "hello world";
    js["msg"]["liu shuo"] = "hello china";
    //上面等同于下面一次性添加数组对象
    js["msg"] = {{"zhang san", "hello world"},{"liu shuo", "hello china"}};
    return js.dump();
}
//json序列化实例3
string func3(){
    json js; 
    // 直接序列化一个vector容器 
    vector<int> vec; 
    vec.push_back(1); 
    vec.push_back(2); 
    vec.push_back(5); 
    js["list"] = vec; 
    // 直接序列化一个map容器 
    map<int, string> m; 
    m.insert({1, "黄山"}); 
    m.insert({2, "华山"}); 
    m.insert({3, "泰山"}); 
    js["path"] = m; 

    string sendBuf = js.dump();
    return sendBuf;
}
int main(){
    // string recvBuf = func1();
    // //数据的反序列化    json字符串---->反序列化 数据对象（看作容器，方便访问）
    // json jsbuf = json::parse(recvBuf);
    // cout<<jsbuf["msg_type"]<<endl;
    // cout<<jsbuf["from"]<<endl;
    // cout<<jsbuf["to"]<<endl;
    // cout<<jsbuf["msg"]<<endl;

    // string recvBuf = func2();
    // //数据的反序列化    json字符串---->反序列化 数据对象（看作容器，方便访问）
    // json jsbuf = json::parse(recvBuf);
    // cout<<jsbuf["id"]<<endl;
    // auto arr=jsbuf["id"];
    // cout<<arr<<endl;

    string recvBuf = func3();
    json jsbuf = json::parse(recvBuf);
    vector<int> vec = jsbuf["list"];
    
    return 0;
}