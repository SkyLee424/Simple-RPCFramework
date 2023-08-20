/**
 * @file ServerSocket.hpp
 * @author Sky Lee
 * @brief 解决了快速重启服务器失败的问题
 * @version 0.1.1
 * @date 2023-07-19
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#pragma once
#include <stdexcept>
#include <cstdio>
#include <string.h>
#include <string>
#include <cstdlib>
#include <cassert>

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "TCPSocket.hpp"

sockaddr_in get_sockaddr_in(const std::string &ip, uint16_t port);

class ServerSocket
{
    uint16_t port;
    std::string ip;
    int fd;
    int backlog;
    bool closed = false;
public:
    static const int DEFAULT_BACKLOG = 5;
public:
    using socket_t = int;

    ServerSocket(int port)
        :ServerSocket("127.0.0.1", port) {}

    ServerSocket(const std::string &ip, uint16_t port)
    {
        if(inet_addr(ip.c_str()) == INADDR_NONE)
            throw std::runtime_error("非法 IP");
        if(port < 0 || port > 0xffff)
            throw std::runtime_error("非法端口");
        this->ip = ip;
        this->port = port;
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if(fd < 0)
            throw std::runtime_error("ServerSocket: socket error");

        // 可将 TIME_WAIT 下的套接字端口号重新分配给新的套接字
        // 避免快速重启服务器失败的问题
        int opinion;
        socklen_t optlen = sizeof(opinion);
        opinion = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opinion, optlen);

        bind();
        listen();
    }

    ServerSocket(const std::string ip, uint16_t port, int backlog)
        :ServerSocket(ip, port)
    {
        this->backlog = backlog;
    }

    socket_t getSocket(void)
    {
        return fd;
    }

    // 注意：这里返回的一定是指针
    // 具体在多线程中有体现！！
    TCPSocket* accept(void)
    {
        sockaddr_in clnt_addr;
        socklen_t len = sizeof(clnt_addr);
        int clnt_sock = ::accept(fd, (sockaddr*)&clnt_addr, &len);

        if(clnt_sock < 0)
            throw std::runtime_error("accept error");
        
        TCPSocket *ret = new TCPSocket();
        ret->socketFd = clnt_sock;
        ret->ip = inet_ntoa(clnt_addr.sin_addr);
        ret->port = ntohs(clnt_addr.sin_port);
        return ret;
    }

    void close(void)
    {
        if(closed)
            return ;
        ::close(fd);
        closed = true;
    }

    // 防止用户忘记关闭套接字
    ~ServerSocket()
    {
        close();
    }

private:
    void bind(void)
    {
        sockaddr_in address = get_sockaddr_in(ip, port);
        
        if(::bind(fd, (sockaddr*)&address, sizeof(address)) < 0)
            throw std::runtime_error("bind error");
    }

    void listen(void)
    {
        if(::listen(fd, backlog) < 0)
            throw std::runtime_error("listen error");
    }
};

sockaddr_in get_sockaddr_in(const std::string &ip, uint16_t port)
{
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip.c_str());
    address.sin_port = htons(port);
    return address;
}