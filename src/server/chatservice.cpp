#include "chatservice.hpp"
#include "public.hpp"
#include <muduo/base/Logging.h>
#include <vector>

using namespace std;
using namespace muduo;
using namespace placeholders;
//提供单例的获取接口
ChatService *ChatService::instance()
{
    static ChatService service;
    return &service;
}
//注册消息以及对应的Handle回调操作
ChatService::ChatService()
{
    // 用户基本业务管理相关事件处理回调注册
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGINOUT_MSG, std::bind(&ChatService::loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, std::bind(&ChatService::addFriend, this, _1, _2, _3)});

    // 群组业务管理相关事件处理回调注册
    _msgHandlerMap.insert({CREAT_GROUP_MSG, std::bind(&ChatService::creatGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

    //连接redis服务器
    if (_redis.connect())
    {
        //设置上报消息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

//获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    //记录错误日志
    auto it = _msgHandlerMap.find(msgid);
    if (it == _msgHandlerMap.end())
    {
        //返回一个默认的处理器，什么都不做
        return [=](const TcpConnectionPtr &coon, json &js, Timestamp)
        {
            LOG_ERROR << "msgid:" << msgid << "can not find handler";
        };
    }
    else
    {
        return _msgHandlerMap[msgid];
    }
}
//处理登录业务  id pwd
void ChatService::login(const TcpConnectionPtr &coon, json &js, Timestamp time)
{
    int id = js["id"].get<int>();
    string pwd = js["password"];

    User user = _userModel.query(id);
    if (user.getId() == id && user.getPwd() == pwd)
    {
        if (user.getState() == "online")
        {
            //该用户已经登陆，不允许重新登陆
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2;
            response["errmsg"] = "The user has logged in and is not allowed to log in again";
            coon->send(response.dump());
        }
        else
        {
            {
                lock_guard<mutex> lock(_connMutex);
                //登陆成功，记录用户连接信息
                _userConnMap.insert({id, coon});
            }

            //id用户登陆成功后，向redis订阅channel(id)
            _redis.subscribe(id);

            //登陆成功，更新用户状态信息
            user.setState("online");
            _userModel.updateState(user);

            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0;
            response["id"] = user.getId();
            response["name"] = user.getName();

            //查询该用户是否有离线消息
            vector<string> vec = _offlineMsgModel.query(id);
            if (!vec.empty())
            {
                response["offlinemsg"] = vec;
                //读取完用户的离线消息之后将其清除
                _offlineMsgModel.remove(id);
            }
            //查询该用户的好友信息并返回
            vector<User> userVec = _friendMoudel.query(id);
            if (!userVec.empty())
            {
                vector<string> vec2;
                for (User &user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }
            // 查询用户的群组信息
            vector<Group> groupuserVec = _groupMoudel.queryGroups(id);
            if (!groupuserVec.empty())
            {
                // group:[{groupid:[xxx, xxx, xxx, xxx]}]
                vector<string> groupV;
                for (Group &group : groupuserVec)
                {
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for (GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }

                response["groups"] = groupV;
            }

            coon->send(response.dump());
        }
    }
    else
    {
        //该用户不存在，用户存在但是密码错误
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1;
        response["errmsg"] = "User name or password error";
        coon->send(response.dump());
    }
}
//处理注册业务  name password
void ChatService::reg(const TcpConnectionPtr &coon, json &js, Timestamp)
{
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);

    bool start = _userModel.insert(user);
    if (start)
    {
        //注册成功
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;
        response["id"] = user.getId();
        coon->send(response.dump());
    }
    else
    {
        //注册失败
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;
        coon->send(response.dump());
    }
}

//处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &coon)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); it++)
        {
            if (it->second == coon)
            {
                //从mao表删除用户的连接信息
                user.setId(it->first);
                _userConnMap.erase(it);
                break;
            }
        }
    }

    //用户注销，相当于就是下线，在redis中取消订阅通道
    _redis.unsubscribe(user.getId());

    if (user.getId() != -1)
    {
        //更新用户的状态信息
        user.setState("offline");
        _userModel.updateState(user);
    }
}

//处理服务器异常退出
void ChatService::reset()
{
    //把online状态的用户设置为offline
    _userModel.resetState();
}

//一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &coon, json &js, Timestamp time)
{
    int toid = js["toid"].get<int>();
    //在存储连接信息得mao表中找对方id对应的conn
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if (it != _userConnMap.end())
        {
            //toid在线，转发消息,服务器主动推送消息给toid
            it->second->send(js.dump());
            return;
        }
    }

    //查询toid是否在数据库中是在线
    User user = _userModel.query(toid);
    if (user.getState() == "online")
    {
        _redis.publish(toid, js.dump());
        return;
    }

    //toid不在线,存储离线消息
    _offlineMsgModel.insert(toid, js.dump());
}
//添加好友业务 msgid id  friendid
void ChatService::addFriend(const TcpConnectionPtr &coon, json &js, Timestamp)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

    //存储好友信息
    _friendMoudel.insert(userid, friendid);
}

//创建群组业务
void ChatService::creatGroup(const TcpConnectionPtr &coon, json &js, Timestamp)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    //存储新创建的群组信息
    Group group(-1, name, desc);
    if (_groupMoudel.createGroup(group))
    {
        //存储群组创建人信息
        _groupMoudel.addGroup(userid, group.getId(), "creator");
    }
}
//加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &coon, json &js, Timestamp)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupMoudel.addGroup(userid, groupid, "normal");
}
//群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &coon, json &js, Timestamp)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupMoudel.queryGroupUsers(userid, groupid);

    lock_guard<mutex> lock(_connMutex);
    for (int id : useridVec)
    {
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            //转发群消息
            it->second->send(js.dump());
        }
        else
        {
            //查询id是否在数据库中是否在线
            User user = _userModel.query(id);
            if (user.getState() == "online")
            {
                _redis.publish(id, js.dump());
                return;
            }
            else
            {
                //存储离线群消息
                _offlineMsgModel.insert(id, js.dump());
            }
        }
    }
}
// 处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }
    //用户注销，相当于就是下线，在redis中取消订阅通道
    _redis.unsubscribe(userid);

    // 更新用户的状态信息
    User user(userid, "", "", "offline");
    _userModel.updateState(user);
}

//从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{

    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end())
    {
        it->second->send(msg);
        return;
    }

    //存储用户的离线消息
    _offlineMsgModel.insert(userid, msg);
}