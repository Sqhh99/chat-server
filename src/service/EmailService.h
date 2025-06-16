#pragma once

#include <string>
#include <Poco/Net/MailMessage.h>
#include <Poco/Net/SMTPClientSession.h>
#include <Poco/Net/NetException.h>
#include <memory>

class EmailService {
public:
    // 单例模式
    static EmailService& getInstance();

    // 初始化邮件服务配置
    bool init(const std::string& smtpServer, int port, 
              const std::string& username, const std::string& password,
              const std::string& senderEmail, const std::string& senderName);

    // 发送普通文本邮件
    bool sendEmail(const std::string& recipient, const std::string& subject, const std::string& content);

    // 发送HTML格式邮件
    bool sendHtmlEmail(const std::string& recipient, const std::string& subject, const std::string& htmlContent);

private:
    EmailService();
    ~EmailService();
    
    // 禁止拷贝和赋值
    EmailService(const EmailService&) = delete;
    EmailService& operator=(const EmailService&) = delete;

    std::string smtpServer_;
    int port_;
    std::string username_;
    std::string password_;
    std::string senderEmail_;
    std::string senderName_;
    bool useSSL_;

    // 创建基本邮件
    void createBasicMessage(Poco::Net::MailMessage& message, const std::string& recipient, const std::string& subject);
};
