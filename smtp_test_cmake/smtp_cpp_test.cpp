#include <iostream>
#include <string>
#include <Poco/Net/SecureSMTPClientSession.h>
#include <Poco/Net/MailMessage.h>
#include <Poco/Net/MailRecipient.h>
#include <Poco/Net/NetException.h>
#include <Poco/Net/SSLManager.h>
#include <Poco/Net/KeyConsoleHandler.h>
#include <Poco/Net/AcceptCertificateHandler.h>

int main(int argc, char** argv) {
    std::string smtpServer = "smtp.163.com";
    int port = 465;
    std::string username = "sqhh99@163.com";
    std::string password = "GHx58fk48fuxcw8H";
    std::string senderEmail = "sqhh99@163.com";
    std::string senderName = "C++ SMTP Test";
    
    std::string recipientEmail = "sqhh99@outlook.com";
    if (argc > 1) {
        recipientEmail = argv[1];
    }
    
    std::cout << "SMTP服务器: " << smtpServer << ":" << port << std::endl;
    std::cout << "发件人: " << senderName << " <" << senderEmail << ">" << std::endl;
    std::cout << "收件人: " << recipientEmail << std::endl;

    try {
        // 初始化SSL
        std::cout << "正在初始化SSL..." << std::endl;
        Poco::Net::initializeSSL();
        
        // 创建SSL上下文配置
        Poco::SharedPtr<Poco::Net::InvalidCertificateHandler> pCert = 
            new Poco::Net::AcceptCertificateHandler(false);
        Poco::Net::Context::Ptr pContext = new Poco::Net::Context(
            Poco::Net::Context::CLIENT_USE, "", "", "", 
            Poco::Net::Context::VERIFY_RELAXED, 9, true);
        Poco::Net::SSLManager::instance().initializeClient(nullptr, pCert, pContext);
        
        // 创建邮件消息
        std::cout << "正在创建邮件..." << std::endl;
        Poco::Net::MailMessage message;
        message.setSender(senderEmail);
        message.addRecipient(Poco::Net::MailRecipient(
            Poco::Net::MailRecipient::PRIMARY_RECIPIENT, recipientEmail));
        message.setSubject("C++ SMTP测试");
        message.setDate(Poco::Timestamp());
        message.setContent("这是一封从C++程序发出的测试邮件，用于测试POCO库的SMTP功能。");
        
        // 连接服务器发送邮件
        std::cout << "正在连接SMTP服务器..." << std::endl;
        Poco::Net::SecureSMTPClientSession session(smtpServer, port);
        session.open();
        std::cout << "正在登录..." << std::endl;
        session.login();
        std::cout << "正在验证..." << std::endl;
        session.login(Poco::Net::SMTPClientSession::AUTH_LOGIN, username, password);
        std::cout << "正在发送邮件..." << std::endl;
        session.sendMessage(message);
        std::cout << "正在关闭连接..." << std::endl;
        session.close();
        
        std::cout << "邮件发送成功！" << std::endl;
        return 0;
        
    } catch (Poco::Exception& exc) {
        std::cerr << "POCO异常: " << exc.displayText() << std::endl;
        return 1;
    } catch (std::exception& exc) {
        std::cerr << "标准异常: " << exc.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "未知异常" << std::endl;
        return 1;
    }
}
