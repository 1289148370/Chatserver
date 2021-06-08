#ifndef FRIENDMOUDEL_H
#define FRIENDMOUSEL_H

#include "user.hpp"
#include <vector>
//提供好友信息的操作接口方法
class FriendModel
{
public:
    //添加好友信息
    void insert(int userid, int friendid);

    //返回用户的好友列表 friendid —》user->friendname,frendstate
    //直接做联合查询
    vector<User> query(int userid);
};
#endif