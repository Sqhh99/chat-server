#include "ChatServer.h"
#include "../service/RedisService.h"
#include "../service/MessageArchiveService.h"
#include "../model/UserModel.h"
#include <json/json.h>

// 查找用户ID通过连接
int ChatServer::getUserIdByConnection(const muduo::net::TcpConnectionPtr& conn) {
    for (const auto& pair : userConnectionMap_) {
        if (pair.second == conn) {
            return pair.first;
        }
    }
    return -1;
}

// 查找用户连接通过ID
muduo::net::TcpConnectionPtr ChatServer::getConnectionByUserId(int userId) {
    auto it = userConnectionMap_.find(userId);
    if (it != userConnectionMap_.end()) {
        return it->second;
    }
    return nullptr;
}

// 处理私聊消息
void ChatServer::handlePrivateChat(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg) {
    // 获取发送者ID
    int fromUserId = getUserIdByConnection(conn);
    if (fromUserId == -1) {
        LOG_ERROR << "User not logged in. Cannot send private message.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=You must be logged in to send messages");
        return;
    }
    
    // 获取接收者ID或用户名和消息内容
    auto toUserIdIt = msg.find("toUserId");
    auto contentIt = msg.find("content");
    
    if (toUserIdIt == msg.end() || contentIt == msg.end()) {
        LOG_ERROR << "Invalid private chat message format. Missing toUserId or content.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid message format");
        return;
    }
    
    int toUserId = -1;
    std::string toUserName = toUserIdIt->second;
    std::string content = contentIt->second;
    
    // 尝试将toUserId转换为整数，如果失败则认为是用户名
    try {
        toUserId = std::stoi(toUserName);
    } catch (const std::invalid_argument& e) {
        // 如果不是数字，则通过用户名查找用户ID
        auto toUser = UserModel::getInstance().getUserByName(toUserName);
        if (toUser) {
            toUserId = toUser->getId();
        } else {
            LOG_ERROR << "Cannot find user with name: " << toUserName;
            conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=User not found");
            return;
        }
    } catch (const std::out_of_range& e) {
        LOG_ERROR << "User ID is out of range";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid user ID");
        return;
    }
    
    // 更新连接活动时间
    connectionLastActiveTime_[conn] = muduo::Timestamp::now();
    
    // 检查是否为好友关系
    bool isFriend = RedisService::getInstance().isFriend(fromUserId, toUserId);
    if (!isFriend) {
        LOG_ERROR << "User " << fromUserId << " tried to send message to non-friend user " << toUserId;
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=You can only send messages to your friends");
        return;
    }
    
    // 发送消息到Redis
    bool success = RedisService::getInstance().sendPrivateMessage(fromUserId, toUserId, content);
    
    if (!success) {
        LOG_ERROR << "Failed to send private message from user " << fromUserId << " to user " << toUserId;
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Failed to send message");
        return;
    }
    
    // 获取发送者信息
    auto fromUser = UserModel::getInstance().getUserById(fromUserId);
    if (!fromUser) {
        LOG_ERROR << "Failed to get user information for userId: " << fromUserId;
        return;
    }
    
    // 构建消息
    std::string message = std::to_string(static_cast<int>(MessageType::PRIVATE_CHAT)) + 
                        ":fromUserId=" + std::to_string(fromUserId) + 
                        ";fromUsername=" + fromUser->getUsername() + 
                        ";content=" + content + 
                        ";timestamp=" + std::to_string(
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()
                            ).count()
                        );
    
    // 发送消息给接收者
    auto toConn = getConnectionByUserId(toUserId);
    if (toConn && toConn->connected()) {
        toConn->send(message);
        LOG_INFO << "Private message sent from user " << fromUserId << " to user " << toUserId;
    } else {
        LOG_INFO << "Recipient user " << toUserId << " is offline. Message stored for later delivery.";
    }
    
    // 发送确认给发送者
    conn->send(message);
}

// 处理群聊消息
void ChatServer::handleGroupChat(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg) {
    // 获取发送者ID
    int fromUserId = getUserIdByConnection(conn);
    if (fromUserId == -1) {
        LOG_ERROR << "User not logged in. Cannot send group message.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=You must be logged in to send messages");
        return;
    }
    
    // 获取群组ID和消息内容
    auto groupIdIt = msg.find("groupId");
    auto contentIt = msg.find("content");
    
    if (groupIdIt == msg.end() || contentIt == msg.end()) {
        LOG_ERROR << "Invalid group chat message format. Missing groupId or content.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid message format");
        return;
    }
    
    int groupId = -1;
    std::string groupIdStr = groupIdIt->second;
    std::string content = contentIt->second;
    
    // 尝试将groupId转换为整数，如果失败则可能是群组名称（未实现）
    try {
        groupId = std::stoi(groupIdStr);
    } catch (const std::invalid_argument& e) {
        // 未来可以实现通过群组名称查找群组ID的功能
        LOG_ERROR << "Invalid group ID format: " << groupIdStr;
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid group ID format");
        return;
    } catch (const std::out_of_range& e) {
        LOG_ERROR << "Group ID is out of range";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid group ID");
        return;
    }
    
    // 更新连接活动时间
    connectionLastActiveTime_[conn] = muduo::Timestamp::now();
    
    // 发送消息到Redis
    bool success = RedisService::getInstance().sendGroupMessage(fromUserId, groupId, content);
    
    if (!success) {
        LOG_ERROR << "Failed to send group message from user " << fromUserId << " to group " << groupId;
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Failed to send message");
        return;
    }
    
    // 获取发送者信息
    auto fromUser = UserModel::getInstance().getUserById(fromUserId);
    if (!fromUser) {
        LOG_ERROR << "Failed to get user information for userId: " << fromUserId;
        return;
    }
    
    // 构建消息
    std::string message = std::to_string(static_cast<int>(MessageType::GROUP_CHAT)) + 
                        ":groupId=" + std::to_string(groupId) + 
                        ";fromUserId=" + std::to_string(fromUserId) + 
                        ";fromUsername=" + fromUser->getUsername() + 
                        ";content=" + content + 
                        ";timestamp=" + std::to_string(
                            std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::system_clock::now().time_since_epoch()
                            ).count()
                        );
    
    // 获取群组成员列表
    std::vector<int> members = RedisService::getInstance().getGroupMembers(groupId);
    
    // 发送消息给所有在线群组成员
    for (int memberId : members) {
        if (memberId == fromUserId) continue;  // 跳过发送者自己
        
        auto memberConn = getConnectionByUserId(memberId);
        if (memberConn && memberConn->connected()) {
            memberConn->send(message);
        }
    }
    
    // 发送确认给发送者
    conn->send(message);
    
    LOG_INFO << "Group message sent from user " << fromUserId << " to group " << groupId;
}

// 处理创建群组
void ChatServer::handleCreateGroup(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg) {
    // 获取发送者ID
    int fromUserId = getUserIdByConnection(conn);
    if (fromUserId == -1) {
        LOG_ERROR << "User not logged in. Cannot create group.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=You must be logged in to create a group");
        return;
    }
    
    // 获取群组名称
    auto groupNameIt = msg.find("groupName");
    if (groupNameIt == msg.end()) {
        LOG_ERROR << "Invalid create group request. Missing groupName.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid request format");
        return;
    }
    
    std::string groupName = groupNameIt->second;
    
    // 更新连接活动时间
    connectionLastActiveTime_[conn] = muduo::Timestamp::now();
    
    // 生成一个新的群组ID (简化处理，实际应从数据库生成)
    int groupId = static_cast<int>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    ) % 1000000;  // 限制为6位数字
    
    // 创建群组
    bool success = RedisService::getInstance().createGroup(groupId, groupName, fromUserId);
    
    if (!success) {
        LOG_ERROR << "Failed to create group " << groupName << " by user " << fromUserId;
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Failed to create group");
        return;
    }
    
    // 构建响应消息
    std::string response = std::to_string(static_cast<int>(MessageType::CREATE_GROUP_RESPONSE)) + 
                         ":status=0" + 
                         ";groupId=" + std::to_string(groupId) + 
                         ";groupName=" + groupName;
    
    // 发送响应给创建者
    conn->send(response);
    
    LOG_INFO << "Group " << groupId << " (" << groupName << ") created by user " << fromUserId;
}

// 处理加入群组
void ChatServer::handleJoinGroup(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg) {
    // 获取发送者ID
    int fromUserId = getUserIdByConnection(conn);
    if (fromUserId == -1) {
        LOG_ERROR << "User not logged in. Cannot join group.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=You must be logged in to join a group");
        return;
    }
    
    // 获取群组ID
    auto groupIdIt = msg.find("groupId");
    if (groupIdIt == msg.end()) {
        LOG_ERROR << "Invalid join group request. Missing groupId.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid request format");
        return;
    }
    
    int groupId;
    try {
        groupId = std::stoi(groupIdIt->second);
    } catch (const std::exception& e) {
        LOG_ERROR << "Invalid group ID: " << groupIdIt->second << ". Expected a number.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid group ID. Please enter a numeric ID");
        return;
    }
    
    // 更新连接活动时间
    connectionLastActiveTime_[conn] = muduo::Timestamp::now();
    
    // 加入群组
    bool success = RedisService::getInstance().joinGroup(fromUserId, groupId);
    
    // 构建响应消息
    std::string response;
    if (success) {
        response = std::to_string(static_cast<int>(MessageType::JOIN_GROUP_RESPONSE)) + 
                 ":status=0" + 
                 ";groupId=" + std::to_string(groupId);
        
        LOG_INFO << "User " << fromUserId << " joined group " << groupId;
    } else {
        response = std::to_string(static_cast<int>(MessageType::JOIN_GROUP_RESPONSE)) + 
                 ":status=1" + 
                 ";message=Failed to join group";
        
        LOG_ERROR << "Failed to join group " << groupId << " by user " << fromUserId;
    }
    
    // 发送响应给用户
    conn->send(response);
}

// 处理离开群组
void ChatServer::handleLeaveGroup(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg) {
    // 获取发送者ID
    int fromUserId = getUserIdByConnection(conn);
    if (fromUserId == -1) {
        LOG_ERROR << "User not logged in. Cannot leave group.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=You must be logged in to leave a group");
        return;
    }
    
    // 获取群组ID
    auto groupIdIt = msg.find("groupId");
    if (groupIdIt == msg.end()) {
        LOG_ERROR << "Invalid leave group request. Missing groupId.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid request format");
        return;
    }
    
    int groupId;
    try {
        groupId = std::stoi(groupIdIt->second);
    } catch (const std::exception& e) {
        LOG_ERROR << "Invalid group ID: " << groupIdIt->second << ". Expected a number.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid group ID. Please enter a numeric ID");
        return;
    }
    
    // 更新连接活动时间
    connectionLastActiveTime_[conn] = muduo::Timestamp::now();
    
    // 离开群组
    bool success = RedisService::getInstance().leaveGroup(fromUserId, groupId);
    
    // 构建响应消息
    std::string response;
    if (success) {
        response = std::to_string(static_cast<int>(MessageType::LEAVE_GROUP_RESPONSE)) + 
                 ":status=0" + 
                 ";groupId=" + std::to_string(groupId);
        
        LOG_INFO << "User " << fromUserId << " left group " << groupId;
    } else {
        response = std::to_string(static_cast<int>(MessageType::LEAVE_GROUP_RESPONSE)) + 
                 ":status=1" + 
                 ";message=Failed to leave group";
        
        LOG_ERROR << "Failed to leave group " << groupId << " by user " << fromUserId;
    }
    
    // 发送响应给用户
    conn->send(response);
}

// 处理获取用户列表
void ChatServer::handleGetUserList(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& /* msg */) {
    // 获取发送者ID
    int fromUserId = getUserIdByConnection(conn);
    if (fromUserId == -1) {
        LOG_ERROR << "User not logged in. Cannot get user list.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=You must be logged in to get user list");
        return;
    }
    
    // 更新连接活动时间
    connectionLastActiveTime_[conn] = muduo::Timestamp::now();
    
    // 获取在线用户列表
    std::vector<int> onlineUsers = RedisService::getInstance().getOnlineUsers();
    
    // 构建用户列表JSON
    Json::Value userList(Json::arrayValue);
    for (int userId : onlineUsers) {
        auto user = UserModel::getInstance().getUserById(userId);
        if (user) {
            Json::Value userObj;
            userObj["id"] = userId;
            userObj["username"] = user->getUsername();
            userObj["online"] = true;
            userList.append(userObj);
        }
    }
    
    // 转换为字符串（使用紧凑格式）
    std::string userListStr = compactJsonString(userList);
    
    // 构建响应消息
    std::string response = std::to_string(static_cast<int>(MessageType::USER_LIST_RESPONSE)) + 
                         ":status=0" + 
                         ";users=" + userListStr;
    
    // 发送响应给用户
    conn->send(response);
    
    LOG_INFO << "User list sent to user " << fromUserId;
}

// 处理获取群组列表
void ChatServer::handleGetGroupList(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& /* msg */) {
    // 获取发送者ID
    int fromUserId = getUserIdByConnection(conn);
    if (fromUserId == -1) {
        LOG_ERROR << "User not logged in. Cannot get group list.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=You must be logged in to get group list");
        return;
    }
    
    // 更新连接活动时间
    connectionLastActiveTime_[conn] = muduo::Timestamp::now();
    
    // 获取用户的群组列表
    std::vector<int> userGroups = RedisService::getInstance().getUserGroups(fromUserId);
    
    // 构建群组列表JSON
    Json::Value groupList(Json::arrayValue);
    for (int groupId : userGroups) {
        Json::Value groupObj;
        groupObj["id"] = groupId;
        // TODO: 从Redis获取群组名称，这里简化处理
        groupObj["name"] = "Group-" + std::to_string(groupId);
        groupList.append(groupObj);
    }
    
    // 转换为字符串 (使用紧凑输出格式)
    std::string groupListStr = compactJsonString(groupList);
    
    // 构建响应消息
    std::string response = std::to_string(static_cast<int>(MessageType::GROUP_LIST_RESPONSE)) + 
                         ":status=0" + 
                         ";groups=" + groupListStr;
    
    // 发送响应给用户
    conn->send(response);
    
    LOG_INFO << "Group list sent to user " << fromUserId;
}

// 处理获取群成员
void ChatServer::handleGetGroupMembers(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg) {
    // 获取发送者ID
    int fromUserId = getUserIdByConnection(conn);
    if (fromUserId == -1) {
        LOG_ERROR << "User not logged in. Cannot get group members.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=You must be logged in to get group members");
        return;
    }
    
    // 获取群组ID
    auto groupIdIt = msg.find("groupId");
    if (groupIdIt == msg.end()) {
        LOG_ERROR << "Invalid get group members request. Missing groupId.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid request format");
        return;
    }
    
    int groupId;
    try {
        groupId = std::stoi(groupIdIt->second);
    } catch (const std::exception& e) {
        LOG_ERROR << "Invalid group ID: " << groupIdIt->second << ". Expected a number.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid group ID. Please enter a numeric ID");
        return;
    }
    
    // 更新连接活动时间
    connectionLastActiveTime_[conn] = muduo::Timestamp::now();
    
    // 获取群组成员列表
    std::vector<int> members = RedisService::getInstance().getGroupMembers(groupId);
    
    // 构建成员列表JSON
    Json::Value memberList(Json::arrayValue);
    for (int memberId : members) {
        auto member = UserModel::getInstance().getUserById(memberId);
        if (member) {
            Json::Value memberObj;
            memberObj["id"] = memberId;
            memberObj["username"] = member->getUsername();
            memberObj["online"] = RedisService::getInstance().isUserOnline(memberId);
            memberList.append(memberObj);
        }
    }
    
    // 转换为字符串（使用紧凑格式）
    std::string memberListStr = compactJsonString(memberList);
    
    // 构建响应消息
    std::string response = std::to_string(static_cast<int>(MessageType::GROUP_MEMBERS_RESPONSE)) + 
                         ":status=0" + 
                         ";groupId=" + std::to_string(groupId) + 
                         ";members=" + memberListStr;
    
    // 发送响应给用户
    conn->send(response);
    
    LOG_INFO << "Group " << groupId << " members list sent to user " << fromUserId;
}

// 处理获取好友列表
void ChatServer::handleGetUserFriends(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& /* msg */) {
    // 获取发送者ID
    int fromUserId = getUserIdByConnection(conn);
    if (fromUserId == -1) {
        LOG_ERROR << "User not logged in. Cannot get friends list.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=You must be logged in to get friends list");
        return;
    }
    
    // 更新连接活动时间
    connectionLastActiveTime_[conn] = muduo::Timestamp::now();
    
    // 获取好友列表
    std::vector<int> friends = RedisService::getInstance().getUserFriends(fromUserId);
    
    // 构建好友列表JSON
    Json::Value friendList(Json::arrayValue);
    for (int friendId : friends) {
        auto friendUser = UserModel::getInstance().getUserById(friendId);
        if (friendUser) {
            Json::Value friendObj;
            friendObj["id"] = friendId;
            friendObj["username"] = friendUser->getUsername();
            friendObj["online"] = RedisService::getInstance().isUserOnline(friendId);
            friendList.append(friendObj);
        }
    }
    
    // 转换为字符串（使用紧凑格式）
    std::string friendListStr = compactJsonString(friendList);
    
    // 构建响应消息
    std::string response = std::to_string(static_cast<int>(MessageType::USER_FRIENDS_RESPONSE)) + 
                         ":status=0" + 
                         ";friends=" + friendListStr;
    
    // 发送响应给用户
    conn->send(response);
    
    LOG_INFO << "Friends list sent to user " << fromUserId;
}

// 处理添加好友 (改为发送好友请求)
void ChatServer::handleAddFriend(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg) {
    // 为了向后兼容，将ADD_FRIEND消息重定向到ADD_FRIEND_REQUEST处理
    handleAddFriendRequest(conn, msg);
}

// 处理发送好友请求
void ChatServer::handleAddFriendRequest(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg) {
    // 获取发送者ID
    int fromUserId = getUserIdByConnection(conn);
    if (fromUserId == -1) {
        LOG_ERROR << "User not logged in. Cannot send friend request.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=You must be logged in to send a friend request");
        return;
    }
    
    // 获取好友ID或用户名
    auto friendIdIt = msg.find("friendId");
    if (friendIdIt == msg.end()) {
        LOG_ERROR << "Invalid friend request. Missing friendId.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid request format");
        return;
    }
    
    // 更新连接活动时间
    connectionLastActiveTime_[conn] = muduo::Timestamp::now();
    
    std::string friendIdentifier = friendIdIt->second;
    std::shared_ptr<User> friendUser;
    int friendId = -1;
    
    // 尝试将输入视为用户ID
    try {
        friendId = std::stoi(friendIdentifier);
        friendUser = UserModel::getInstance().getUserById(friendId);
    } catch (const std::exception& e) {
        // 如果转换失败，则假定输入是用户名
        LOG_INFO << "Input is not a numeric ID, trying as username: " << friendIdentifier;
        friendUser = UserModel::getInstance().getUserByName(friendIdentifier);
        if (friendUser) {
            friendId = friendUser->getId();
        }
    }
    
    // 检查好友是否存在
    if (!friendUser) {
        LOG_ERROR << "User '" << friendIdentifier << "' does not exist. Cannot send friend request.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=User does not exist");
        return;
    }
    
    // 检查是否是自己
    if (fromUserId == friendId) {
        LOG_ERROR << "User " << fromUserId << " tried to send friend request to himself.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=You cannot send friend request to yourself");
        return;
    }
    
    // 检查是否已经是好友
    if (RedisService::getInstance().isFriend(fromUserId, friendId)) {
        LOG_ERROR << "User " << fromUserId << " and user " << friendId << " are already friends.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=You are already friends with this user");
        return;
    }
    
    // 发送好友请求
    bool success = RedisService::getInstance().sendFriendRequest(fromUserId, friendId);
    
    // 构建响应消息
    std::string response;
    if (success) {
        response = std::to_string(static_cast<int>(MessageType::ADD_FRIEND_RESPONSE)) + 
                 ":status=0" + 
                 ";friendId=" + std::to_string(friendId) + 
                 ";username=" + friendUser->getUsername() +
                 ";message=Friend request sent successfully";
        
        LOG_INFO << "User " << fromUserId << " sent friend request to user " << friendId;
        
        // 如果目标用户在线，通知他有新的好友请求
        auto targetConn = getConnectionByUserId(friendId);
        if (targetConn) {
            auto senderUser = UserModel::getInstance().getUserById(fromUserId);
            if (senderUser) {
                std::string notification = std::to_string(static_cast<int>(MessageType::ADD_FRIEND_REQUEST)) + 
                                         ":fromUserId=" + std::to_string(fromUserId) +
                                         ";username=" + senderUser->getUsername() +
                                         ";message=You have a new friend request";
                targetConn->send(notification);
            }
        }
    } else {
        response = std::to_string(static_cast<int>(MessageType::ADD_FRIEND_RESPONSE)) + 
                 ":status=1" + 
                 ";message=Failed to send friend request";
        
        LOG_ERROR << "Failed to send friend request from user " << fromUserId << " to user " << friendId;
    }
    
    // 发送响应给用户
    conn->send(response);
}

// 处理接受好友请求
void ChatServer::handleAcceptFriendRequest(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg) {
    // 获取发送者ID
    int toUserId = getUserIdByConnection(conn);
    if (toUserId == -1) {
        LOG_ERROR << "User not logged in. Cannot accept friend request.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=You must be logged in to accept friend request");
        return;
    }
    
    // 获取请求发送者的ID
    auto fromUserIdIt = msg.find("fromUserId");
    if (fromUserIdIt == msg.end()) {
        LOG_ERROR << "Invalid accept friend request. Missing fromUserId.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid request format");
        return;
    }
    
    // 更新连接活动时间
    connectionLastActiveTime_[conn] = muduo::Timestamp::now();
    
    int fromUserId;
    try {
        fromUserId = std::stoi(fromUserIdIt->second);
    } catch (const std::exception& e) {
        LOG_ERROR << "Invalid fromUserId: " << fromUserIdIt->second;
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid user ID");
        return;
    }
    
    // 检查请求发送者是否存在
    auto fromUser = UserModel::getInstance().getUserById(fromUserId);
    if (!fromUser) {
        LOG_ERROR << "User " << fromUserId << " does not exist.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=User does not exist");
        return;
    }
    
    // 接受好友请求
    bool success = RedisService::getInstance().acceptFriendRequest(fromUserId, toUserId);
    
    // 构建响应消息
    std::string response;
    if (success) {
        response = std::to_string(static_cast<int>(MessageType::ACCEPT_FRIEND_RESPONSE)) + 
                 ":status=0" + 
                 ";fromUserId=" + std::to_string(fromUserId) + 
                 ";username=" + fromUser->getUsername() +
                 ";message=Friend request accepted successfully";
        
        LOG_INFO << "User " << toUserId << " accepted friend request from user " << fromUserId;
        
        // 如果请求发送者在线，通知他好友请求被接受
        auto fromConn = getConnectionByUserId(fromUserId);
        if (fromConn) {
            auto toUser = UserModel::getInstance().getUserById(toUserId);
            if (toUser) {
                std::string notification = std::to_string(static_cast<int>(MessageType::ACCEPT_FRIEND_RESPONSE)) + 
                                         ":toUserId=" + std::to_string(toUserId) +
                                         ";username=" + toUser->getUsername() +
                                         ";message=Your friend request has been accepted";
                fromConn->send(notification);
            }
        }
    } else {
        response = std::to_string(static_cast<int>(MessageType::ACCEPT_FRIEND_RESPONSE)) + 
                 ":status=1" + 
                 ";message=Failed to accept friend request";
        
        LOG_ERROR << "Failed to accept friend request from user " << fromUserId << " to user " << toUserId;
    }
    
    // 发送响应给用户
    conn->send(response);
}

// 处理拒绝好友请求
void ChatServer::handleRejectFriendRequest(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg) {
    // 获取发送者ID
    int toUserId = getUserIdByConnection(conn);
    if (toUserId == -1) {
        LOG_ERROR << "User not logged in. Cannot reject friend request.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=You must be logged in to reject friend request");
        return;
    }
    
    // 获取请求发送者的ID
    auto fromUserIdIt = msg.find("fromUserId");
    if (fromUserIdIt == msg.end()) {
        LOG_ERROR << "Invalid reject friend request. Missing fromUserId.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid request format");
        return;
    }
    
    // 更新连接活动时间
    connectionLastActiveTime_[conn] = muduo::Timestamp::now();
    
    int fromUserId;
    try {
        fromUserId = std::stoi(fromUserIdIt->second);
    } catch (const std::exception& e) {
        LOG_ERROR << "Invalid fromUserId: " << fromUserIdIt->second;
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid user ID");
        return;
    }
    
    // 检查请求发送者是否存在
    auto fromUser = UserModel::getInstance().getUserById(fromUserId);
    if (!fromUser) {
        LOG_ERROR << "User " << fromUserId << " does not exist.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=User does not exist");
        return;
    }
    
    // 拒绝好友请求
    bool success = RedisService::getInstance().rejectFriendRequest(fromUserId, toUserId);
    
    // 构建响应消息
    std::string response;
    if (success) {
        response = std::to_string(static_cast<int>(MessageType::REJECT_FRIEND_RESPONSE)) + 
                 ":status=0" + 
                 ";fromUserId=" + std::to_string(fromUserId) + 
                 ";username=" + fromUser->getUsername() +
                 ";message=Friend request rejected successfully";
        
        LOG_INFO << "User " << toUserId << " rejected friend request from user " << fromUserId;
        
        // 可以选择是否通知请求发送者被拒绝（这里不通知，避免打扰）
    } else {
        response = std::to_string(static_cast<int>(MessageType::REJECT_FRIEND_RESPONSE)) + 
                 ":status=1" + 
                 ";message=Failed to reject friend request";
        
        LOG_ERROR << "Failed to reject friend request from user " << fromUserId << " to user " << toUserId;
    }
    
    // 发送响应给用户
    conn->send(response);
}

// 处理获取好友请求列表
void ChatServer::handleGetFriendRequests(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& /* msg */) {
    // 获取发送者ID
    int userId = getUserIdByConnection(conn);
    if (userId == -1) {
        LOG_ERROR << "User not logged in. Cannot get friend requests.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=You must be logged in to get friend requests");
        return;
    }
    
    // 更新连接活动时间
    connectionLastActiveTime_[conn] = muduo::Timestamp::now();
    
    // 获取好友请求列表
    std::vector<std::pair<int, std::string>> requests = RedisService::getInstance().getFriendRequests(userId);
    
    // 构建好友请求列表JSON
    Json::Value requestList(Json::arrayValue);
    for (const auto& request : requests) {
        int requestUserId = request.first;
        auto requestUser = UserModel::getInstance().getUserById(requestUserId);
        if (requestUser) {
            Json::Value requestObj;
            requestObj["id"] = requestUserId;
            requestObj["username"] = requestUser->getUsername();
            requestObj["online"] = RedisService::getInstance().isUserOnline(requestUserId);
            requestList.append(requestObj);
        }
    }
    
    // 转换为字符串（使用紧凑格式）
    std::string requestListStr = compactJsonString(requestList);
    
    // 构建响应消息
    std::string response = std::to_string(static_cast<int>(MessageType::FRIEND_REQUESTS_RESPONSE)) + 
                         ":status=0" + 
                         ";requests=" + requestListStr;
    
    // 发送响应给用户
    conn->send(response);
    
    LOG_INFO << "Friend requests list sent to user " << userId << ", found " << requests.size() << " requests";
}

// 处理获取聊天记录
void ChatServer::handleGetChatHistory(const muduo::net::TcpConnectionPtr& conn, const std::unordered_map<std::string, std::string>& msg) {
    // 获取发送者ID
    int fromUserId = getUserIdByConnection(conn);
    if (fromUserId == -1) {
        LOG_ERROR << "User not logged in. Cannot get chat history.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=You must be logged in to get chat history");
        return;
    }
    
    // 获取聊天类型、目标ID和消息数量
    auto typeIt = msg.find("type");
    auto targetIdIt = msg.find("targetUserId");
    auto groupIdIt = msg.find("groupId");
    auto countIt = msg.find("count");
    
    if (typeIt == msg.end() || (typeIt->second == "private" && targetIdIt == msg.end()) || 
        (typeIt->second == "group" && groupIdIt == msg.end())) {
        LOG_ERROR << "Invalid get chat history request. Missing required parameters.";
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid request format");
        return;
    }
    
    std::string type = typeIt->second;
    int count = (countIt != msg.end()) ? std::stoi(countIt->second) : 20;  // 默认获取20条消息
    int offset = 0; // 可以添加分页支持
    
    // 更新连接活动时间
    connectionLastActiveTime_[conn] = muduo::Timestamp::now();
    
    if (type == "private") {
        int targetUserId = std::stoi(targetIdIt->second);
        
        // 获取私聊历史消息
        std::vector<std::string> messages = MessageArchiveService::getInstance().getHistoricalMessages(fromUserId, targetUserId, count, offset);
        
        // 如果数据库没有足够的消息，再从Redis获取最近的消息
        if (messages.size() < static_cast<size_t>(count)) {
            std::vector<std::string> recentMessages = RedisService::getInstance().getPrivateMessages(fromUserId, targetUserId, count - static_cast<int>(messages.size()));
            messages.insert(messages.end(), recentMessages.begin(), recentMessages.end());
        }
        
        // 转换为字符串
        Json::StreamWriterBuilder writer;
        std::string messagesJsonStr = Json::writeString(writer, Json::arrayValue);
        if (!messages.empty()) {
            // 构建JSON数组
            std::string jsonArrayStr = "[" + messages[0];
            for (size_t i = 1; i < messages.size(); ++i) {
                jsonArrayStr += "," + messages[i];
            }
            jsonArrayStr += "]";
            messagesJsonStr = jsonArrayStr;
        }
        
        // 构建响应消息
        std::string response = std::to_string(static_cast<int>(MessageType::CHAT_HISTORY_RESPONSE)) + 
                             ":status=0" + 
                             ";type=private" +
                             ";userId=" + std::to_string(fromUserId) +
                             ";targetId=" + std::to_string(targetUserId) +
                             ";messages=" + messagesJsonStr;
                             
        conn->send(response);
        LOG_INFO << "Sent " << messages.size() << " private chat history messages to user " << fromUserId;
    } 
    else if (type == "group") {
        int groupId = std::stoi(groupIdIt->second);
        
        // 检查用户是否是群成员
        std::vector<int> groupMembers = RedisService::getInstance().getGroupMembers(groupId);
        bool isMember = false;
        for (int memberId : groupMembers) {
            if (memberId == fromUserId) {
                isMember = true;
                break;
            }
        }
        
        if (!isMember) {
            LOG_ERROR << "User " << fromUserId << " is not a member of group " << groupId;
            conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=You are not a member of this group");
            return;
        }
        
        // 获取群聊历史消息
        MessageArchiveService& msgArchiveService = MessageArchiveService::getInstance();
        std::vector<std::string> messages = msgArchiveService.getHistoricalGroupMessages(groupId, count, offset);
        
        // 如果数据库没有足够的消息，再从Redis获取最近的消息
        if (messages.size() < static_cast<size_t>(count)) {
            std::vector<std::string> recentMessages = RedisService::getInstance().getGroupMessages(groupId, count - static_cast<int>(messages.size()));
            messages.insert(messages.end(), recentMessages.begin(), recentMessages.end());
        }
        
        // 转换为字符串
        Json::StreamWriterBuilder writer;
        std::string messagesJsonStr = Json::writeString(writer, Json::arrayValue);
        if (!messages.empty()) {
            // 构建JSON数组
            std::string jsonArrayStr = "[" + messages[0];
            for (size_t i = 1; i < messages.size(); ++i) {
                jsonArrayStr += "," + messages[i];
            }
            jsonArrayStr += "]";
            messagesJsonStr = jsonArrayStr;
        }
        
        // 构建响应消息
        std::string response = std::to_string(static_cast<int>(MessageType::CHAT_HISTORY_RESPONSE)) + 
                             ":status=0" + 
                             ";type=group" +
                             ";groupId=" + std::to_string(groupId) +
                             ";messages=" + messagesJsonStr;
        
        conn->send(response);
        LOG_INFO << "Sent " << messages.size() << " group chat history messages to user " << fromUserId;
    }
    else {
        LOG_ERROR << "Invalid chat type: " << type;
        conn->send(std::to_string(static_cast<int>(MessageType::ERROR)) + ":message=Invalid chat type");
    }
}
