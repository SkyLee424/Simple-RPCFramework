#include <iostream>
#include <thread>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include "People.h"
#include "RPCClient.hpp"

// 测试基本功能
void testBasicFeature(const std::string& ip, uint16_t port)
{
    RPCClient clnt(ip, port);
    try
    {
        std::cout << clnt.remoteCall<int>("add", 1, 1) << std::endl;                                              // std::function
        std::cout << clnt.remoteCall<int>("sub", 1, 1) << std::endl;                                              // normal function
        clnt.remoteCall<void>("show", 114514);                                                                    // void return type
        clnt.remoteCall<void>("func1");                                                                           // void return type, no parma
        std::cout << clnt.remoteCall<int>("Foo::test1", 1, 17) << std::endl;                                      // member function
        std::cout << clnt.remoteCall<std::string>("hello") << std::endl;                                          // std::string return typr
        clnt.remoteCall<void>("testString", 1, std::string("hello, server!"), 1.1, std::string("RPC Framework")); // std::string parma

        People HeXin;
        HeXin = clnt.remoteCall<People>("getHeXin", HeXin); // Parameters and return types are custom types
        std::cout << "name: " << HeXin.name << ", age: " << HeXin.age << ", BinZhou: " << HeXin.BinZhou << std::endl;

        std::vector<int> nums = {1, 2, 3, 4};
        std::cout << "Sum: " << clnt.remoteCall<int>("getSum", nums) << std::endl;

        nums = {2,7,11,15};
        auto res = clnt.remoteCall<std::vector<int>>("twoSum", nums, 9);
        std::cout << "twoSum returns: [" << res[0] << ", " << res[1] << "]" << std::endl;

        auto HeXins = clnt.remoteCall<std::vector<People>>("getManyHeXin", 5);
        for(const auto &he : HeXins)
            std::cout << "name: " << he.name << ", age: " << he.age << ", BinZhou: " << he.BinZhou << std::endl;

        std::cout << clnt.remoteCall<int>("testTimeOut") << std::endl; // long time consuming task
        clnt.remoteCall<void>("excp");
        clnt.remoteCall<int>("niubi");
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}

// 测试同时能连接的最大数量
void testMaxConnections(const std::string& ip, uint16_t port, size_t taskNum)
{
    std::vector<std::unique_ptr<RPCClient>> clnts;
    for(int i = 0; i < taskNum; ++i)
    {
        std::unique_ptr<RPCClient> clnt(new RPCClient(ip, port));
        clnts.emplace_back(std::move(clnt));
    }
    std::cin.get();
}

/**
 * @brief 测试并发数，连接将请求执行完毕后释放
 * 
 * @param ip 
 * @param port 
 * @param num_threads 
 */
void testConcurrency(const std::string& ip, uint16_t port, int num_threads, int clntNum)
{
    std::vector<std::thread> threads;
    std::atomic<int> successNum = 0;
    
    for (int i = 0; i < num_threads; ++i)
    {
        threads.emplace_back([&]()
        {
            for(int i = 0; i < clntNum; ++i)
            {
                try
                {
                    RPCClient clnt(ip, port);
                    if(clnt.remoteCall<int>("add", 1, 1) == 2)
                        ++successNum;
                    if(clnt.remoteCall<int>("sub", 1, 1) == 0)
                        ++successNum;
                    if(clnt.remoteCall<std::string>("hello") == "hello, clnt!\nhahaha")
                        ++successNum;
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Exception: " << e.what() << std::endl;
                }
            }
        });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    std::cout << "Success query: " << successNum << std::endl;
}

/**
 * @brief 测试并发数，所有连接将在所有请求执行完毕后再释放
 * 
 * @param ip 
 * @param port 
 * @param num_threads 开的线程数
 * @param clntNum 每个线程有多少个客户端
 */
void testConcurrency_1(const std::string& ip, uint16_t port, int num_threads, int clntNum)
{
    std::vector<std::thread> threads;
    std::mutex clnts_lock;
    std::vector<std::shared_ptr<RPCClient>> clnts;
    std::atomic<int> successNum = 0;

    for (int i = 0; i < num_threads; ++i)
    {
        std::thread t([&](std::mutex &_clnts_lock)
        {
            for(int i = 0; i < clntNum; ++i)
            {
                std::shared_ptr<RPCClient> clnt(new RPCClient(ip, port));
                try
                {
                    if(clnt->remoteCall<int>("add", 1, 1) == 2)
                        ++successNum;
                    if(clnt->remoteCall<int>("sub", 1, 1) == 0)
                        ++successNum;
                    if(clnt->remoteCall<std::string>("hello") == "hello, clnt!\nhahaha")
                        ++successNum;
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Exception: " << e.what() << std::endl;
                }
                {
                    std::lock_guard<std::mutex> lock(_clnts_lock);
                    clnts.emplace_back(std::move(clnt));
                }
            }
        }, std::ref(clnts_lock));

        threads.emplace_back(std::move(t));
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    std::cout << "Success query: " << successNum << std::endl;
}