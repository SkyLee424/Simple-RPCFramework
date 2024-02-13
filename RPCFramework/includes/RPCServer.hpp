#pragma once

#include "TCPSocket.hpp"
#include "ThreadPool.h"
#include "TaskQueue.hpp"
#include "RPCFramework.hpp"
#include <sys/epoll.h>
#include <vector>
#include <unordered_map>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <signal.h>
#include <log4cplus/logger.h>
#include <log4cplus/configurator.h>
#include <log4cplus/loggingmacros.h>
#include <fcntl.h>

/**
 * @brief 基于多 Reacor 多线程实现的 RPCServer
 * @author Sky_Lee
 * @date 2024-02-11
 *
 */
class RPCServer
{
    // configs
    uint16_t reactor_nums;          // reactor 的总数量
    uint16_t task_thread_nums;      // 每个 reactor 拥有的 task thread 数
    size_t epoll_buffer_size;       // 单次循环 epoll_event 的最大值
    int epoll_wait_timeout;         // epoll_wait 的超时时间

    // member vars
    TCPSocket srv_sock;             // Server 的 socket
    RPCFramework framework;         // RPC 框架部分

    log4cplus::Logger logger;       // 记录除了错误日志的其它日志
    log4cplus::Logger errorLogger;  // 记录错误日志

    int main_epfd;                      // 主 reactor 的 epoll 实例
    std::mutex epfds_lock;              // 互斥访问 epfds
    std::unordered_map<int, int> epfds; // 记录每个 sub reactor 的 active connections
    std::function<bool(int, int)> cmp = [this](int epfd0, int epfd1) -> bool
    {
        return epfds.at(epfd0) > epfds.at(epfd1);
    };
    std::priority_queue<int, std::vector<int>, decltype(cmp)> pq{cmp};  // 小根堆，每次选择监视 socket 数量最小的 epfd
    ThreadPool reactors;                                                // 使用线程池管理主从 reactor
    std::atomic<int> active_reactors;                                   // 当前活跃的 reactor 数

public:
    static constexpr uint16_t DEFAULT_REACTOR_NUM = 2;                // 默认 1 主 1 从
    static constexpr int DEFAULT_BACKLOG = INT32_MAX;                 // 默认全连接数
    static constexpr uint16_t DEFAULT_TASK_THREAD_HOLD = 8;           // 默认一个 epoll 实例最多可以开的线程数
    static constexpr size_t DEFAULT_EPOLL_BUFFER_SIZE = 4096;         // 默认一个 epoll 实例的 buffer 的大小
    static constexpr int DEFAULT_EPOLL_WAIT_TIME = 5000;              // 默认 epoll_wait 的等待时间

    /**
     * @brief 创建 RPC 服务
     * 
     * @param ip                服务器监听的 IP
     * @param port              服务器监听的端口
     * @param backlog           全连接队列的大小（还与内核的 somaxconn 有关）
     * @param reactor_num       reactor 的总数量
     * @param task_thread_nums  每个 reactor 拥有的 task thread 数
     * @param epoll_buffer_size 单次循环 epoll_event 的最大值
     * @param epoll_wait_time   epoll_wait 的超时时间
     */
    RPCServer(const std::string &ip, uint16_t port, int backlog,
              uint16_t reactor_num = DEFAULT_REACTOR_NUM, 
              uint16_t task_thread_nums = DEFAULT_TASK_THREAD_HOLD, 
              size_t epoll_buffer_size = DEFAULT_EPOLL_BUFFER_SIZE,
              int epoll_wait_time = DEFAULT_EPOLL_WAIT_TIME);

    ~RPCServer();

    void start();

    /**
     * @brief 注册 RPC 服务
     * 
     * @tparam Func 
     * @param name      服务名称
     * @param procedure 服务本身
     */
    template <typename Func>
    void registerProcedure(const std::string &name, Func procedure);

    /**
     * @brief 
     * 
     * @tparam Obj 
     * @tparam Func 
     * @param name      服务名称
     * @param obj       对象
     * @param procedure 服务本身（成员函数）
     */
    template <typename Obj, typename Func>
    void registerProcedure(const std::string &name, Obj &obj, Func procedure);
private:
    static void accept_handler(RPCServer *rpc_srv);

    static void request_handler(RPCServer *rpc_srv, int epfd);

    static void sig_handler(int sig);

    static bool exited;
};

bool RPCServer::exited = false;

RPCServer::RPCServer(const std::string &ip, uint16_t port, int backlog, 
                     uint16_t reactor_nums, 
                     uint16_t task_thread_nums, 
                     size_t epoll_buffer_size,
                     int epoll_wait_time)
    : reactor_nums(reactor_nums), task_thread_nums(task_thread_nums), epoll_buffer_size(epoll_buffer_size), reactors(reactor_nums), srv_sock(ip, port, backlog), epoll_wait_timeout(epoll_wait_time)
{
    log4cplus::initialize();
    log4cplus::PropertyConfigurator::doConfigure("Log/config/log4cplus.properties"); // 配置文件的路径
    try
    {
        logger = log4cplus::Logger::getInstance("ServerLogger"); // 要使用的日志记录器
        errorLogger = log4cplus::Logger::getInstance("ErrorLogger");
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error("initialize log4cplus failed: " + std::string(e.what()));
    }
    LOG4CPLUS_INFO(logger, "Initialize log4cplus successfully");

    if (reactor_nums <= 1)
    {
        throw std::runtime_error("at least two reactors!");
    }
    
    // 创建 main reactor
    main_epfd = epoll_create1(0);
    epoll_event event;
    event.data.fd = srv_sock.native_sock();
    event.events = EPOLLIN;
    epoll_ctl(main_epfd, EPOLL_CTL_ADD, srv_sock.native_sock(), &event);
    LOG4CPLUS_INFO(logger, "Initialize main reactor successfully");

    for (size_t i = 1; i < reactor_nums; i++)
    {
        int epfd = epoll_create1(0);
        epfds[epfd] = 0;
        pq.push(epfd);
        // 创建 sub reactor
        reactors.enqueue(request_handler, this, epfd);
    }
    LOG4CPLUS_INFO(logger, "Initialize sub reactor successfully");
}

RPCServer::~RPCServer() 
{
    srv_sock.close();
}

void RPCServer::sig_handler(int sig_num)
{
    if (exited)
    {
        exit(-1);
    }
    exited = true;
}

void RPCServer::start(void)
{
    reactors.enqueue(accept_handler, this);
    LOG4CPLUS_INFO(logger, "RPC Server startup is complete and can now accept RPC requests from clients");

    // 阻塞，直到 control^c
    signal(SIGINT, sig_handler);
    while (!exited)
    {
        pause();
    }
    // 执行清理
    LOG4CPLUS_INFO(logger, "Performing necessary cleanup...(Press again to force stop)");
    while (active_reactors > 0)
    {
        sleep(1);
    }
    LOG4CPLUS_INFO(logger, "RPC Server is about to exit...");
}

template <typename Func>
void RPCServer::registerProcedure(const std::string &name, Func procedure)
{
    framework.registerProcedure(name, procedure);
}

template <typename Obj, typename Func>
void RPCServer::registerProcedure(const std::string &name, Obj &obj, Func procedure)
{
    framework.registerProcedure(name, obj, procedure);
}

void RPCServer::accept_handler(RPCServer *rpc_srv)
{
    epoll_event events[rpc_srv->epoll_buffer_size];
    epoll_event ev;
    ++rpc_srv->active_reactors;
    while (true)
    {
        int eventsNum = epoll_wait(rpc_srv->main_epfd, events, rpc_srv->epoll_buffer_size, rpc_srv->epoll_wait_timeout);
        if (eventsNum == -1)
        {
            LOG4CPLUS_ERROR(rpc_srv->errorLogger, "RPCServer::accept_handler: epoll_wait error: " + std::string(strerror(errno)));
            continue;
        }
        // 如果需要退出
        if (rpc_srv->exited && eventsNum == 0)
            break;

        for (size_t i = 0; i < eventsNum; i++)
        {
            // 如果是连接请求事件
            if (events[i].data.fd == rpc_srv->srv_sock.native_sock())
            {
                // 接受连接
                TCPSocket* clnt_sock = rpc_srv->srv_sock.accept();
                int clnt_native_sock = clnt_sock->native_sock();

                // 创建读事件
                fcntl(clnt_native_sock, F_SETFL, O_NONBLOCK);
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = clnt_native_sock;

                // 分发 clnt_sock 给 sub reactor
                int epfd = -1;
                {
                    std::lock_guard<std::mutex> lock(rpc_srv->epfds_lock);
                    epfd = rpc_srv->pq.top();
                    ++rpc_srv->epfds[epfd]; // 活跃连接数 + 1
                    rpc_srv->pq.pop();
                    rpc_srv->pq.push(epfd);
                }
                
                // 添加新的 clnt_sock 到 epoll 实例
                if (epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_native_sock, &ev) == -1) {
                    LOG4CPLUS_ERROR(rpc_srv->errorLogger, "RPCServer::accept_handler: epoll_ctl: EPOLL_CTL_ADD error: " + std::string(strerror(errno)));
                    clnt_sock->close();
                }
                delete clnt_sock;
            }
            // 否则，忽略事件
        }
    }
    --rpc_srv->active_reactors;
}

void RPCServer::request_handler(RPCServer *rpc_srv, int epfd)
{
    TaskQueue tq(rpc_srv->task_thread_nums, 200);
    std::unordered_map<int, std::string> resp;
    std::mutex resp_lock;
    epoll_event events[rpc_srv->epoll_buffer_size];
    epoll_event ev;

    ++rpc_srv->active_reactors;
    while (true)
    {
        int eventsNum = epoll_wait(epfd, events, rpc_srv->epoll_buffer_size, rpc_srv->epoll_wait_timeout);
        if (eventsNum == -1)
        {
            LOG4CPLUS_ERROR(rpc_srv->errorLogger, "RPCServer::request_handler: epoll_wait error: " + std::string(strerror(errno)));
            continue;
        }
        // 如果需要退出
        if (rpc_srv->exited && eventsNum == 0)
            break;

        for (size_t i = 0; i < eventsNum; i++)
        {
            int clnt_sock = events[i].data.fd;
            // 读事件
            if (events[i].events & EPOLLIN)
            {
                uint32_t msg_len;
                ssize_t readSize;
                readSize = recv(clnt_sock, reinterpret_cast<char *>(&msg_len), sizeof(msg_len), 0);

                if (readSize <= 0)
                {
                    if (readSize == 0) // 断开连接请求
                    {
                        epoll_ctl(epfd, EPOLL_CTL_DEL, clnt_sock, NULL);
                        close(clnt_sock);
                        {
                            std::lock_guard<std::mutex> lock(rpc_srv->epfds_lock);
                            --rpc_srv->epfds[epfd];
                        }
                        {
                            std::lock_guard<std::mutex> lock(resp_lock);
                            resp.erase(clnt_sock);
                        }
                    }
                    else 
                    {
                        LOG4CPLUS_ERROR(rpc_srv->errorLogger, "RPCServer::request_handler: recv error: " + std::string(strerror(errno)));
                    }
                    continue;
                }
                
                msg_len = ntohl(msg_len);
                std::string buffer(msg_len, '\0');
                int remainingSize = msg_len;
                int offset = 0;

                while (remainingSize > 0)
                {
                    int chunkSize = recv(clnt_sock, &buffer[offset], remainingSize, 0);
                    if (chunkSize < 0)
                    {
                        LOG4CPLUS_ERROR(rpc_srv->errorLogger, "RPCServer::request_handler: recv error: " + std::string(strerror(errno)));
                    }
                    remainingSize -= chunkSize;
                    offset += chunkSize;
                }

                // 添加请求到 TaskQueue
                tq.enqueue(clnt_sock, [rpc_srv, epfd, clnt_sock](const std::string &data, std::unordered_map<int, std::string> &_resp, std::mutex &_resp_lock){
                    // 调用 rpc 服务
                    std::string resp_data = rpc_srv->framework.handleRequest(data);
                    // std::string resp_data = "this is a resp, orignal data: " + data;
                    // 将响应写入 resp 哈希表
                    {
                        std::lock_guard<std::mutex> lock(_resp_lock);
                        _resp[clnt_sock] = resp_data;
                    }
                    // 注册写事件
                    epoll_event ev;
                    ev.data.fd = clnt_sock;
                    ev.events = EPOLLOUT | EPOLLET;
                    if (epoll_ctl(epfd, EPOLL_CTL_MOD, clnt_sock, &ev) == -1)
                    {
                        LOG4CPLUS_ERROR(rpc_srv->errorLogger, "RPCServer::request_handler: epoll_ctl: EPOLL_CTL_MOD error: " + std::string(strerror(errno)));
                    }
                }, buffer, std::ref(resp), std::ref(resp_lock));
            }
            // 写事件
            else if (events[i].events & EPOLLOUT)
            {
                std::string resp_data;
                {
                    std::lock_guard<std::mutex> lock(resp_lock);
                    try
                    {
                        resp_data = resp.at(clnt_sock);
                    }
                    catch(const std::exception& e)
                    {
                        std::cout << "get resp error: " << e.what() << "\n";
                        LOG4CPLUS_ERROR(rpc_srv->errorLogger, "RPCServer::request_handler: get resp from resp_mapping error: " + std::string(e.what()));
                    }
                }

                // 构造消息头，包含数据包的长度信息
                uint32_t msg_len = htonl(resp_data.size());
                // 将 msg_len 强转，封装在消息头部
                std::string packet = std::string(reinterpret_cast<const char *>(&msg_len), sizeof(msg_len)) + resp_data;
                // 响应执行结果                
                int sendSize = send(clnt_sock, packet.data(), packet.size(), 0);
                if (sendSize < 0)
                {
                    LOG4CPLUS_ERROR(rpc_srv->errorLogger, "RPCServer::request_handler: send error: " + std::string(strerror(errno)));
                }

                // 注册读事件
                ev.data.fd = clnt_sock;
                ev.events = EPOLLIN | EPOLLET;
                if (epoll_ctl(epfd, EPOLL_CTL_MOD, clnt_sock, &ev) == -1)
                {
                    LOG4CPLUS_ERROR(rpc_srv->errorLogger, "RPCServer::request_handler: epoll_ctl: EPOLL_CTL_MOD error: " + std::string(strerror(errno)));
                }
            }
        }
    }
    --rpc_srv->active_reactors;
}