#ifndef USER_H
#define USER_H

#include <string>
#include "muduo/base/Timestamp.h"

// 用户状态
enum class UserState {
    OFFLINE,  // 离线
    ONLINE    // 在线
};

// 用户信息类 - 对应数据库users表
class User {
public:
    User() = default;
    User(int id, std::string username, std::string pwd) 
        : id_(id), username_(username), password_(pwd), online_(false) {}
    
    // 基本属性设置和获取
    void setId(int id) { id_ = id; }
    int getId() const { return id_; }
    
    void setUsername(const std::string &username) { username_ = username; }
    std::string getUsername() const { return username_; }
    
    void setEmail(const std::string &email) { email_ = email; }
    std::string getEmail() const { return email_; }
    
    void setPassword(const std::string &password) { password_ = password; }
    std::string getPassword() const { return password_; }
    
    void setAvatar(const std::string &avatar) { avatar_ = avatar; }
    std::string getAvatar() const { return avatar_; }
    
    void setVerified(bool verified) { verified_ = verified; }
    bool getVerified() const { return verified_; }
    
    void setLastLoginTime(const muduo::Timestamp &time) { last_login_time_ = time; }
    muduo::Timestamp getLastLoginTime() const { return last_login_time_; }
    
    void setOnline(bool online) { online_ = online; }
    bool isOnline() const { return online_; }
    
    void setCreateTime(const muduo::Timestamp &time) { create_time_ = time; }
    muduo::Timestamp getCreateTime() const { return create_time_; }
    
private:
    int id_{-1};                           // 用户ID
    std::string username_;                 // 用户名
    std::string email_;                    // 邮箱
    std::string password_;                 // 密码
    std::string avatar_;                   // 头像URL
    bool verified_{false};                 // 是否已验证
    muduo::Timestamp last_login_time_;     // 最后登录时间
    bool online_{false};                   // 是否在线
    muduo::Timestamp create_time_;         // 创建时间
};

#endif // USER_H
