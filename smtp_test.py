#!/usr/bin/env python3
import smtplib
import ssl
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
import sys

def test_smtp_connection(smtp_server, port, username, password, sender_email, receiver_email):
    """
    测试SMTP服务器连接和发送邮件功能。
    
    参数:
    smtp_server - SMTP服务器地址
    port - SMTP服务器端口
    username - 邮箱用户名
    password - 邮箱密码或授权码
    sender_email - 发件人邮箱
    receiver_email - 收件人邮箱
    """
    print(f"测试SMTP连接: {smtp_server}:{port}")
    print(f"用户名: {username}")
    print(f"发件人: {sender_email}")
    print(f"收件人: {receiver_email}")
    
    # 创建邮件
    message = MIMEMultipart()
    message["Subject"] = "SMTP测试邮件"
    message["From"] = sender_email
    message["To"] = receiver_email
    
    # 添加邮件正文
    body = """
    这是一封测试邮件，用于验证SMTP服务器配置是否正确。
    
    时间: 2025-06-14
    """
    message.attach(MIMEText(body, "plain"))
    
    try:
        # 创建安全SSL上下文
        context = ssl.create_default_context()
        
        # 连接到服务器并发送邮件
        with smtplib.SMTP_SSL(smtp_server, port, context=context) as server:
            print("正在连接到SMTP服务器...")
            server.ehlo()  # 向服务器标识自己
            
            print("正在登录...")
            server.login(username, password)
            
            print("正在发送邮件...")
            server.sendmail(sender_email, receiver_email, message.as_string())
            
            print("邮件发送成功!")
            
    except Exception as e:
        print(f"错误: {e}")
        return False
    
    return True

if __name__ == "__main__":
    # 163邮箱配置
    smtp_server = "smtp.163.com"
    port = 465  # SSL端口
    username = "sqhh99@163.com"  # 用户名通常是邮箱地址
    password = "GHx58fk48fuxcw8H"  # 授权码
    sender_email = username  # 确保发件人与用户名相同
    
    # 接收者邮箱 (如果提供了命令行参数，则使用它)
    receiver_email = sys.argv[1] if len(sys.argv) > 1 else sender_email
    
    # 测试连接
    result = test_smtp_connection(
        smtp_server, port, username, password, 
        sender_email, receiver_email
    )
    
    sys.exit(0 if result else 1)
