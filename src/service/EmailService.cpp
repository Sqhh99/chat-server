#include "EmailService.h"
#include <Poco/Net/SecureSMTPClientSession.h>
#include <Poco/Net/StringPartSource.h>
#include <Poco/Net/FilePartSource.h>
#include <Poco/Net/SSLManager.h>
#include <Poco/Net/KeyConsoleHandler.h>
#include <Poco/Net/AcceptCertificateHandler.h>
#include <muduo/base/Logging.h>

EmailService& EmailService::getInstance() {
    static EmailService instance;
    return instance;
}

EmailService::EmailService() : port_(0), useSSL_(false) {
}

EmailService::~EmailService() {
}

bool EmailService::init(const std::string& smtpServer, int port, 
                        const std::string& username, const std::string& password,
                        const std::string& senderEmail, const std::string& senderName) {
    smtpServer_ = smtpServer;
    port_ = port;
    username_ = username;
    password_ = password;
    senderEmail_ = senderEmail;
    senderName_ = senderName;
    
    // 端口587或465通常表示需要使用SSL/TLS
    useSSL_ = (port == 465 || port == 587);
    
    if (useSSL_) {
        // 初始化SSL
        try {
            Poco::Net::initializeSSL();
            Poco::SharedPtr<Poco::Net::InvalidCertificateHandler> pCert = 
                new Poco::Net::AcceptCertificateHandler(false);
            Poco::Net::Context::Ptr pContext = new Poco::Net::Context(
                Poco::Net::Context::CLIENT_USE, "", "", "", 
                Poco::Net::Context::VERIFY_RELAXED, 9, true);
            Poco::Net::SSLManager::instance().initializeClient(nullptr, pCert, pContext);
        }
        catch (Poco::Exception& exc) {
            LOG_ERROR << "SSL initialization failed: " << exc.displayText();
            return false;
        }
    }
    
    LOG_INFO << "Email service initialized with SMTP server: " << smtpServer_
             << ":" << port_ << ", sender: " << senderName_ << " <" << senderEmail_ << ">";
             
    return true;
}

void EmailService::createBasicMessage(Poco::Net::MailMessage& message, const std::string& recipient, const std::string& subject) {
    message.setSender(senderEmail_);
    message.addRecipient(Poco::Net::MailRecipient(
        Poco::Net::MailRecipient::PRIMARY_RECIPIENT, recipient));
    message.setSubject(subject);
    message.setDate(Poco::Timestamp());
}

bool EmailService::sendEmail(const std::string& recipient, const std::string& subject, const std::string& content) {
    try {
        Poco::Net::MailMessage message;
        createBasicMessage(message, recipient, subject);
        message.setContent(content);
        
        if (useSSL_) {
            Poco::Net::SecureSMTPClientSession session(smtpServer_, port_);
            session.login();
            if (!username_.empty() && !password_.empty()) {
                session.login(Poco::Net::SMTPClientSession::AUTH_LOGIN, username_, password_);
            }
            session.sendMessage(message);
            session.close();
        } else {
            Poco::Net::SMTPClientSession session(smtpServer_, port_);
            session.login();
            if (!username_.empty() && !password_.empty()) {
                session.login(Poco::Net::SMTPClientSession::AUTH_LOGIN, username_, password_);
            }
            session.sendMessage(message);
            session.close();
        }
        
        LOG_INFO << "Email sent to " << recipient << " with subject: " << subject;
        return true;
    }
    catch (Poco::Exception& exc) {
        LOG_ERROR << "Failed to send email: " << exc.displayText();
        return false;
    }
    catch (std::exception& exc) {
        LOG_ERROR << "Failed to send email: " << exc.what();
        return false;
    }
    catch (...) {
        LOG_ERROR << "Unknown error occurred while sending email";
        return false;
    }
}

bool EmailService::sendHtmlEmail(const std::string& recipient, const std::string& subject, const std::string& htmlContent) {
    try {
        Poco::Net::MailMessage message;
        createBasicMessage(message, recipient, subject);
        message.setContentType("text/html");
        message.setContent(htmlContent);
        
        if (useSSL_) {
            Poco::Net::SecureSMTPClientSession session(smtpServer_, port_);
            session.login();
            if (!username_.empty() && !password_.empty()) {
                session.login(Poco::Net::SMTPClientSession::AUTH_LOGIN, username_, password_);
            }
            session.sendMessage(message);
            session.close();
        } else {
            Poco::Net::SMTPClientSession session(smtpServer_, port_);
            session.login();
            if (!username_.empty() && !password_.empty()) {
                session.login(Poco::Net::SMTPClientSession::AUTH_LOGIN, username_, password_);
            }
            session.sendMessage(message);
            session.close();
        }
        
        LOG_INFO << "HTML email sent to " << recipient << " with subject: " << subject;
        return true;
    }
    catch (Poco::Exception& exc) {
        LOG_ERROR << "Failed to send HTML email: " << exc.displayText();
        return false;
    }
}
