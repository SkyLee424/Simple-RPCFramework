#pragma once

#include <iostream>
#include <unordered_map>
#include <functional>
#include <string>
#include <thread>
#include <mutex>
#include "ServerSocket.hpp"
#include "TCPSocket.hpp"
#include "Serializer.hpp"
#include "ProcedurePacket.hpp"
#include "ReturnPacket.hpp"
#include "ThreadPool.h"

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



// 调用中间件，当 R 不为 void 时，调用该版本
template <typename R, typename Function, typename Tuple>
typename std::enable_if<!std::is_same<R, void>::value, typename RetType<R>::type>::type
invoke(Function &&f, Tuple &&tuple)
{
    return apply_tuple(f, tuple);
}

// 当 R 为 void 时，调用该版本，返回 0（占位，无实际意义）
template <typename R, typename Function, typename Tuple>
typename std::enable_if<std::is_same<R, void>::value, typename RetType<R>::type>::type
invoke(Function &&f, Tuple &&tuple)
{
    apply_tuple(f, tuple);
    return 0;
}

class RPCServer
{
    static constexpr size_t DEFAULT_THREAD_HOLD = 6;

    std::unordered_map<std::string, std::function<std::string(const std::string&)>> procedures;
    std::mutex mlock;
    ServerSocket server;
    ThreadPool pool;
public:
    RPCServer(const std::string &ip, uint16_t port, size_t thread_hold = DEFAULT_THREAD_HOLD)
        : server(ip, port), pool(thread_hold) {}

    ~RPCServer()
    {
        server.close();
    }

    void start(void);

    // 支持注册普通函数和 std::function 对象
    template <typename Func>
    void registerProcedure(const std::string &name, Func procedure);

    // 支持注册类成员函数
    template <typename Obj, typename Func>
    void registerProcedure(const std::string &name, Obj &obj, Func procedure);

private:
    // 处理用户的远程调用，并将返回结果序列化后，返回给用户
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

    // 调用类的成员函数
    template <typename Obj, typename Func>
    std::string callProxy(Obj &obj, Func f, const std::string &req);

    // 调用帮助函数，支持 std::function
    template <typename R, typename ...Args>
    std::string callProxyHelper(std::function<R(Args ...)> f, const std::string &req);

    // 调用帮助函数，支持普通函数
    template <typename R, typename ...Args>
    std::string callProxyHelper(R(*f)(Args ...), const std::string &req);   

    // 调用帮助函数，支持类的成员函数
    template <typename R, typename Obj, typename ...Args>
    std::string callProxyHelper(Obj &obj, R(Obj::*f)(Args...), const std::string &req);
};

void RPCServer::start(void)
{
    while (true)
    {
        auto _clnt = server.accept();
        pool.enqueue([&](std::mutex &m_lock, TCPSocket *clnt)
        {
            {
                std::lock_guard<std::mutex> lock(m_lock);
                std::cout << "[log] Connect with " << clnt->getIP() << ":" << clnt->getPort() << std::endl;
            }
            while (true)
            {
                std::string req;
                try
                {
                    req = clnt->receive();
                }
                catch (const std::exception &e)
                {
                    {
                        std::lock_guard<std::mutex> lock(m_lock);
                        std::cout << "[log] Released Connection with " << clnt->getIP() << ":" << clnt->getPort() << std::endl;
                    }
                    clnt->close();
                    break;
                }
                
                std::string ret = handleRequest(req);
                clnt->send(ret);
            }
        }, std::ref(this->mlock), _clnt);
    }
}

template <typename ...Args>
std::string RPCServer::handleRequest(const std::string &request)
{
    // 反序列化
    ProcedurePacket<Args...> packet = Serializer::Deserialize<ProcedurePacket<Args...>>(request);
    std::string name = packet.name;
    if(!procedures.count(name))
    {
        ReturnPacket<void> retPack(ReturnPacket<void>::NO_SUCH_PROCEDURE);
        return Serializer::Serialize(retPack);
    }
    auto procedure = procedures[name];
    return procedure(request); // 实际上调用的是 callProxy
}

template <typename Func>
void RPCServer::registerProcedure(const std::string &name, Func procedure)
{
    // bind callProxy 的函数指针，记得传入 this 指针，因为 callProxy 不是静态的
    procedures[name] = std::bind(&RPCServer::callProxy<Func>, this, procedure, std::placeholders::_1);
}

template <typename Obj, typename Func>
void RPCServer::registerProcedure(const std::string &name, Obj &obj, Func procedure)
{
    // 注意，这里需要使用 std::ref 获取 obj 的引用
    procedures[name] = std::bind(&RPCServer::callProxy<Obj, Func>, this, std::ref(obj), procedure, std::placeholders::_1);
}

template <typename Func>
std::string RPCServer::callProxy(Func f, const std::string &req)
{
    return callProxyHelper(f, req);
}

template <typename Obj, typename Func>
std::string RPCServer::callProxy(Obj &obj, Func f, const std::string &req)
{
    return callProxyHelper(obj, f, req);
}

template <typename R, typename ...Args>
std::string RPCServer::callProxyHelper(std::function<R(Args ...)> f, const std::string &req)   
{
    ProcedurePacket<Args...> packet = Serializer::Deserialize<ProcedurePacket<Args...>>(req);
    auto args = packet.t;
    typename RetType<R>::type ret = invoke<R>(f, args);
    ReturnPacket<R> retPack(ReturnPacket<R>::SUCCESS, ret);
    return Serializer::Serialize(retPack);
    // return Serializer::Serialize(ret);
}

template <typename R, typename ...Args>
std::string RPCServer::callProxyHelper(R(*f)(Args ...), const std::string &req)   
{
    std::function<R(Args...)> func(f);
    return callProxyHelper(func, req);
}

template <typename R, typename Obj, typename ...Args>
std::string RPCServer::callProxyHelper(Obj &obj, R(Obj::*f)(Args...), const std::string &req)
{
    ProcedurePacket<Args...> packet = Serializer::Deserialize<ProcedurePacket<Args...>>(req);
    auto args = packet.t;

    std::function<R(Args...)> func = [&](Args ...a)
    {
        // error: must use ‘.*’ or ‘->*’ to call pointer-to-member function in ‘f (...)’, e.g. ‘(... ->* f) (...)’
        // return obj.*f(a...);
        return (obj.*f)(a...); // 注意这里，是调用参数里面的 f
    };

    typename RetType<R>::type ret = invoke<R>(func, args);
    ReturnPacket<R> retPack(ReturnPacket<R>::SUCCESS, ret);
    return Serializer::Serialize(retPack);
    // return Serializer::Serialize(ret);
}