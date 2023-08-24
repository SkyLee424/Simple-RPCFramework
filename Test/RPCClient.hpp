#pragma once

#include <iostream>
#include "ServerSocket.hpp"
#include "TCPSocket.hpp"
#include "Serializer.hpp"
#include "ProcedurePacket.hpp"
#include "ReturnPacket.hpp"

class RPCClient
{
    TCPSocket *clnt;
    bool closed;
public:
    RPCClient(const std::string &ip, uint16_t port)
        : clnt(new TCPSocket(ip, port)), closed(false)
    {
        clnt->connect();
    }

    ~RPCClient()
    {
        if(!closed)
        {
            clnt->close();
            delete clnt;
        }
    }

    template <typename R, typename ...Args>
    typename
    std::enable_if<!std::is_same<R, void>::value, R>::type
    remoteCall(const std::string &procedureName, const Args& ...args);

    // 当返回值为 void 时，调用该版本
    template <typename R, typename ...Args>
    typename
    std::enable_if<std::is_same<R, void>::value, void>::type
    remoteCall(const std::string &procedureName, const Args& ...args);
};

template <typename R, typename ...Args>
typename
std::enable_if<!std::is_same<R, void>::value, R>::type
RPCClient::remoteCall(const std::string &procedureName, const Args& ...args)
{
    ProcedurePacket<Args ...> packet(procedureName, args...);
    std::string req = Serializer::Serialize(packet);
    clnt->send(req);
    std::string res = clnt->receive();
    ReturnPacket<R> ret = Serializer::Deserialize<ReturnPacket<R>>(res);
    if(!ret.vaild())
        throw std::runtime_error("remoteCall: Received error code from server, error code: " + std::to_string(ret.getCode()));
    return ret.getRet();
}

template <typename R, typename ...Args>
typename
std::enable_if<std::is_same<R, void>::value, void>::type
RPCClient::remoteCall(const std::string &procedureName, const Args& ...args)
{
    remoteCall<int>(procedureName, args...);
}