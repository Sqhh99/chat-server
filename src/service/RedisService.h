#pragma once

#include <string>
#include <vector>
#include <memory>
#include <sw/redis++/redis++.h>
#include <muduo/base/Logging.h>

class RedisService {
public:
    // 单例模式
    static RedisService& getInstance();
    
    // 初始化Redis服务
    bool init(const std::string& host = "127.0.0.1", 
             int port = 6379, 
             const std::string& password = "",
             int db = 0);
    
    // 发送私聊消息
    bool sendPrivateMessage(int fromUserId, int toUserId, const std::string& content);
    
    // 发送群聊消息
    bool sendGroupMessage(int fromUserId, int groupId, const std::string& content);
    
    // 获取私聊历史消息
    std::vector<std::string> getPrivateMessages(int userId1, int userId2, int count = 20);
    
    // 获取群聊历史消息
    std::vector<std::string> getGroupMessages(int groupId, int count = 20);
    
    // 获取用户的所有私聊对话
    std::vector<int> getUserChats(int userId);
    
    // 获取用户加入的群组
    std::vector<int> getUserGroups(int userId);
    
    // 创建群组
    bool createGroup(int groupId, const std::string& groupName, int creatorId);
    
    // 加入群组
    bool joinGroup(int userId, int groupId);
    
    // 离开群组
    bool leaveGroup(int userId, int groupId);
    
    // 获取群组成员
    std::vector<int> getGroupMembers(int groupId);
    
    // 设置用户在线状态
    bool setUserOnline(int userId, bool online);
    
    // 检查用户是否在线
    bool isUserOnline(int userId);
    
    // 获取所有在线用户
    std::vector<int> getOnlineUsers();
    
    // 发送好友请求
    bool sendFriendRequest(int fromUserId, int toUserId);
    
    // 接受好友请求
    bool acceptFriendRequest(int fromUserId, int toUserId);
    
    // 拒绝好友请求
    bool rejectFriendRequest(int fromUserId, int toUserId);
    
    // 获取收到的好友请求列表
    std::vector<int> getReceivedFriendRequests(int userId);
    
    // 获取发出的好友请求列表
    std::vector<int> getSentFriendRequests(int userId);
    
    // 检查好友请求是否存在
    bool hasFriendRequest(int fromUserId, int toUserId);
    
    // 添加好友（已通过验证）
    bool addFriend(int userId1, int userId2);
    
    // 移除好友
    bool removeFriend(int userId1, int userId2);
    
    // 获取用户好友列表
    std::vector<int> getUserFriends(int userId);
    
    // 检查两个用户是否是好友
    bool isFriend(int userId1, int userId2);
    
    // 标记消息为已读
    bool markMessageAsRead(int userId, const std::string& messageId);
    
    // 获取并清除用户离线消息
    std::vector<std::string> getOfflineMessages(int userId);
    
    // 检查用户是否有离线消息
    bool hasOfflineMessages(int userId);
    
    // 获取离线消息计数
    int getOfflineMessageCount(int userId);
    
    // 标记群组消息为已读
    bool markGroupMessageAsRead(int userId, int groupId, const std::string& messageId);
    
    // 撤回私聊消息
    bool recallPrivateMessage(int userId, int targetUserId, const std::string& messageId);
    
    // 撤回群组消息
    bool recallGroupMessage(int userId, int groupId, const std::string& messageId);

    // 以下方法提供给 MessageArchiveService 使用
    // 获取所有匹配的键
    std::vector<std::string> getKeys(const std::string& pattern);
    
    // 获取列表中的所有元素
    std::vector<std::string> getAllListItems(const std::string& key);
    
    // 获取指定范围的列表元素
    std::vector<std::string> getListRange(const std::string& key, long long start, long long stop);
    
    // 设置键值
    bool setValue(const std::string& key, const std::string& value);
    
    // 获取键值
    std::string getValue(const std::string& key, const std::string& defaultValue = "");
    
    // 检查键是否存在
    bool keyExists(const std::string& key);
    
    // 检查键是否是列表类型
    bool isListType(const std::string& key);
    
    // 获取键的类型
    std::string getKeyType(const std::string& key);

    // 删除键
    bool delKey(const std::string& key);

    // 裁剪列表
    bool trimList(const std::string& key, long long start, long long stop);

private:
    RedisService();
    ~RedisService();
    
    // 禁止拷贝和赋值
    RedisService(const RedisService&) = delete;
    RedisService& operator=(const RedisService&) = delete;
    
    // Redis连接
    std::unique_ptr<sw::redis::Redis> redis_;
    bool initialized_;
    
    // 生成聊天键
    std::string getChatKey(int userId1, int userId2);
    
    // 生成群组键
    std::string getGroupKey(int groupId);
    
    // 生成群组成员键
    std::string getGroupMembersKey(int groupId);
    
    // 生成用户群组键
    std::string getUserGroupsKey(int userId);
    
    // 生成用户好友键
    std::string getUserFriendsKey(int userId);
    
    // 在线用户集合键
    const std::string ONLINE_USERS_KEY = "online:users";
};
