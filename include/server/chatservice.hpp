#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <iostream>
#include <functional>
#include <mutex>
#include <muduo/net/TcpConnection.h>
#include "json.hpp"
#include <map>
#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include "redis.hpp"

using namespace muduo;
using namespace muduo::net;
using namespace std;
using json = nlohmann::json;

//消息处理器的类型重命名
using MsgHandler = function<void(const TcpConnectionPtr &coon, json &js, Timestamp)>;

//聊天服务器的业务类，单例模式
class ChatService
{
public:
    //提供单例的获取接口
    static ChatService *instance();

    //处理登录业务
    void login(const TcpConnectionPtr &coon, json &js, Timestamp);
    //处理注册业务
    void reg(const TcpConnectionPtr &coon, json &js, Timestamp);
    //一对一聊天业务
    void oneChat(const TcpConnectionPtr &coon, json &js, Timestamp);
    //添加好友业务
    void addFriend(const TcpConnectionPtr &coon, json &js, Timestamp);
    //创建群组业务
    void creatGroup(const TcpConnectionPtr &coon, json &js, Timestamp);
    //加入群组业务
    void addGroup(const TcpConnectionPtr &coon, json &js, Timestamp);
    //群组聊天业务
    void groupChat(const TcpConnectionPtr &coon, json &js, Timestamp);
    //获取消息对应的处理器
    MsgHandler getHandler(int msgid);
    //处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr &coon);
    //处理服务器异常退出
    void reset();
    // 处理注销业务
    void loginout(const TcpConnectionPtr &conn, json &js, Timestamp time);
    //从redis消息队列中获取订阅的消息
    void handleRedisSubscribeMessage(int, string);

private:
    //构造函数私有化
    ChatService();

    //存储消息id和其对应的业务处理方法
    unordered_map<int, MsgHandler> _msgHandlerMap;

    //存储在线用户的连接信息,这个范文应该考虑线程安全问题
    unordered_map<int, TcpConnectionPtr> _userConnMap;
    //定义互斥锁，保证_userConnMap得访问安全
    mutex _connMutex;

    //数据操作类对象
    UserModel _userModel;
    offlineMsgModel _offlineMsgModel;
    FriendModel _friendMoudel;
    GroupModel _groupMoudel;

    //redis操作对象
    Redis _redis;
};

#endif