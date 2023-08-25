#pragma once
#include "ServerSocket.hpp"
#include <iostream>

sockaddr_in get_sockaddr_in(const std::string &ip, uint16_t port);

class TCPSocket
{
    friend class ServerSocket;
    int socketFd;
    std::string ip;
    uint16_t port;
    bool closed = false;
    pthread_mutex_t sendLock, recvLock;

public:
    using socket_t = int;
    // 用于服务器
    TCPSocket()
    {
        pthread_mutex_init(&sendLock, NULL);
        pthread_mutex_init(&recvLock, NULL);
    }

    // 用于客户端
    TCPSocket(const std::string ip, int port)
        :TCPSocket()
    {
        if(inet_addr(ip.c_str()) == INADDR_NONE)
            throw std::runtime_error("非法 IP");
        if(port < 0 || port > 0xffff)
            throw std::runtime_error("非法端口");
            
        this->ip = ip;
        this->port = port;

        socketFd = socket(AF_INET, SOCK_STREAM, 0);
        if(socketFd < 0)
        {
            std::string errorMsg(strerror(errno));
            throw std::runtime_error("TCPSocket: socket error: " + errorMsg);
        }
    }

    ~TCPSocket()
    {
        // std::cout << "~TCPSocket, " << ip << ":" << port << std::endl;
        close();
        pthread_mutex_destroy(&sendLock);
        pthread_mutex_destroy(&recvLock);
    }

    void connect(void)
    {
        sockaddr_in serv_addr = get_sockaddr_in(ip, port);

        // 将自己的 socketFd 与服务器的 IP:Port 绑定，并请求连接
        if(::connect(socketFd, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
        {
            std::string errorMsg(strerror(errno));
            throw std::runtime_error("connect error: " + errorMsg);
        }
    }

    void send(const std::string& msg)
    {
        // 构造消息头，包含数据包的长度信息
        uint32_t msgLength = htonl(msg.size()); // 转化为网络字节序
        // 将 msgLength 强转，封装在消息头部
        std::string packet = std::string(reinterpret_cast<const char*>(&msgLength), sizeof(msgLength)) + msg;

        pthread_mutex_lock(&sendLock);
        int sendSize = ::send(socketFd, packet.data(), packet.size(), 0);
        pthread_mutex_unlock(&sendLock);

        if (sendSize < 0)
        {
            std::string errorMsg(strerror(errno));
            throw std::runtime_error("send error: " + errorMsg);
        }
    }

    std::string receive(void)
    {
        pthread_mutex_lock(&recvLock);
        uint32_t msgLength;
        // 先来获取消息头，注意，在读的时候，需要强转的类型应该与之前一致
        int readSize = recv(socketFd, reinterpret_cast<char*>(&msgLength), sizeof(msgLength), 0);
        pthread_mutex_unlock(&recvLock);

        if (readSize <= 0)
        {
            std::string errorMsg(strerror(errno));
            throw std::runtime_error("recv error: " + errorMsg);
        }

        msgLength = ntohl(msgLength);
        std::string buffer(msgLength, '\0');
        int remainingSize = msgLength;
        int offset = 0;

        while (remainingSize > 0) {
            pthread_mutex_lock(&recvLock);
            int chunkSize = recv(socketFd, &buffer[offset], remainingSize, 0); // 这一行细细体会
            pthread_mutex_unlock(&recvLock);

            if (chunkSize < 0)
            {
                std::string errorMsg(strerror(errno));
                throw std::runtime_error("recv error: " + errorMsg);
            }
            else if(chunkSize == 0)
                throw std::logic_error("clnt exit");

            remainingSize -= chunkSize;
            offset += chunkSize;
        }
        
        return buffer;
    }

    void close(void)
    {
        if(closed)
            return ;
        ::close(socketFd);
        closed = true;
    }

    bool valid(void)
    {
        return !closed;
    }

    std::string getIP(void)
    {
        return ip;
    }

    uint16_t getPort(void)
    {
        return port;
    }

    socket_t getSocket(void)
    {
        return socketFd;
    }
};