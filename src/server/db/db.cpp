#include "db.h"

#include <muduo/base/Logging.h>

//初始化连接
MySQL::MySQL(){
    _conn = mysql_init(NULL);
}
//释放连接
MySQL::~MySQL(){
    if(_conn != NULL){
        mysql_close(_conn);
    }
}
//连接数据库
bool MySQL::connect(){
    MYSQL* p = mysql_real_connect(_conn, server.c_str(), user.c_str(), password.c_str(), dbname.c_str(), 3306, NULL, 0);
    if(p != NULL){
        //C和C++代码默认的编码字符是ASCII，如果不设置，从mysql获取的数据是中文
        mysql_query(_conn, "set names gbk");
        LOG_INFO << "connect mysql success!";
        // return true;
    }else{
        LOG_INFO << "connect mysql fail!";
    }
    // return false;
    return p;//?为什么返回这个p
}
//更新操作
bool MySQL::update(string sql){
    if(mysql_query(_conn, sql.c_str())){
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql <<"更新失败！";
        return false;
    }
    return true;
}
//查询操作
MYSQL_RES* MySQL::query(string sql){
    if(mysql_query(_conn, sql.c_str())){
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":" << sql <<"查询失败！";
        return NULL;
    }
    // return mysql_store_result(_conn);
    return mysql_use_result(_conn);
}
//获取连接
MYSQL* MySQL::getConnection(){
    return _conn;
}