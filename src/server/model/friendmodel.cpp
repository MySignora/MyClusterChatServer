#include "friendmodel.hpp"
#include "db.h"

//添加好友
void FriendModel::insert(int userid, int friendid){
    //1.组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into friend values(%d, '%d')", userid, friendid);
    MySQL mysql;
    if(mysql.connect()){
        mysql.update(sql);
    }
}

//返回用户好友列表
vector<User> FriendModel::query(int userid){
    //1.组装sql语句
    char sql[1024] = {0};
    // sprintf(sql, "select id,name,state from user where user.id = (select friendid from friend where userid=%d", userid);
    sprintf(sql, "select a.id,a.name,a.state from user a inner join friend b on b.friendid=a.id where b.userid = %d", userid);

    vector<User> vec;
    MySQL mysql;
    if(mysql.connect()){
        MYSQL_RES* res = mysql.query(sql);
        if(res != NULL){
            
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != NULL){
                User user;
                user.setId(atoi(row[0]));
                user.setName(string(row[1]));
                user.setState(string(row[2]));
                vec.push_back(user);
            }
            mysql_free_result(res);
            return vec;
        }
    }
    return vec;
}