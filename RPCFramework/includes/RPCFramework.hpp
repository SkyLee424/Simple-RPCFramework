#pragma once

#include <unordered_map>
#include <functional>
#include <string>
#include <log4cplus/logger.h>
#include <log4cplus/configurator.h>
#include <log4cplus/loggingmacros.h>
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

class RPCFramework
{
public: 
    static constexpr int DEFAULT_CRITICAL_TIME = 3000; // 默认调用过程临界时间，单位为 ms
private:
    std::unordered_map<std::string, std::function<std::string(const std::string&)>> procedures;
    log4cplus::Logger logger;
    int criticalTime; // 若调用某个过程超过该时间，将会输出警告信息到日志文件中，-1 代表关闭警告
    
public:
    RPCFramework(int criticalTime = DEFAULT_CRITICAL_TIME)
        :criticalTime(criticalTime)
    {
        log4cplus::initialize();
        try
        {
            log4cplus::PropertyConfigurator::doConfigure("Log/config/log4cplus.properties");
            logger = log4cplus::Logger::getInstance("FrameworkLogger");
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            exit(-1);
        }
    }

    // 支持注册普通函数和 std::function 对象
    template <typename Func>
    void registerProcedure(const std::string &name, Func procedure);

    // 支持注册类成员函数
    template <typename Obj, typename Func>
    void registerProcedure(const std::string &name, Obj &obj, Func procedure);

    // 处理用户的远程调用，并将返回结果序列化后，返回给用户
    template <typename ...Args>
    std::string handleRequest(const std::string &request);

private:

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

template <typename ...Args>
std::string RPCFramework::handleRequest(const std::string &request)
{
    // 反序列化
    ProcedurePacket<Args...> packet = Serializer::Deserialize<ProcedurePacket<Args...>>(request);
    std::string name = packet.name;
    if(!procedures.count(name))
    {
        LOG4CPLUS_WARN(logger, "No such procedure: " + name);
        ReturnPacket<void> retPack(ReturnPacket<void>::NO_SUCH_PROCEDURE);
        return Serializer::Serialize(retPack);
    }
    auto procedure = procedures[name];
    auto startTime = std::chrono::steady_clock::now();

    std::string ret;
    try
    {
        ret = procedure(request); // 实际上调用的是 callProxy
    }
    catch(const std::exception& e)
    {
        LOG4CPLUS_ERROR(logger, "Handler procedure \'" + name +  "\' error, message: " + std::string(e.what()));
        ReturnPacket<void> retPack(ReturnPacket<void>::UNKNOWN);
        return Serializer::Serialize(retPack);
    }
    
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    if(duration >= criticalTime)
        LOG4CPLUS_WARN(logger, "Procedure '" + name + "' runtime exceeded, cost " + std::to_string(duration) + " ms");
    return ret;
}

template <typename Func>
void RPCFramework::registerProcedure(const std::string &name, Func procedure)
{
    LOG4CPLUS_INFO(logger, "Regist procedure " + name);
    // bind callProxy 的函数指针，记得传入 this 指针，因为 callProxy 不是静态的
    procedures[name] = std::bind(&RPCFramework::callProxy<Func>, this, procedure, std::placeholders::_1);
}

template <typename Obj, typename Func>
void RPCFramework::registerProcedure(const std::string &name, Obj &obj, Func procedure)
{
    LOG4CPLUS_INFO(logger, "Regist procedure " + name);
    // 注意，这里需要使用 std::ref 获取 obj 的引用
    procedures[name] = std::bind(&RPCFramework::callProxy<Obj, Func>, this, std::ref(obj), procedure, std::placeholders::_1);
}

template <typename Func>
std::string RPCFramework::callProxy(Func f, const std::string &req)
{
    return callProxyHelper(f, req);
}

template <typename Obj, typename Func>
std::string RPCFramework::callProxy(Obj &obj, Func f, const std::string &req)
{
    return callProxyHelper(obj, f, req);
}

template <typename R, typename ...Args>
std::string RPCFramework::callProxyHelper(std::function<R(Args ...)> f, const std::string &req)   
{
    ProcedurePacket<Args...> packet = Serializer::Deserialize<ProcedurePacket<Args...>>(req);
    auto args = packet.t;
    typename RetType<R>::type ret = invoke<R>(f, args);
    ReturnPacket<R> retPack(ReturnPacket<R>::SUCCESS, ret);
    return Serializer::Serialize(retPack);
    // return Serializer::Serialize(ret);
}

template <typename R, typename ...Args>
std::string RPCFramework::callProxyHelper(R(*f)(Args ...), const std::string &req)   
{
    std::function<R(Args...)> func(f);
    return callProxyHelper(func, req);
}

template <typename R, typename Obj, typename ...Args>
std::string RPCFramework::callProxyHelper(Obj &obj, R(Obj::*f)(Args...), const std::string &req)
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