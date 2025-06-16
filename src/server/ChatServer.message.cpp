#include "ChatServer.h"
#include "../service/RedisService.h"
#include "../model/UserModel.h"
#include <json/json.h>

// 处理撤回消息
void ChatServer::handleRecallMessage(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg) {
    // 获取发送者ID
    int userId = getUserIdByConnection(conn);
    if (userId == -1) {
        LOG_ERROR << "User not logged in. Cannot recall message.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=You must be logged in to recall messages");
        return;
    }
    
    // 获取消息ID和类型
    auto messageIdIt = msg.find("messageId");
    auto typeIt = msg.find("type");
    
    if (messageIdIt == msg.end() || typeIt == msg.end()) {
        LOG_ERROR << "Invalid recall message request. Missing messageId or type.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid request format");
        return;
    }
    
    std::string messageId = messageIdIt->second;
    std::string type = typeIt->second;
    
    // 更新连接活动时间
    connectionLastActiveTime_[conn] = muduo::Timestamp::now();
    
    bool success = false;
    
    if (type == "private") {
        // 获取目标用户ID
        auto targetUserIdIt = msg.find("targetUserId");
        if (targetUserIdIt == msg.end()) {
            LOG_ERROR << "Invalid recall message request. Missing targetUserId for private message.";
            conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid request format");
            return;
        }
        
        int targetUserId = std::stoi(targetUserIdIt->second);
        
        // 撤回私聊消息
        success = RedisService::getInstance().recallPrivateMessage(userId, targetUserId, messageId);
        
        if (success) {
            // 通知目标用户
            auto targetConn = getConnectionByUserId(targetUserId);
            if (targetConn && targetConn->connected()) {
                // 构建撤回通知
                std::string recallNotice = std::to_string(static_cast<int>(MessageType::RECALL_MESSAGE_RESPONSE)) + 
                                        ":messageId=" + messageId +
                                        ";type=private" +
                                        ";fromUserId=" + std::to_string(userId);
                
                targetConn->send(recallNotice);
            }
        }
    } else if (type == "group") {
        // 获取群组ID
        auto groupIdIt = msg.find("groupId");
        if (groupIdIt == msg.end()) {
            LOG_ERROR << "Invalid recall message request. Missing groupId for group message.";
            conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid request format");
            return;
        }
        
        int groupId = std::stoi(groupIdIt->second);
        
        // 撤回群聊消息
        success = RedisService::getInstance().recallGroupMessage(userId, groupId, messageId);
        
        if (success) {
            // 通知群组所有成员
            std::vector<int> members = RedisService::getInstance().getGroupMembers(groupId);
            
            // 构建撤回通知
            std::string recallNotice = std::to_string(static_cast<int>(MessageType::RECALL_MESSAGE_RESPONSE)) + 
                                    ":messageId=" + messageId +
                                    ";type=group" +
                                    ";groupId=" + std::to_string(groupId) +
                                    ";fromUserId=" + std::to_string(userId);
            
            for (int memberId : members) {
                if (memberId == userId) continue;  // 跳过发送者自己
                
                auto memberConn = getConnectionByUserId(memberId);
                if (memberConn && memberConn->connected()) {
                    memberConn->send(recallNotice);
                }
            }
        }
    } else {
        LOG_ERROR << "Invalid message type for recall: " << type;
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid message type");
        return;
    }
    
    // 发送结果给请求者
    if (success) {
        std::string response = std::to_string(static_cast<int>(MessageType::RECALL_MESSAGE_RESPONSE)) + 
                            ":status=success" +
                            ";messageId=" + messageId;
        conn->send(response);
        LOG_INFO << "Message " << messageId << " recalled successfully by user " << userId;
    } else {
        std::string response = std::to_string(static_cast<int>(MessageType::ERROR)) + 
                            ":message=Failed to recall message" +
                            ";messageId=" + messageId;
        conn->send(response);
        LOG_ERROR << "Failed to recall message " << messageId << " by user " << userId;
    }
}

// 处理标记消息已读
void ChatServer::handleMarkMessageRead(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg) {
    // 获取发送者ID
    int userId = getUserIdByConnection(conn);
    if (userId == -1) {
        LOG_ERROR << "User not logged in. Cannot mark message as read.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=You must be logged in to mark messages as read");
        return;
    }
    
    // 获取消息ID和类型
    auto messageIdIt = msg.find("messageId");
    auto typeIt = msg.find("type");
    
    if (messageIdIt == msg.end() || typeIt == msg.end()) {
        LOG_ERROR << "Invalid mark read request. Missing messageId or type.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid request format");
        return;
    }
    
    std::string messageId = messageIdIt->second;
    std::string type = typeIt->second;
    
    // 更新连接活动时间
    connectionLastActiveTime_[conn] = muduo::Timestamp::now();
    
    bool success = false;
    
    if (type == "private") {
        // 标记私聊消息为已读
        success = RedisService::getInstance().markMessageAsRead(userId, messageId);
        
        // 通知发送者消息已读
        if (success) {
            // 获取消息发送者ID
            // 这里需要从Redis获取消息数据，假设已经实现了一个方法来获取消息数据
            // 在实际中，需要根据messageId从Redis或数据库获取消息详情
            // 这里简化处理，假设targetUserId是发送者ID
            auto targetUserIdIt = msg.find("fromUserId");
            if (targetUserIdIt != msg.end()) {
                int fromUserId = std::stoi(targetUserIdIt->second);
                
                auto fromConn = getConnectionByUserId(fromUserId);
                if (fromConn && fromConn->connected()) {
                    // 构建已读通知
                    std::string readNotice = std::to_string(static_cast<int>(MessageType::MARK_MESSAGE_READ_RESPONSE)) + 
                                         ":messageId=" + messageId +
                                         ";type=private" +
                                         ";userId=" + std::to_string(userId);
                    
                    fromConn->send(readNotice);
                }
            }
        }
    } else if (type == "group") {
        // 获取群组ID
        auto groupIdIt = msg.find("groupId");
        if (groupIdIt == msg.end()) {
            LOG_ERROR << "Invalid mark read request. Missing groupId for group message.";
            conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid request format");
            return;
        }
        
        int groupId = std::stoi(groupIdIt->second);
        
        // 标记群聊消息为已读
        success = RedisService::getInstance().markGroupMessageAsRead(userId, groupId, messageId);
    } else {
        LOG_ERROR << "Invalid message type for mark read: " << type;
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid message type");
        return;
    }
    
    // 发送结果给请求者
    if (success) {
        std::string response = std::to_string(static_cast<int>(MessageType::MARK_MESSAGE_READ_RESPONSE)) + 
                            ":status=success" +
                            ";messageId=" + messageId;
        conn->send(response);
        LOG_INFO << "Message " << messageId << " marked as read by user " << userId;
    } else {
        std::string response = std::to_string(static_cast<int>(MessageType::ERROR)) + 
                            ":message=Failed to mark message as read" +
                            ";messageId=" + messageId;
        conn->send(response);
        LOG_ERROR << "Failed to mark message " << messageId << " as read by user " << userId;
    }
}
