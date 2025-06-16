#include "UserModel.h"
#include <iostream>
#include <sstream>
#include <cstring>
#include <stdexcept>
#include <chrono>
#include <iomanip>
#include "muduo/base/Timestamp.h"

UserModel::UserModel() {
    // 初始化PostgreSQL连接信息
    conninfo_ = "host=localhost port=5432 dbname=chat_server user=sqhh99 password=2932897504xu";
    
    if (!init()) {
        LOG_ERROR << "Failed to initialize database connection";
    } else {
        LOG_INFO << "Database connection initialized successfully";
    }
}

UserModel::~UserModel() {
    // 连接会在使用unique_ptr后自动关闭
}

bool UserModel::init() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // 测试连接是否有效
        auto conn = std::make_unique<pqxx::connection>(conninfo_);
        if (!conn->is_open()) {
            LOG_ERROR << "Failed to connect to PostgreSQL database";
            return false;
        }
        LOG_INFO << "PostgreSQL connection established";
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR << "PostgreSQL connection error: " << e.what();
        return false;
    }
}

std::unique_ptr<pqxx::connection> UserModel::getConnection() {
    try {
        auto conn = std::make_unique<pqxx::connection>(conninfo_);
        return conn;
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to get database connection: " << e.what();
        return nullptr;
    }
}

bool UserModel::verifyLogin(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto conn = getConnection();
        if (!conn) {
            LOG_ERROR << "Failed to get database connection";
            return false;
        }
        
        // 开始事务
        pqxx::work txn(*conn);
        
        // 查询用户名和密码是否匹配
        std::string sql = "SELECT id FROM users WHERE username = " + 
                          txn.quote(username) + 
                          " AND password = " + 
                          txn.quote(password);
        
        pqxx::result result = txn.exec(sql);
        
        if (result.empty()) {
            LOG_INFO << "Login failed for user: " << username;
            return false;
        }
        
        // 登录成功，更新用户状态
        int userId = result[0][0].as<int>();
        
        // 更新最后登录时间和在线状态
        std::string updateSql = "UPDATE users SET last_login_time = NOW(), online = TRUE WHERE id = " + 
                               std::to_string(userId);
        txn.exec(updateSql);
        
        // 提交事务
        txn.commit();
        
        LOG_INFO << "User " << username << " logged in successfully";
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR << "Login verification failed: " << e.what();
        return false;
    }
}

std::shared_ptr<User> UserModel::getUserByName(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto conn = getConnection();
        if (!conn) {
            LOG_ERROR << "Failed to get database connection";
            return nullptr;
        }
        
        // 开始事务
        pqxx::work txn(*conn);
        
        // 查询用户信息
        std::string sql = "SELECT id, username, email, password, avatar, verified, "
                          "last_login_time, online, create_time FROM users WHERE username = " + 
                          txn.quote(username);
        
        pqxx::result result = txn.exec(sql);
        
        if (result.empty()) {
            LOG_INFO << "User not found: " << username;
            return nullptr;
        }
        
        // 创建用户对象并填充数据
        auto user = std::make_shared<User>();
        user->setId(result[0][0].as<int>());
        user->setUsername(result[0][1].as<std::string>());
        user->setEmail(result[0][2].as<std::string>());
        user->setPassword(result[0][3].as<std::string>());
        
        // 头像可能为空
        if (!result[0][4].is_null()) {
            user->setAvatar(result[0][4].as<std::string>());
        }
        
        user->setVerified(result[0][5].as<bool>());
        
        // 时间戳处理
        if (!result[0][6].is_null()) {
            std::string lastLoginTimeStr = result[0][6].as<std::string>();
            struct tm tm_time = {};
            strptime(lastLoginTimeStr.c_str(), "%Y-%m-%d %H:%M:%S", &tm_time);
            time_t t = mktime(&tm_time);
            user->setLastLoginTime(muduo::Timestamp::fromUnixTime(t));
        }
        
        user->setOnline(result[0][7].as<bool>());
        
        std::string createTimeStr = result[0][8].as<std::string>();
        struct tm tm_create = {};
        strptime(createTimeStr.c_str(), "%Y-%m-%d %H:%M:%S", &tm_create);
        time_t create_t = mktime(&tm_create);
        user->setCreateTime(muduo::Timestamp::fromUnixTime(create_t));
        
        return user;
    } catch (const std::exception& e) {
        LOG_ERROR << "Get user by name failed: " << e.what();
        return nullptr;
    }
}

std::shared_ptr<User> UserModel::getUserById(int userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto conn = getConnection();
        if (!conn) {
            LOG_ERROR << "Failed to get database connection";
            return nullptr;
        }
        
        // 开始事务
        pqxx::work txn(*conn);
        
        // 查询用户信息
        std::string sql = "SELECT id, username, email, password, avatar, verified, "
                          "last_login_time, online, create_time FROM users WHERE id = " + 
                          std::to_string(userId);
        
        pqxx::result result = txn.exec(sql);
        
        if (result.empty()) {
            LOG_INFO << "User not found with ID: " << userId;
            return nullptr;
        }
        
        // 创建用户对象并填充数据
        auto user = std::make_shared<User>();
        user->setId(result[0][0].as<int>());
        user->setUsername(result[0][1].as<std::string>());
        user->setEmail(result[0][2].as<std::string>());
        user->setPassword(result[0][3].as<std::string>());
        
        // 头像可能为空
        if (!result[0][4].is_null()) {
            user->setAvatar(result[0][4].as<std::string>());
        }
        
        user->setVerified(result[0][5].as<bool>());
        
        // 时间戳处理
        if (!result[0][6].is_null()) {
            std::string lastLoginTimeStr = result[0][6].as<std::string>();
            struct tm tm_time = {};
            strptime(lastLoginTimeStr.c_str(), "%Y-%m-%d %H:%M:%S", &tm_time);
            time_t t = mktime(&tm_time);
            user->setLastLoginTime(muduo::Timestamp::fromUnixTime(t));
        }
        
        user->setOnline(result[0][7].as<bool>());
        
        std::string createTimeStr = result[0][8].as<std::string>();
        struct tm tm_create = {};
        strptime(createTimeStr.c_str(), "%Y-%m-%d %H:%M:%S", &tm_create);
        time_t create_t = mktime(&tm_create);
        user->setCreateTime(muduo::Timestamp::fromUnixTime(create_t));
        
        return user;
    } catch (const std::exception& e) {
        LOG_ERROR << "Get user by ID failed: " << e.what();
        return nullptr;
    }
}

bool UserModel::updateUserOnlineState(int userId, bool online) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto conn = getConnection();
        if (!conn) {
            LOG_ERROR << "Failed to get database connection";
            return false;
        }
        
        // 开始事务
        pqxx::work txn(*conn);
        
        // 更新用户在线状态
        std::string sql = "UPDATE users SET online = " + 
                          std::string(online ? "TRUE" : "FALSE") + 
                          " WHERE id = " + 
                          std::to_string(userId);
        
        txn.exec(sql);
        txn.commit();
        
        LOG_INFO << "Updated online state for user " << userId << " to " << (online ? "online" : "offline");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR << "Update user online state failed: " << e.what();
        return false;
    }
}

bool UserModel::updateUserLoginTime(int userId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto conn = getConnection();
        if (!conn) {
            LOG_ERROR << "Failed to get database connection";
            return false;
        }
        
        // 开始事务
        pqxx::work txn(*conn);
        
        // 更新用户最后登录时间
        std::string sql = "UPDATE users SET last_login_time = NOW() WHERE id = " + 
                          std::to_string(userId);
        
        txn.exec(sql);
        txn.commit();
        
        LOG_INFO << "Updated last login time for user " << userId;
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR << "Update user login time failed: " << e.what();
        return false;
    }
}

std::vector<std::shared_ptr<User>> UserModel::getOnlineUsers() {
    std::vector<std::shared_ptr<User>> users;
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        auto conn = getConnection();
        if (!conn) {
            LOG_ERROR << "Failed to get database connection";
            return users;
        }
        
        // 开始事务
        pqxx::work txn(*conn);
        
        // 查询所有在线用户
        std::string sql = "SELECT id, username, email, avatar, verified "
                          "FROM users WHERE online = TRUE";
        
        pqxx::result result = txn.exec(sql);
        
        for (const auto& row : result) {
            auto user = std::make_shared<User>();
            user->setId(row[0].as<int>());
            user->setUsername(row[1].as<std::string>());
            user->setEmail(row[2].as<std::string>());
            
            // 头像可能为空
            if (!row[3].is_null()) {
                user->setAvatar(row[3].as<std::string>());
            }
            
            user->setVerified(row[4].as<bool>());
            user->setOnline(true);
            
            users.push_back(user);
        }
        
        LOG_INFO << "Retrieved " << users.size() << " online users";
        return users;
    } catch (const std::exception& e) {
        LOG_ERROR << "Get online users failed: " << e.what();
        return users;
    }
}
// 新增用户注册功能
bool UserModel::registerUser(const std::string& username, const std::string& password, 
                            const std::string& email, const std::string& avatar) {
    try {
        // 先检查用户名和邮箱是否已存在
        if (isUserExists(username)) {
            LOG_INFO << "Cannot register: Username already exists: " << username;
            return false;
        }
        
        if (isEmailExists(email)) {
            LOG_INFO << "Cannot register: Email already exists: " << email;
            return false;
        }
        
        auto conn = getConnection();
        if (!conn) {
            LOG_ERROR << "Failed to get database connection";
            return false;
        }
        
        // 开始事务
        pqxx::work txn(*conn);
        
        // 准备插入语句
        std::string sql = "INSERT INTO users (username, email, password, avatar, verified, create_time) VALUES ("
                        + txn.quote(username) + ", "
                        + txn.quote(email) + ", "
                        + txn.quote(password) + ", "
                        + txn.quote(avatar) + ", "
                        + "TRUE, NOW()) RETURNING id";
        
        // 执行SQL并获取新插入的用户ID
        pqxx::result result = txn.exec(sql);
        txn.commit();
        
        if (!result.empty()) {
            int userId = result[0][0].as<int>();
            LOG_INFO << "User registered successfully: " << username << " (ID: " << userId << ")";
            return true;
        } else {
            LOG_ERROR << "Failed to retrieve new user ID after registration";
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "Error registering user: " << e.what();
        return false;
    }
}

// 检查用户名是否已存在
bool UserModel::isUserExists(const std::string& username) {
    try {
        auto conn = getConnection();
        if (!conn) {
            LOG_ERROR << "Failed to get database connection";
            return false;
        }
        
        pqxx::work txn(*conn);
        
        std::string sql = "SELECT COUNT(*) FROM users WHERE username = " + txn.quote(username);
        pqxx::result result = txn.exec(sql);
        
        int count = result[0][0].as<int>();
        return count > 0;
    } catch (const std::exception& e) {
        LOG_ERROR << "Error checking username existence: " << e.what();
        return false;
    }
}

// 检查邮箱是否已存在
bool UserModel::isEmailExists(const std::string& email) {
    try {
        auto conn = getConnection();
        if (!conn) {
            LOG_ERROR << "Failed to get database connection";
            return false;
        }
        
        pqxx::work txn(*conn);
        
        std::string sql = "SELECT COUNT(*) FROM users WHERE email = " + txn.quote(email);
        pqxx::result result = txn.exec(sql);
        
        int count = result[0][0].as<int>();
        return count > 0;
    } catch (const std::exception& e) {
        LOG_ERROR << "Error checking email existence: " << e.what();
        return false;
    }
}
