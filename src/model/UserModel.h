#ifndef USER_MODEL_H
#define USER_MODEL_H

#include "User.h"
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <pqxx/pqxx>
#include "muduo/base/Logging.h"

// 用户数据访问模型 - 连接PostgreSQL数据库
class UserModel {
public:
    static UserModel& getInstance() {
        static UserModel instance;
        return instance;
    }
    
    ~UserModel();

    // 验证用户登录并更新状态
    bool verifyLogin(const std::string& username, const std::string& password);
    
    // 注册新用户
    bool registerUser(const std::string& username, const std::string& password, 
                      const std::string& email, const std::string& avatar = "");
    
    // 检查用户名是否已存在
    bool isUserExists(const std::string& username);
    
    // 检查邮箱是否已存在
    bool isEmailExists(const std::string& email);
    
    // 根据用户名获取用户信息
    std::shared_ptr<User> getUserByName(const std::string& username);
    
    // 根据ID获取用户信息
    std::shared_ptr<User> getUserById(int userId);
    
    // 更新用户在线状态
    bool updateUserOnlineState(int userId, bool online);
    
    // 更新用户最后登录时间
    bool updateUserLoginTime(int userId);
    
    // 查询所有在线用户
    std::vector<std::shared_ptr<User>> getOnlineUsers();
    
private:
    UserModel(); // 私有构造函数
    
    // 初始化数据库连接
    bool init();
    
    // 获取数据库连接
    std::unique_ptr<pqxx::connection> getConnection();
    
private:
    std::string conninfo_; // 连接信息
    std::mutex mutex_;     // 保护共享数据
};

#endif // USER_MODEL_H
