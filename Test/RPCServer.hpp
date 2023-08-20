#include <iostream>
#include <unordered_map>
#include <functional>
#include <string>
#include "ServerSocket.hpp"
#include "TCPSocket.hpp"
#include "Serializer.hpp"
#include "ProcedurePacket.hpp"

template <typename Function, typename Tuple, size_t... Index>
decltype(auto) apply_tuple_impl(Function&& func, Tuple&& tuple, std::index_sequence<Index...>) {
    return func(std::get<Index>(std::forward<Tuple>(tuple))...);
}

template <typename Function, typename Tuple>
decltype(auto) apply_tuple(Function&& func, Tuple&& tuple) {
    constexpr size_t tuple_size = std::tuple_size<std::decay_t<Tuple>>::value;
    return apply_tuple_impl(std::forward<Function>(func), std::forward<Tuple>(tuple),
                            std::make_index_sequence<tuple_size>());
}


class RPCServer
{
    std::unordered_map<std::string, std::function<std::string(const std::string&)>> procedures;
    ServerSocket server;

public:
    RPCServer(const std::string &ip, uint16_t port)
        : server(ip, port) {}

    ~RPCServer()
    {
        server.close();
    }

    void start(void);

    template <typename Func>
    void registerProcedure(const std::string &name, Func procedure);

private:
    template <typename ...Args>
    std::string handleRequest(const std::string &request);

    /**
     * @brief 
     * 
     * @tparam Func 
     * @param f 待调用过程
     * @param req 用户发出的请求
     * @return std::string 
     */
    template <typename Func>
    std::string callProxy(Func f, const std::string &req);

    template <typename R, typename ...Args>
    std::string callProxyHelper(std::function<R(Args ...)> f, const std::string &req);

    template <typename R, typename ...Args>
    std::string callProxyHelper(R(*f)(Args ...), const std::string &req);   
};

void RPCServer::start(void)
{
    auto clnt = server.accept();
    std::cout << "[log] Connect with " << clnt->getIP() << ":" << clnt->getPort() << std::endl;
    while (true)
    {
        std::string req;
        try
        {
            req = clnt->receive();
        }
        catch (const std::exception &e)
        {
            std::cout << "[log] Released Connection with " << clnt->getIP() << ":" << clnt->getPort() << std::endl;
            clnt->close();
            break;
        }

        std::string ret = handleRequest(req);
        clnt->send(ret);
    }
}

template <typename ...Args>
std::string RPCServer::handleRequest(const std::string &request)
{
    // 反序列化
    ProcedurePacket<Args...> packet = Serializer::Deserialize<ProcedurePacket<Args...>>(request);
    std::string name = packet.name;
    if(!procedures.count(name))
        throw std::runtime_error("No Such Procedure!");
    auto procedure = procedures[name];
    return procedure(request); // 实际上调用的是 callProxy
}

template <typename Func>
void RPCServer::registerProcedure(const std::string &name, Func procedure)
{
    // bind callProxy 的函数指针，记得传入 this 指针，因为 callProxy 不是静态的
    procedures[name] = std::bind(&RPCServer::callProxy<Func>, this, procedure, std::placeholders::_1);
}

template <typename Func>
std::string RPCServer::callProxy(Func f, const std::string &req)
{
    return callProxyHelper(f, req);
}



// std::function
template <typename R, typename ...Args>
std::string RPCServer::callProxyHelper(std::function<R(Args ...)> f, const std::string &req)   
{
    ProcedurePacket<Args...> packet = Serializer::Deserialize<ProcedurePacket<Args...>>(req);
    auto args = packet.t;
    R ret = apply_tuple(f, args);
    return Serializer::Serialize(ret);
}

// function pointer
template <typename R, typename ...Args>
std::string RPCServer::callProxyHelper(R(*f)(Args ...), const std::string &req)   
{
    std::function<R(Args...)> func(f);
    return callProxyHelper(func, req);
}