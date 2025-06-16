#include "VerificationCodeService.h"
#include <muduo/base/Logging.h>
#include <algorithm>

VerificationCodeService& VerificationCodeService::getInstance() {
    static VerificationCodeService instance;
    return instance;
}

VerificationCodeService::VerificationCodeService() {
    // 使用随机设备初始化随机数生成器
    std::random_device rd;
    rng_ = std::mt19937(rd());
}

VerificationCodeService::~VerificationCodeService() {
}

std::string VerificationCodeService::generateCode(const std::string& email) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 生成验证码
    std::string code;
    std::uniform_int_distribution<> dist(0, 9);
    for (int i = 0; i < CODE_LENGTH; ++i) {
        code += std::to_string(dist(rng_));
    }
    
    // 设置过期时间
    auto expireTime = std::chrono::steady_clock::now() + std::chrono::minutes(CODE_EXPIRY_MINUTES);
    
    // 存储验证码
    codes_[email] = {code, expireTime};
    
    LOG_INFO << "Generated verification code for " << email << ": " << code;
    
    return code;
}

bool VerificationCodeService::verifyCode(const std::string& email, const std::string& code) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = codes_.find(email);
    if (it == codes_.end()) {
        LOG_INFO << "No verification code found for " << email;
        return false;
    }
    
    // 检查是否过期
    auto now = std::chrono::steady_clock::now();
    if (now > it->second.expireTime) {
        LOG_INFO << "Verification code for " << email << " has expired";
        codes_.erase(it);
        return false;
    }
    
    // 检查验证码是否匹配
    bool isValid = (it->second.code == code);
    
    // 验证成功后移除验证码，防止重复使用
    if (isValid) {
        LOG_INFO << "Verification code for " << email << " is valid";
        codes_.erase(it);
    } else {
        LOG_INFO << "Invalid verification code for " << email << ": " << code;
    }
    
    return isValid;
}

void VerificationCodeService::cleanupExpiredCodes() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto it = codes_.begin();
    while (it != codes_.end()) {
        if (now > it->second.expireTime) {
            LOG_INFO << "Removing expired verification code for " << it->first;
            it = codes_.erase(it);
        } else {
            ++it;
        }
    }
}
