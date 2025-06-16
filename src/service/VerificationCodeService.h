#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <random>

// 验证码服务类
class VerificationCodeService {
public:
    // 单例模式
    static VerificationCodeService& getInstance();
    
    // 生成验证码
    std::string generateCode(const std::string& email);
    
    // 验证验证码
    bool verifyCode(const std::string& email, const std::string& code);
    
    // 清理过期的验证码
    void cleanupExpiredCodes();
    
private:
    VerificationCodeService();
    ~VerificationCodeService();
    
    // 禁止拷贝和赋值
    VerificationCodeService(const VerificationCodeService&) = delete;
    VerificationCodeService& operator=(const VerificationCodeService&) = delete;
    
    // 验证码条目
    struct CodeEntry {
        std::string code;
        std::chrono::steady_clock::time_point expireTime;
    };
    
    // 验证码存储
    std::unordered_map<std::string, CodeEntry> codes_;
    std::mutex mutex_;
    
    // 验证码长度
    static constexpr int CODE_LENGTH = 6;
    
    // 验证码有效期（分钟）
    static constexpr int CODE_EXPIRY_MINUTES = 10;
    
    // 随机数生成器
    std::mt19937 rng_;
};
