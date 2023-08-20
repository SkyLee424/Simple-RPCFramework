#pragma once
#include "ServerSocket.hpp"
#include "TCPSocket.hpp"
#include "Procedure.hpp"
#include <iostream>
#include <vector>
#include <unordered_map>
#include <functional>
#include <nlohmann/json.hpp>

/**
 * @brief 简单的 RPC 服务器，仅支持一个线程的访问
 * 
 */
class RPCServer
{
    std::unordered_map<std::string, std::function<int(int, int)>> procedures;
    ServerSocket server;
public:
    RPCServer(const std::string &ip, uint16_t port)
        :server(ip, port) {}

    ~RPCServer()
    {
        server.close();
    }

    void start(void);
    
    void registerProcedure(const std::string &name, std::function<int(int, int)> procedure);

private:
    std::string handleRequest(const std::string &request);
};

void RPCServer::start(void)
{
    auto clnt = server.accept();
    std::cout << "[log]Connect with " << clnt->getIP() << ":" << clnt->getPort() << std::endl;
    while (true)
    {
        std::string req;
        try
        {
            req = clnt->receive();
        }
        catch(const std::exception& e)
        {
            std::cout << "[log]Released Connection with " << clnt->getIP() << ":" << clnt->getPort() << std::endl;
            clnt->close();
            break;
        }
        
        std::string ret = handleRequest(req);
        clnt->send(ret);
    }
    
}

inline
void RPCServer::registerProcedure(const std::string &name, std::function<int(int, int)> procedure)
{
    procedures[name] = procedure;
}


std::string RPCServer::handleRequest(const std::string &request)
{
    nlohmann::json reqJson = nlohmann::json::parse(request);
    std::string procedureName = reqJson["name"];
    nlohmann::json responseJson;
    if(!procedures.count(procedureName))
    {
        responseJson = 
        {
            {"error", "No Such Procedure!"}
        };
    }
    else
    {
        std::vector<int> args = reqJson["args"];
        try
        {
            int ret = procedures[procedureName](args[0], args[1]);
            responseJson = 
            {
                {"result", ret}
            };
        }
        catch(const std::exception& e)
        {
            responseJson = 
            {
                {"error", e.what()}
            };
        }
        
    }
    return responseJson.dump();
}