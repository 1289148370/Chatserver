#ifndef GROUPUSER_H
#define GROUPUSER_H
#include "user.hpp"
#include <string>
using namespace std;
class GroupUser : public User
{
public:
    void setRole(string role) { this->role = role; }
    string getRole() { return this->role; }

private:
    string role;
};

#endif