#pragma once

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <mutex>

// 消息归档服务
class MessageArchiveService {
public:
    // 单例模式
    static MessageArchiveService& getInstance();
    
    // 初始化归档服务
    bool init();
    
    // 启动归档线程
    void start();
    
    // 停止归档线程
    void stop();
    
    // 手动触发归档
    bool archiveMessages();
    
    // 获取消息历史记录从数据库（当Redis中没有时）
    std::vector<std::string> getHistoricalMessages(int userId1, int userId2, int count = 50, int offset = 0);
    std::vector<std::string> getHistoricalGroupMessages(int groupId, int count = 50, int offset = 0);

private:
    MessageArchiveService();
    ~MessageArchiveService();
    
    // 禁止拷贝和赋值
    MessageArchiveService(const MessageArchiveService&) = delete;
    MessageArchiveService& operator=(const MessageArchiveService&) = delete;
    
    // 归档线程函数
    void archiveThread();
    
    // 归档私聊消息
    bool archivePrivateMessages();
    
    // 归档群组消息
    bool archiveGroupMessages();
    
    // 归档好友关系
    bool archiveFriendships();
    
    // 清理Redis中已归档的消息
    bool cleanupArchivedMessages(const std::string& key, long long timestamp);
    
    // 查询最后归档时间
    long long getLastArchiveTime(const std::string& key);
    
    // 更新最后归档时间
    bool updateLastArchiveTime(const std::string& key, long long timestamp);
    
    // 线程相关
    std::unique_ptr<std::thread> archiveThread_;
    std::atomic<bool> running_;
    std::mutex mutex_;
    std::condition_variable cv_;
    
    // 归档配置
    static constexpr int ARCHIVE_INTERVAL = 3600; // 默认每小时归档一次
    static constexpr int BATCH_SIZE = 1000; // 每批处理的消息数量
};
