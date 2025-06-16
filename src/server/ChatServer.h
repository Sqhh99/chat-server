#ifndef CHAT_SERVER_H
#define CHAT_SERVER_H

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include "muduo/net/TcpServer.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/base/Logging.h"
#include "muduo/base/Timestamp.h"
#include <json/json.h>

#include "../model/User.h"
#include "../service/RedisService.h"

// 消息类型
enum class MessageType {
    LOGIN_REQUEST = 1,     // 登录请求
    LOGIN_RESPONSE = 2,    // 登录响应
    LOGOUT_REQUEST = 3,    // 登出请求
    LOGOUT_RESPONSE = 4,   // 登出响应
    ERROR = 5,             // 错误消息
    HEARTBEAT_REQUEST = 6, // 心跳请求
    HEARTBEAT_RESPONSE = 7, // 心跳响应
    REGISTER_REQUEST = 8,  // 注册请求
    REGISTER_RESPONSE = 9, // 注册响应
    VERIFY_CODE_REQUEST = 10, // 验证码请求
    VERIFY_CODE_RESPONSE = 11, // 验证码响应
    PRIVATE_CHAT = 12,     // 私聊消息
    GROUP_CHAT = 13,       // 群聊消息
    CREATE_GROUP = 14,     // 创建群组
    CREATE_GROUP_RESPONSE = 15, // 创建群组响应
    JOIN_GROUP = 16,       // 加入群组
    JOIN_GROUP_RESPONSE = 17, // 加入群组响应
    LEAVE_GROUP = 18,      // 离开群组
    LEAVE_GROUP_RESPONSE = 19, // 离开群组响应
    GET_USER_LIST = 20,    // 获取用户列表
    USER_LIST_RESPONSE = 21, // 用户列表响应
    GET_GROUP_LIST = 22,   // 获取群组列表
    GROUP_LIST_RESPONSE = 23, // 群组列表响应
    GET_GROUP_MEMBERS = 24, // 获取群成员
    GROUP_MEMBERS_RESPONSE = 25, // 群成员响应
    GET_USER_FRIENDS = 26, // 获取好友列表
    USER_FRIENDS_RESPONSE = 27, // 好友列表响应
    ADD_FRIEND = 28,       // 添加好友
    ADD_FRIEND_RESPONSE = 29, // 添加好友响应
    GET_CHAT_HISTORY = 30, // 获取聊天记录
    CHAT_HISTORY_RESPONSE = 31, // 聊天记录响应
    RECALL_MESSAGE = 32,   // 撤回消息
    RECALL_MESSAGE_RESPONSE = 33, // 撤回消息响应
    MARK_MESSAGE_READ = 34, // 标记消息已读
    MARK_MESSAGE_READ_RESPONSE = 35, // 标记消息已读响应
    FILE_MESSAGE = 36,     // 文件消息
    FILE_MESSAGE_RESPONSE = 37, // 文件消息响应
    IMAGE_MESSAGE = 38,    // 图片消息
    IMAGE_MESSAGE_RESPONSE = 39 // 图片消息响应
};

// 聊天服务器类
class ChatServer {
public:
    ChatServer(muduo::net::EventLoop* loop, 
               const muduo::net::InetAddress& listenAddr, 
               const std::string& nameArg);
    
    // 启动服务器
    void start();
    
    // 停止服务器
    void stop();
    
    // 添加辅助函数，生成紧凑格式的JSON字符串
    static std::string compactJsonString(const Json::Value& value) {
        Json::StreamWriterBuilder writer;
        writer["indentation"] = ""; // 设置为紧凑输出，无缩进
        return Json::writeString(writer, value);
    }
    
private:
    // 连接回调
    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    
    // 消息回调
    void onMessage(const muduo::net::TcpConnectionPtr& conn,
                  muduo::net::Buffer* buffer,
                  muduo::Timestamp /* time */);
    
    // 处理登录消息
    void handleLogin(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg);
    
    // 处理登出消息
    void handleLogout(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg);
    
    // 处理验证码请求
    void handleVerifyCodeRequest(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg);
    
    // 处理注册请求
    void handleRegister(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg);
    
    // 处理心跳消息
    void handleHeartbeat(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& /* msg */);
    
    // 定期检查心跳
    void checkHeartbeats();
    
    // 处理私聊消息
    void handlePrivateChat(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg);
    
    // 处理群聊消息
    void handleGroupChat(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg);
    
    // 处理创建群组
    void handleCreateGroup(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg);
    
    // 处理加入群组
    void handleJoinGroup(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg);
    
    // 处理离开群组
    void handleLeaveGroup(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg);
    
    // 处理获取用户列表
    void handleGetUserList(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg);
    
    // 处理获取群组列表
    void handleGetGroupList(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg);
    
    // 处理获取群成员
    void handleGetGroupMembers(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg);
    
    // 处理获取好友列表
    void handleGetUserFriends(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg);
    
    // 处理添加好友
    void handleAddFriend(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg);
    
    // 处理获取聊天记录
    void handleGetChatHistory(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg);
    
    // 处理撤回消息
    void handleRecallMessage(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg);
    
    // 处理标记消息已读
    void handleMarkMessageRead(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg);
    
    // 查找用户ID通过连接
    int getUserIdByConnection(const muduo::net::TcpConnectionPtr& conn);
    
    // 查找用户连接通过ID
    muduo::net::TcpConnectionPtr getConnectionByUserId(int userId);
    
    // 消息分发
    using MessageHandler = std::function<void(const muduo::net::TcpConnectionPtr&, const std::unordered_map<std::string, std::string>&)>;
    std::unordered_map<int, MessageHandler> msgHandlerMap_;
    
    // TCP服务器
    muduo::net::TcpServer server_;
    
    // 事件循环指针
    muduo::net::EventLoop* loop_;
    
    // 用户连接映射表 用户id -> 连接
    std::unordered_map<int, muduo::net::TcpConnectionPtr> userConnectionMap_;
    
    // 连接最后活动时间映射
    std::unordered_map<muduo::net::TcpConnectionPtr, muduo::Timestamp> connectionLastActiveTime_;
    
    // 心跳检查定时器ID
    muduo::net::TimerId heartbeatTimerId_;
    
    // 心跳超时时间（秒）
    static constexpr int HEARTBEAT_TIMEOUT = 60;
    
    // 心跳检查间隔（秒）
    static constexpr int HEARTBEAT_CHECK_INTERVAL = 20;
};

#endif // CHAT_SERVER_H
