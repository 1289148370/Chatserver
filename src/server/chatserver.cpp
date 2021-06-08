#include "chatserver.hpp"
#include <functional>
#include <string>
#include "json.hpp"
#include "chatservice.hpp"


using namespace std;
using namespace placeholders;
using json = nlohmann::json;

ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &nameArg)
    : _server(loop, listenAddr, nameArg), _loop(loop)
{
    //注册链接回调
    _server.setConnectionCallback(std::bind(&ChatServer::onConection, this, _1));

    //注册消息回调
    _server.setMessageCallback(std::bind(&ChatServer::onMessage, this, _1, _2, _3));

    //设置服务器端的线程数量  
    _server.setThreadNum(4);
}
//开启事件循环
void ChatServer::start()
{
    _server.start();
}

//上报连接相关信息的回调函数
void ChatServer::onConection(const TcpConnectionPtr &conn)
{
    //客户端断开连接
    if(!conn->connected()){
        //客户端异常断开
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown();
    }
}
//上报读写相关信息的回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn, //连接
                           Buffer *buffer,               //缓冲区
                           Timestamp time)               //接收到数据的时间信息
{
    string buf = buffer->retrieveAllAsString();
    //数据的反序列化,反序列化以后js里面存在消息类型，网络模块和业务模块代码解耦
    //不要在这里判断js里面的消息类型，然后直接进行处理，应该进行回调
    //解耦的两种方法:接口、回调
    //通过js["msgid"]获取=》业务handler=>conn js time
    json js = json::parse(buf);

    //获取js["msgid"]对应的处理器    
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());
    msgHandler(conn, js, time);
}
