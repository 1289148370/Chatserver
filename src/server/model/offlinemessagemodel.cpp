#include "offlinemessagemodel.hpp"
#include "db.h"

//存储用户的离线消息表
void offlineMsgModel::insert(int userid, string msg)
{
    //组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "insert into offlinemessage values(%d, '%s')",
            userid, msg.c_str());

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}
//删除用户的离线消息表
void offlineMsgModel::remove(int userid)
{
    //组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "delete from offlinemessage where userid = %d", userid);

    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}
//查询用户的离线消息
vector<string> offlineMsgModel::query(int userid)
{
    //组装sql语句
    char sql[1024] = {0};
    sprintf(sql, "select message from offlinemessage where userid = %d", userid);

    vector<string> vec;
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if(res != nullptr){
            //吧userid用户的所有离线消息放入vector中返回
            MYSQL_ROW row;
            while((row = mysql_fetch_row(res)) != nullptr){
                vec.push_back(row[0]);
            }
            mysql_free_result(res);
            return vec;
        }
    }
    return vec;
}