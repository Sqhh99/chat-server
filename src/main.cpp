#include <iostream>
#include <signal.h>
#include "server/ChatServer.h"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "service/MessageArchiveService.h"
#include "service/RedisService.h"

// 全局变量，用于在信号处理函数中访问聊天服务器
ChatServer* g_chatServer = nullptr;

// 处理SIGINT和SIGTERM信号
void handleSignal(int sig) {
    LOG_INFO << "Received signal " << sig << ", shutting down...";
    if (g_chatServer) {
        g_chatServer->stop();
    }
}

int main(int argc, char* argv[]) {
    // 设置日志级别
    muduo::Logger::setLogLevel(muduo::Logger::INFO);
    LOG_INFO << "Chat server starting...";
    
    // 注册信号处理函数
    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);
    
    // 设置服务器监听地址和端口
    uint16_t port = 8888;
    std::string ip = "0.0.0.0";
    
    if (argc > 1) {
        port = static_cast<uint16_t>(std::stoi(argv[1]));
    }
    
    if (argc > 2) {
        ip = argv[2];
    }
    
    muduo::net::InetAddress serverAddr(ip, port);
    
    // 创建事件循环
    muduo::net::EventLoop loop;
    
    // 初始化Redis服务
    if (!RedisService::getInstance().init("127.0.0.1", 6379, "2932897504xu")) {
        LOG_ERROR << "Failed to initialize Redis service";
        return 1;
    }
    LOG_INFO << "Redis service initialized successfully";
    
    // 初始化消息归档服务
    if (!MessageArchiveService::getInstance().init()) {
        LOG_ERROR << "Failed to initialize Message Archive service";
        return 1;
    }
    LOG_INFO << "Message Archive service initialized successfully";
    
    // 启动消息归档线程
    MessageArchiveService::getInstance().start();
    
    // 创建聊天服务器
    ChatServer server(&loop, serverAddr, "ChatServer");
    g_chatServer = &server;
    
    // 启动服务器
    server.start();
    
    LOG_INFO << "Chat server is running on " << ip << ":" << port;
    
    // 运行事件循环
    loop.loop();
    
    // 停止消息归档服务
    MessageArchiveService::getInstance().stop();
    
    // 清理全局指针
    g_chatServer = nullptr;
    
    return 0;
}
