#pragma once
#include <stdexcept>
#include <string>
#include <mutex>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <iostream>

sockaddr_in get_sockaddr_in(const std::string &ip, uint16_t port);

class TCPSocket
{
    std::string IP;
    uint16_t port;
    int _native_sock;
    bool closed;
    std::mutex send_lock, read_lock;

public:
    TCPSocket();
    TCPSocket(const std::string &ip, uint16_t port, int backlog);
    ~TCPSocket();

    /* server 用 */
    void bind(void);
    void listen(int backlog);
    TCPSocket *accept(void);

    /* client 用 */
    void connect(const std::string &IP, uint16_t port);

    /* server 和 client 用 */
    void send(const std::string &msg);
    std::string receive(void);
    void close();

    std::string getIP() const
    {return IP;}

    uint16_t getPort() const
    {return port;}

    int native_sock() const
    {return _native_sock;}
};

TCPSocket::TCPSocket()
    :closed(false)
{
    _native_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (_native_sock < 0)
    {
        std::string errorMsg(strerror(errno));
        throw std::runtime_error("socket error: " + errorMsg);
    }
}

TCPSocket::TCPSocket(const std::string &ip, uint16_t port, int backlog)
    : TCPSocket() // 初始化套接字
{
    if (inet_addr(ip.c_str()) == INADDR_NONE)
        throw std::runtime_error("Illegal host");
    if (port < 0 || port > 0xffff)
        throw std::runtime_error("Illegal port");

    this->IP = ip;
    this->port = port;

    int opinion;
    socklen_t optlen = sizeof(opinion);
    opinion = 1;
    setsockopt(native_sock(), SOL_SOCKET, SO_REUSEADDR, &opinion, optlen);

    bind();
    listen(backlog);
}

TCPSocket::~TCPSocket()
{
    close();
}

void TCPSocket::bind(void)
{
    sockaddr_in address = get_sockaddr_in(IP, port);

    if (::bind(native_sock(), (sockaddr *)&address, sizeof(address)) < 0)
    {
        std::string errorMsg(strerror(errno));
        throw std::runtime_error("bind error: " + errorMsg);
    }
}

void TCPSocket::listen(int backlog)
{
    if (::listen(native_sock(), backlog) < 0)
    {
        std::string errorMsg(strerror(errno));
        throw std::runtime_error("listen error: " + errorMsg);
    }
}

TCPSocket *TCPSocket::accept(void)
{
    sockaddr_in clnt_addr;
    socklen_t len = sizeof(clnt_addr);
    int clnt_sock = ::accept(native_sock(), (sockaddr *)&clnt_addr, &len);

    if (clnt_sock < 0)
    {
        std::string errorMsg(strerror(errno));
        throw std::runtime_error("accept error: " + errorMsg);
    }

    TCPSocket *ret = new TCPSocket();
    ret->_native_sock = clnt_sock;
    ret->IP = inet_ntoa(clnt_addr.sin_addr);
    ret->port = ntohs(clnt_addr.sin_port);
    return ret;
}

void TCPSocket::connect(const std::string &ip, uint16_t port)
{
    sockaddr_in serv_addr = get_sockaddr_in(ip, port);

    // 将自己的 socketFd 与服务器的 IP:Port 绑定，并请求连接
    if (::connect(native_sock(), (sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        std::string errorMsg(strerror(errno));
        throw std::runtime_error("connect error: " + errorMsg);
    }
}

void TCPSocket::close(void)
{
    if (closed)
        return;
    ::close(native_sock());
    closed = true;
}

void TCPSocket::send(const std::string &msg)
{
    // 构造消息头，包含数据包的长度信息
    uint32_t msgLength = htonl(msg.size()); // 转化为网络字节序
    // 将 msgLength 强转，封装在消息头部
    std::string packet = std::string(reinterpret_cast<const char *>(&msgLength), sizeof(msgLength)) + msg;

    int sendSize;
    {
        // std::lock_guard<std::mutex> lock(send_lock);
        sendSize = ::send(native_sock(), packet.data(), packet.size(), 0);
    }
    if (sendSize < 0)
    {
        std::string errorMsg(strerror(errno));
        throw std::runtime_error("send error: " + errorMsg);
    }
}

std::string TCPSocket::receive(void)
{
    uint32_t msgLength;
    ssize_t readSize;
    {
        std::lock_guard<std::mutex> lock(read_lock);
        // 先来获取消息头，注意，在读的时候，需要强转的类型应该与之前一致
        readSize = recv(native_sock(), reinterpret_cast<char *>(&msgLength), sizeof(msgLength), 0);
    }

    if (readSize <= 0)
    {
        std::string errorMsg(strerror(errno));
        throw std::runtime_error("recv error: " + errorMsg);
    }

    msgLength = ntohl(msgLength);
    std::string buffer(msgLength, '\0');
    int remainingSize = msgLength;
    int offset = 0;

    while (remainingSize > 0)
    {
        int chunkSize;
        {
            std::lock_guard<std::mutex> lock(read_lock);
            chunkSize = recv(native_sock(), &buffer[offset], remainingSize, 0);
        }
        if (chunkSize < 0)
        {
            std::string errorMsg(strerror(errno));
            throw std::runtime_error("recv error: " + errorMsg);
        }
        remainingSize -= chunkSize;
        offset += chunkSize;
    }

    return buffer;
}

// 工具函数
sockaddr_in get_sockaddr_in(const std::string &ip, uint16_t port)
{
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip.c_str());
    address.sin_port = htons(port);
    return address;
}