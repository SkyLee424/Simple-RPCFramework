#pragma once

#include "ServerSocket.hpp"
#include "TCPSocket.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

class RPCClient
{
    TCPSocket *clnt;

public:
    RPCClient(const std::string &ip, uint16_t port)
        : clnt(new TCPSocket(ip, port))
    {
        clnt->connect();
    }

    ~RPCClient()
    {
        clnt->close();
        delete clnt;
    }

    int remoteCall(const std::string &procedureName, int arg0, int arg1);
};

int RPCClient::remoteCall(const std::string &procedureName, int arg0, int arg1)
{
    nlohmann::json reqJson =
    {
        {"name", procedureName},
        {"args", {arg0, arg1}}
    };

    clnt->send(reqJson.dump()); //  序列化为字符串

    std::string ret = clnt->receive();

    nlohmann::json responseJson = nlohmann::json::parse(ret); // 反序列化

    if(responseJson.contains("error"))
        throw std::runtime_error("Error from Remote Server: " + std::string(responseJson["error"]));

    return responseJson["result"];
}