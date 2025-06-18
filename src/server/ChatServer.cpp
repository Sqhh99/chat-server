#include "ChatServer.h"
#include "../service/RedisService.h"
#include "../service/EmailService.h"
#include "../service/VerificationCodeService.h"
#include "../model/UserModel.h"
#include "../model/User.h"
#include <json/json.h>
#include <functional>
#include <chrono>
#include <thread>
#include <sstream>
#include <regex>
#include <unordered_set>

// 使用C++11的std::placeholders
using namespace std::placeholders;

ChatServer::ChatServer(muduo::net::EventLoop* loop, 
                     const muduo::net::InetAddress& listenAddr, 
                     const std::string& nameArg)
    : server_(loop, listenAddr, nameArg),
      loop_(loop)
{
    // 注册连接回调
    server_.setConnectionCallback(
        std::bind(&ChatServer::onConnection, this, _1)
    );
    
    // 注册消息回调
    server_.setMessageCallback(
        std::bind(&ChatServer::onMessage, this, _1, _2, _3)
    );
    
    // 设置服务器线程数量 - 一个I/O线程，四个工作线程
    server_.setThreadNum(4);
    
    // 注册消息处理器
    msgHandlerMap_[static_cast<int>(MessageType::LOGIN_REQUEST)] = 
        std::bind(&ChatServer::handleLogin, this, _1, _2);
        
    msgHandlerMap_[static_cast<int>(MessageType::HEARTBEAT_REQUEST)] =
        std::bind(&ChatServer::handleHeartbeat, this, _1, _2);
        
    msgHandlerMap_[static_cast<int>(MessageType::LOGOUT_REQUEST)] =
        std::bind(&ChatServer::handleLogout, this, _1, _2);
        
    msgHandlerMap_[static_cast<int>(MessageType::VERIFY_CODE_REQUEST)] =
        std::bind(&ChatServer::handleVerifyCodeRequest, this, _1, _2);
        
    msgHandlerMap_[static_cast<int>(MessageType::REGISTER_REQUEST)] =
        std::bind(&ChatServer::handleRegister, this, _1, _2);
        
    // 注册聊天相关消息处理器
    msgHandlerMap_[static_cast<int>(MessageType::PRIVATE_CHAT)] =
        std::bind(&ChatServer::handlePrivateChat, this, _1, _2);
        
    msgHandlerMap_[static_cast<int>(MessageType::GROUP_CHAT)] =
        std::bind(&ChatServer::handleGroupChat, this, _1, _2);
        
    msgHandlerMap_[static_cast<int>(MessageType::CREATE_GROUP)] =
        std::bind(&ChatServer::handleCreateGroup, this, _1, _2);
        
    msgHandlerMap_[static_cast<int>(MessageType::JOIN_GROUP)] =
        std::bind(&ChatServer::handleJoinGroup, this, _1, _2);
        
    msgHandlerMap_[static_cast<int>(MessageType::LEAVE_GROUP)] =
        std::bind(&ChatServer::handleLeaveGroup, this, _1, _2);
        
    msgHandlerMap_[static_cast<int>(MessageType::GET_USER_LIST)] =
        std::bind(&ChatServer::handleGetUserList, this, _1, _2);
        
    msgHandlerMap_[static_cast<int>(MessageType::GET_GROUP_LIST)] =
        std::bind(&ChatServer::handleGetGroupList, this, _1, _2);
        
    msgHandlerMap_[static_cast<int>(MessageType::GET_GROUP_MEMBERS)] =
        std::bind(&ChatServer::handleGetGroupMembers, this, _1, _2);
        
    msgHandlerMap_[static_cast<int>(MessageType::GET_USER_FRIENDS)] =
        std::bind(&ChatServer::handleGetUserFriends, this, _1, _2);
        
    msgHandlerMap_[static_cast<int>(MessageType::ADD_FRIEND_REQUEST)] =
        std::bind(&ChatServer::handleAddFriendRequest, this, _1, _2);
        
    msgHandlerMap_[static_cast<int>(MessageType::ACCEPT_FRIEND_REQUEST)] =
        std::bind(&ChatServer::handleAcceptFriendRequest, this, _1, _2);
        
    msgHandlerMap_[static_cast<int>(MessageType::REJECT_FRIEND_REQUEST)] =
        std::bind(&ChatServer::handleRejectFriendRequest, this, _1, _2);
        
    msgHandlerMap_[static_cast<int>(MessageType::GET_FRIEND_REQUESTS)] =
        std::bind(&ChatServer::handleGetFriendRequests, this, _1, _2);
        
    // 保留旧的ADD_FRIEND处理器以兼容性 (使用数字28作为向后兼容)
    msgHandlerMap_[28] = std::bind(&ChatServer::handleAddFriend, this, _1, _2);
        
    msgHandlerMap_[static_cast<int>(MessageType::GET_CHAT_HISTORY)] =
        std::bind(&ChatServer::handleGetChatHistory, this, _1, _2);
        
    msgHandlerMap_[static_cast<int>(MessageType::RECALL_MESSAGE)] =
        std::bind(&ChatServer::handleRecallMessage, this, _1, _2);
        
    msgHandlerMap_[static_cast<int>(MessageType::MARK_MESSAGE_READ)] =
        std::bind(&ChatServer::handleMarkMessageRead, this, _1, _2);
        
    // 初始化邮件服务
    EmailService::getInstance().init(
        "smtp.163.com",       // SMTP服务器
        25,                   // 非SSL端口
        "sqhh99@163.com",     // 用户名
        "GHx58fk48fuxcw8H",   // 授权码
        "sqhh99@163.com",     // 发件人邮箱
        "ChatServer"          // 发件人名称
    );
}

void ChatServer::start()
{
    server_.start();
    
    // 启动心跳检查定时器
    heartbeatTimerId_ = loop_->runEvery(HEARTBEAT_CHECK_INTERVAL, 
                                       std::bind(&ChatServer::checkHeartbeats, this));
    
    LOG_INFO << "ChatServer started on " << server_.ipPort();
    LOG_INFO << "Heartbeat check started with interval " << HEARTBEAT_CHECK_INTERVAL 
             << "s and timeout " << HEARTBEAT_TIMEOUT << "s";
}

void ChatServer::stop()
{
    // 取消心跳检查定时器
    loop_->cancel(heartbeatTimerId_);
    
    // 关闭服务器
    server_.getLoop()->quit();
    
    LOG_INFO << "ChatServer stopped";
}

void ChatServer::onConnection(const muduo::net::TcpConnectionPtr& conn)
{
    // 处理客户端连接与断开
    if (conn->connected())
    {
        LOG_INFO << "Client connected: " << conn->peerAddress().toIpPort();
        
        // 记录连接最后活动时间
        connectionLastActiveTime_[conn] = muduo::Timestamp::now();
    }
    else
    {
        LOG_INFO << "Client disconnected: " << conn->peerAddress().toIpPort();
        
        // 查找断开的连接是否为已登录用户
        for (auto it = userConnectionMap_.begin(); it != userConnectionMap_.end(); ++it)
        {
            if (it->second == conn)
            {
                // 用户断开连接，更新用户状态为离线（数据库和Redis）
                UserModel::getInstance().updateUserOnlineState(it->first, false);
                RedisService::getInstance().setUserOnline(it->first, false);
                
                // 从用户连接映射表移除
                userConnectionMap_.erase(it);
                break;
            }
        }
        
        // 从活动时间映射表中移除
        connectionLastActiveTime_.erase(conn);
    }
}

void ChatServer::onMessage(const muduo::net::TcpConnectionPtr& conn,
                         muduo::net::Buffer* buffer,
                         muduo::Timestamp /* time */)
{
    // 更新连接最后活动时间
    connectionLastActiveTime_[conn] = muduo::Timestamp::now();
    
    // 提取消息内容
    std::string message = buffer->retrieveAllAsString();
    
    // 这里简化处理，暂时直接处理分隔符格式的消息
    // 格式: msgType:key1=value1;key2=value2;...
    std::stringstream ss(message);
    std::string msgTypeStr;
    std::getline(ss, msgTypeStr, ':');
    
    try {
        int msgType = std::stoi(msgTypeStr);
        
        // 解析消息内容到键值对
        std::string content;
        std::getline(ss, content);
        
        // 创建一个简单的键值对集合
        std::unordered_map<std::string, std::string> msgData;
        
        std::stringstream contentStream(content);
        std::string item;
        while (std::getline(contentStream, item, ';')) {
            std::size_t pos = item.find('=');
            if (pos != std::string::npos) {
                std::string key = item.substr(0, pos);
                std::string value = item.substr(pos + 1);
                msgData[key] = value;
            }
        }
        
        // 根据消息类型进行处理
        auto it = msgHandlerMap_.find(msgType);
        if (it != msgHandlerMap_.end()) {
            // 调用对应的消息处理函数
            it->second(conn, msgData);
        } else {
            LOG_ERROR << "Unknown message type: " << msgType;
            
            // 发送错误消息给客户端
            std::string errorMsg = std::to_string(static_cast<int>(MessageType::ERROR)) + 
                                   ":errorMsg=Unknown message type";
            conn->send(errorMsg);
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "Invalid message format: " << e.what();
        
        // 发送错误消息给客户端
        std::string errorMsg = std::to_string(static_cast<int>(MessageType::ERROR)) + 
                               ":errorMsg=Invalid message format";
        conn->send(errorMsg);
    }
}

void ChatServer::handleLogin(const muduo::net::TcpConnectionPtr& conn, 
                         const std::unordered_map<std::string, std::string>& msg)
{
    auto usernameIt = msg.find("username");
    auto passwordIt = msg.find("password");
    
    if (usernameIt == msg.end() || passwordIt == msg.end()) {
        LOG_ERROR << "Invalid login request: missing username or password";
        std::string errorMsg = std::to_string(static_cast<int>(MessageType::LOGIN_RESPONSE)) + 
                             ":status=1;errorMsg=Missing username or password";
        conn->send(errorMsg);
        return;
    }
    
    std::string username = usernameIt->second;
    std::string password = passwordIt->second;
    
    LOG_INFO << "Login request from user: " << username;
    
    // 验证用户登录
    if (UserModel::getInstance().verifyLogin(username, password))
    {
        // 获取用户信息
        std::shared_ptr<User> user = UserModel::getInstance().getUserByName(username);
        
        if (user)
        {
            // 检查用户是否已经在其他客户端登录
            auto it = userConnectionMap_.find(user->getId());
            if (it != userConnectionMap_.end())
            {
                // 用户已经在其他客户端登录，需要通知之前的客户端被踢下线
                std::string kickOutMsg = std::to_string(static_cast<int>(MessageType::ERROR)) + 
                                      ":errorMsg=Your account logged in elsewhere";
                
                it->second->send(kickOutMsg);
                
                // 更新连接映射
                it->second = conn;
            }
            else
            {
                // 添加用户连接映射
                userConnectionMap_[user->getId()] = conn;
            }
            
            // 更新用户在线状态在Redis中
            RedisService::getInstance().setUserOnline(user->getId(), true);
            
            // 构建登录成功响应
            std::stringstream response;
            response << static_cast<int>(MessageType::LOGIN_RESPONSE) << ":";
            response << "status=0;"; // 0表示成功
            response << "userId=" << user->getId() << ";";
            response << "username=" << user->getUsername() << ";";
            response << "email=" << user->getEmail();
            
            // 头像可能为空
            if (!user->getAvatar().empty()) {
                response << ";avatar=" << user->getAvatar();
            }
            
            // 检查并发送离线消息
            int offlineMsgCount = RedisService::getInstance().getOfflineMessageCount(user->getId());
            if (offlineMsgCount > 0) {
                response << ";offlineMsgCount=" << offlineMsgCount;
                LOG_INFO << "User " << username << " has " << offlineMsgCount << " offline messages";
            }
            
            LOG_INFO << "User " << username << " logged in successfully";
            conn->send(response.str());
            
            // 发送离线消息
            if (offlineMsgCount > 0) {
                // 延迟一小段时间后发送离线消息，确保客户端已准备好接收
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                
                std::vector<std::string> offlineMessages = RedisService::getInstance().getOfflineMessages(user->getId());
                for (const auto& msgJson : offlineMessages) {
                    try {
                        // 解析JSON消息
                        Json::Value msg;
                        Json::Reader reader;
                        if (reader.parse(msgJson, msg)) {
                            std::string type = msg["type"].asString();
                            
                            if (type == "private") {
                                int fromUserId = msg["from"].asInt();
                                auto fromUser = UserModel::getInstance().getUserById(fromUserId);
                                if (!fromUser) continue;
                                
                                // 构建私聊消息
                                std::string privateMessage = std::to_string(static_cast<int>(MessageType::PRIVATE_CHAT)) + 
                                                  ":fromUserId=" + std::to_string(fromUserId) + 
                                                  ";fromUsername=" + fromUser->getUsername() + 
                                                  ";content=" + msg["content"].asString() + 
                                                  ";timestamp=" + msg["timestamp"].asString() +
                                                  ";offline=true";
                                
                                conn->send(privateMessage);
                            } 
                            else if (type == "group") {
                                int fromUserId = msg["from"].asInt();
                                int groupId = msg["group"].asInt();
                                auto fromUser = UserModel::getInstance().getUserById(fromUserId);
                                if (!fromUser) continue;
                                
                                // 构建群聊消息
                                std::string groupMessage = std::to_string(static_cast<int>(MessageType::GROUP_CHAT)) + 
                                                ":groupId=" + std::to_string(groupId) + 
                                                ";fromUserId=" + std::to_string(fromUserId) + 
                                                ";fromUsername=" + fromUser->getUsername() + 
                                                ";content=" + msg["content"].asString() + 
                                                ";timestamp=" + msg["timestamp"].asString() +
                                                ";offline=true";
                                
                                conn->send(groupMessage);
                            }
                        }
                    } catch (const std::exception& e) {
                        LOG_ERROR << "Failed to process offline message: " << e.what();
                    }
                }
                
                LOG_INFO << "Sent " << offlineMessages.size() << " offline messages to user " << username;
            }
        }
        else
        {
            // 用户信息获取失败
            std::string errorMsg = std::to_string(static_cast<int>(MessageType::LOGIN_RESPONSE)) + 
                                ":status=1;errorMsg=System error, please try again later";
            conn->send(errorMsg);
            
            LOG_ERROR << "Failed to retrieve user information for " << username;
        }
    }
    else
    {
        // 登录验证失败
        std::string errorMsg = std::to_string(static_cast<int>(MessageType::LOGIN_RESPONSE)) + 
                            ":status=1;errorMsg=Invalid username or password";
        conn->send(errorMsg);
        
        LOG_INFO << "Login failed for user: " << username;
    }
}

void ChatServer::handleLogout(const muduo::net::TcpConnectionPtr& conn, 
                          const std::unordered_map<std::string, std::string>& msg)
{
    auto userIdIt = msg.find("userId");
    
    if (userIdIt == msg.end()) {
        LOG_ERROR << "Invalid logout request: missing userId";
        std::string errorMsg = std::to_string(static_cast<int>(MessageType::LOGOUT_RESPONSE)) + 
                             ":status=1;errorMsg=Missing userId";
        conn->send(errorMsg);
        return;
    }
    
    int userId = std::stoi(userIdIt->second);
    
    LOG_INFO << "Logout request from user: " << userId;
    
    // 更新用户在线状态在数据库中
    UserModel::getInstance().updateUserOnlineState(userId, false);
    
    // 更新用户在线状态在Redis中
    RedisService::getInstance().setUserOnline(userId, false);
    
    // 从映射表中移除
    userConnectionMap_.erase(userId);
    
    // 创建并发送响应消息
    std::string response = std::to_string(static_cast<int>(MessageType::LOGOUT_RESPONSE)) + 
                        ":status=0";  // 0表示成功
    conn->send(response);
    
    LOG_INFO << "User " << userId << " logged out successfully";
}

void ChatServer::handleHeartbeat(const muduo::net::TcpConnectionPtr& conn, 
                              const std::unordered_map<std::string, std::string>& /* msg */)
{
    // 更新连接最后活动时间
    connectionLastActiveTime_[conn] = muduo::Timestamp::now();
    
    // 心跳响应格式：7:timestamp=当前时间戳
    std::string response = std::to_string(static_cast<int>(MessageType::HEARTBEAT_RESPONSE)) +
                          ":timestamp=" + 
                          std::to_string(muduo::Timestamp::now().microSecondsSinceEpoch());
    
    conn->send(response);
    
    LOG_DEBUG << "Heartbeat from " << conn->peerAddress().toIpPort();
}

void ChatServer::checkHeartbeats()
{
    muduo::Timestamp now = muduo::Timestamp::now();
    
    LOG_DEBUG << "Checking heartbeats for " << connectionLastActiveTime_.size() << " connections";
    
    // 检查所有连接的最后活动时间
    auto it = connectionLastActiveTime_.begin();
    while (it != connectionLastActiveTime_.end()) {
        muduo::Timestamp lastActive = it->second;
        double inactive = muduo::timeDifference(now, lastActive);
        
        // 如果超时，关闭连接
        if (inactive > HEARTBEAT_TIMEOUT) {
            LOG_INFO << "Connection timeout: " << it->first->peerAddress().toIpPort()
                     << ", inactive for " << inactive << " seconds";
            
            // 强制关闭连接 - 这将触发 onConnection 回调，进行资源清理
            it->first->forceClose();
            
            // 移除这个连接记录
            it = connectionLastActiveTime_.erase(it);
        } else {
            ++it;
        }
    }
}
// 处理验证码请求
void ChatServer::handleVerifyCodeRequest(const muduo::net::TcpConnectionPtr& conn, 
                                      const std::unordered_map<std::string, std::string>& msg) {
    auto emailIt = msg.find("email");
    if (emailIt == msg.end()) {
        LOG_ERROR << "Invalid verification code request: missing email";
        std::string errorMsg = std::to_string(static_cast<int>(MessageType::ERROR)) + 
                             ":errorMsg=Missing email address";
        conn->send(errorMsg);
        return;
    }
    
    std::string email = emailIt->second;
    LOG_INFO << "Verification code requested for email: " << email;
    
    // 检查邮箱格式
    if (email.find('@') == std::string::npos) {
        LOG_ERROR << "Invalid email format: " << email;
        std::string errorMsg = std::to_string(static_cast<int>(MessageType::ERROR)) + 
                             ":errorMsg=Invalid email format";
        conn->send(errorMsg);
        return;
    }
    
    // 检查邮箱是否已存在
    if (UserModel::getInstance().isEmailExists(email)) {
        LOG_ERROR << "Email already registered: " << email;
        std::string errorMsg = std::to_string(static_cast<int>(MessageType::ERROR)) + 
                             ":errorMsg=Email already registered";
        conn->send(errorMsg);
        return;
    }
    
    // 生成验证码
    std::string code = VerificationCodeService::getInstance().generateCode(email);
    
    // 构建邮件内容
    std::string subject = "聊天服务器注册验证码";
    std::string content = "您的注册验证码为: " + code + "\n\n"
                        "该验证码在10分钟内有效。\n\n"
                        "如果不是您本人操作，请忽略此邮件。\n\n"
                        "此邮件为系统自动发送，请勿回复。";
    
    // 尝试直接发送邮件，确保连接可靠
    bool success = EmailService::getInstance().sendEmail(email, subject, content);
    
    std::string response;
    if (!success) {
        LOG_ERROR << "Failed to send verification code email to " << email;
        response = std::to_string(static_cast<int>(MessageType::VERIFY_CODE_RESPONSE)) + 
                 ":status=1;message=Failed to send verification code, please try again later";
    } else {
        LOG_INFO << "Verification code email sent to " << email;
        response = std::to_string(static_cast<int>(MessageType::VERIFY_CODE_RESPONSE)) + 
                 ":status=0;message=Verification code has been sent to your email";
    }
    
    // 发送响应
    conn->send(response);
}

// 处理注册请求
void ChatServer::handleRegister(const muduo::net::TcpConnectionPtr& conn, 
                             const std::unordered_map<std::string, std::string>& msg) {
    // 检查必要的参数
    auto usernameIt = msg.find("username");
    auto passwordIt = msg.find("password");
    auto emailIt = msg.find("email");
    auto codeIt = msg.find("code");
    
    if (usernameIt == msg.end() || passwordIt == msg.end() || 
        emailIt == msg.end() || codeIt == msg.end()) {
        LOG_ERROR << "Invalid registration request: missing required parameters";
        std::string errorMsg = std::to_string(static_cast<int>(MessageType::REGISTER_RESPONSE)) + 
                             ":status=1;errorMsg=Missing required parameters";
        conn->send(errorMsg);
        return;
    }
    
    std::string username = usernameIt->second;
    std::string password = passwordIt->second;
    std::string email = emailIt->second;
    std::string code = codeIt->second;
    
    LOG_INFO << "Registration request for user: " << username << ", email: " << email;
    
    // 验证码检查
    if (!VerificationCodeService::getInstance().verifyCode(email, code)) {
        LOG_ERROR << "Invalid or expired verification code for " << email;
        std::string errorMsg = std::to_string(static_cast<int>(MessageType::REGISTER_RESPONSE)) + 
                             ":status=1;errorMsg=Invalid or expired verification code";
        conn->send(errorMsg);
        return;
    }
    
    // 检查用户名是否已存在
    if (UserModel::getInstance().isUserExists(username)) {
        LOG_ERROR << "Username already exists: " << username;
        std::string errorMsg = std::to_string(static_cast<int>(MessageType::REGISTER_RESPONSE)) + 
                             ":status=1;errorMsg=Username already exists";
        conn->send(errorMsg);
        return;
    }
    
    // 检查邮箱是否已存在
    if (UserModel::getInstance().isEmailExists(email)) {
        LOG_ERROR << "Email already exists: " << email;
        std::string errorMsg = std::to_string(static_cast<int>(MessageType::REGISTER_RESPONSE)) + 
                             ":status=1;errorMsg=Email already exists";
        conn->send(errorMsg);
        return;
    }
    
    // 默认头像URL
    std::string avatar = "";
    auto avatarIt = msg.find("avatar");
    if (avatarIt != msg.end()) {
        avatar = avatarIt->second;
    }
    
    // 注册用户
    bool success = UserModel::getInstance().registerUser(username, password, email, avatar);
    
    if (success) {
        LOG_INFO << "User registered successfully: " << username;
        
        // 发送邮件通知用户注册成功
        std::string subject = "欢迎注册聊天服务器";
        std::string content = "亲爱的 " + username + "，\n\n"
                            "欢迎注册我们的聊天服务！您的账号已经成功创建。\n\n"
                            "用户名: " + username + "\n"
                            "邮箱: " + email + "\n\n"
                            "您现在可以使用您的用户名和密码登录系统了。\n\n"
                            "祝您使用愉快！\n\n"
                            "聊天服务器团队";
        
        // 异步发送邮件
        std::thread([email, subject, content]() {
            EmailService::getInstance().sendEmail(email, subject, content);
        }).detach();
        
        // 发送注册成功响应
        std::string response = std::to_string(static_cast<int>(MessageType::REGISTER_RESPONSE)) + 
                            ":status=0;username=" + username + ";email=" + email;
        conn->send(response);
        
        // 获取用户信息并登录用户
        std::shared_ptr<User> user = UserModel::getInstance().getUserByName(username);
        if (user) {
            // 将用户添加到连接映射
            userConnectionMap_[user->getId()] = conn;
            
            // 发送登录成功响应
            std::stringstream loginResponse;
            loginResponse << static_cast<int>(MessageType::LOGIN_RESPONSE) << ":";
            loginResponse << "status=0;"; // 0表示成功
            loginResponse << "userId=" << user->getId() << ";";
            loginResponse << "username=" << user->getUsername() << ";";
            loginResponse << "email=" << user->getEmail();
            
            if (!user->getAvatar().empty()) {
                loginResponse << ";avatar=" << user->getAvatar();
            }
            
            conn->send(loginResponse.str());
        }
    } else {
        LOG_ERROR << "Failed to register user: " << username;
        
        // 发送注册失败响应
        std::string errorMsg = std::to_string(static_cast<int>(MessageType::REGISTER_RESPONSE)) + 
                             ":status=1;errorMsg=Registration failed, please try again later";
        conn->send(errorMsg);
    }
}
