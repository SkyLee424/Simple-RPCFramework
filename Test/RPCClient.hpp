#pragma once

#include "ServerSocket.hpp"
#include "TCPSocket.hpp"
#include "Serializer.hpp"
#include "ProcedurePacket.hpp"
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

    template <typename R, typename ...Args>
    R remoteCall(const std::string &procedureName, const Args& ...args);

private:
    
};

template <typename R, typename ...Args>
R RPCClient::remoteCall(const std::string &procedureName, const Args& ...args)
{
    ProcedurePacket<Args ...> packet(procedureName, args...);
    std::string req = Serializer::Serialize(packet);
    clnt->send(req);
    std::string res = clnt->receive();
    return Serializer::Deserialize<R>(res);
}



