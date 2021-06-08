#ifndef CHATSERVER_H
#define CHATSERVER_H

#include<muduo/net/TcpServer.h>
#include<muduo/net/EventLoop.h>
using namespace muduo;
using namespace muduo::net;

class ChatServer {
public:
	//初始化聊天服务器
	ChatServer(EventLoop *loop,
		const InetAddress &listenAddr,
		const string& nameArg);
	
	//开启事件循环
	void start();
    
private:

	//专门处理用户的连接创建和断开
	void onConection(const TcpConnectionPtr &);

	//专门处理用户的读写事件
	void onMessage(const TcpConnectionPtr &, 
                    Buffer *, 
                    Timestamp);
	TcpServer _server;  //1、定义一个server对象
	EventLoop *_loop;   //2、epoll,,创建EventLoop事件循环对象的指针
};


#endif