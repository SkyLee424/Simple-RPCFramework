#include "ServerSocket.hpp"
#include "ThreadPool.h"
#include "RPCFramework.hpp"
#include <sys/epoll.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <log4cplus/logger.h>
#include <log4cplus/configurator.h>
#include <log4cplus/loggingmacros.h>
#include <iostream> // for debug

class RPCServer;

void doEpoll(RPCServer *server, int epfd);

class RPCServer
{
    friend void doEpoll(RPCServer *server, int epfd);

    std::function<bool(int, int)> cmp = [this](int epfd0, int epfd1) -> bool
    {
        return epfds.at(epfd0) > epfds.at(epfd1);
    };

private:
    size_t task_thread;
    size_t epoll_buffer_size;

    ServerSocket serverSock;
    ThreadPool pool;
    RPCFramework framework;
    std::unordered_map<int, int> epfds;                                // epfds[i]: 监视的 socket 的数量
    std::unordered_map<int, TCPSocket *> clnts;                        // fd 对应的客户
    std::priority_queue<int, std::vector<int>, decltype(cmp)> pq{cmp}; // 小根堆，每次选择监视 socket 数量最小的 epfd
    log4cplus::Logger logger;                                          // 记录除了错误日志的其它日志
    log4cplus::Logger errorLogger;                                     // 记录错误日志

public:
    static constexpr size_t DEFAULT_THREAD_HOLD = 6;          // 默认线程数
    static constexpr size_t DEFAULT_BACKLOG = 511;            // 默认全连接数，即支持同时连接的用户个数 - 1
    static constexpr size_t DEFAULT_TASK_THREAD_HOLD = 6;     // 默认一个 epoll 实例最多可以开的线程数
    static constexpr size_t DEFAULT_EPOLL_BUFFER_SIZE = 1024; // 默认一个 epoll 实例的 buffer 的大小

    RPCServer(const std::string &ip, uint16_t port,
           size_t backlog = DEFAULT_BACKLOG,
           size_t epoll_buffer_size = DEFAULT_EPOLL_BUFFER_SIZE,
           size_t procedure_critical_time = RPCFramework::DEFAULT_CRITICAL_TIME,
           size_t thread_hold = DEFAULT_THREAD_HOLD, size_t task_thread_hold = DEFAULT_TASK_THREAD_HOLD)
        : serverSock(ip, port, backlog), epoll_buffer_size(epoll_buffer_size)
        ,pool(thread_hold), task_thread(task_thread_hold)
        ,framework(procedure_critical_time)
    {
        log4cplus::initialize();
        log4cplus::PropertyConfigurator::doConfigure("Log/config/log4cplus.properties"); // 配置文件的路径
        try
        {
            logger = log4cplus::Logger::getInstance("ServerLogger"); // 要使用的日志记录器
            errorLogger = log4cplus::Logger::getInstance("ErrorLogger");
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            exit(-1);
        }
        
        for (size_t i = 0; i < thread_hold; ++i) // 创建 epfd
        {
            int epfd = epoll_create(1024);
            epfds[epfd] = 0;
            pq.push(epfd);
            pool.enqueue(doEpoll, this, epfd);
        }
    }

    ~RPCServer()
    {
        serverSock.close();
    }

    void start(void)
    {
        epoll_event event;
        while (true)
        {
            TCPSocket *clnt = nullptr;
            try
            {
                clnt = serverSock.accept();
            }
            catch(const std::exception& e)
            {
                LOG4CPLUS_ERROR(errorLogger, e.what());
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            
            LOG4CPLUS_INFO(logger, "Connect with " + clnt->getIP() + ":" + std::to_string(clnt->getPort()));
            int target_epfd = pq.top();
            pq.pop();

            ++epfds[target_epfd];

            LOG4CPLUS_DEBUG(logger, "epfd " + std::to_string(target_epfd) + ", active connections: " + std::to_string(epfds[target_epfd]));

            pq.push(target_epfd);
            clnts[clnt->getSocket()] = clnt;

            event.data.fd = clnt->getSocket();
            event.events = EPOLLIN;

            epoll_ctl(target_epfd, EPOLL_CTL_ADD, clnt->getSocket(), &event);
            LOG4CPLUS_DEBUG(logger, "Add clnt to epfd " + std::to_string(target_epfd));
        }
    }

    template <typename Func>
    void registerProcedure(const std::string &name, Func procedure);

    template <typename Obj, typename Func>
    void registerProcedure(const std::string &name, Obj &obj, Func procedure);
};

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

void doEpoll(RPCServer *server, int epfd)
{
    ThreadPool pool(server->task_thread);
    epoll_event events[server->epoll_buffer_size];
    int loop = 0;
    while (true)
    {
        ++loop;
        int eventsNum = 0;
        auto startTime = std::chrono::high_resolution_clock::now();
        eventsNum = epoll_wait(epfd, events, sizeof(events), -1);
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        double dur_s = duration.count() / 1e6;
        if(dur_s > 1.0)
            LOG4CPLUS_DEBUG(server->logger, "epfd: " + std::to_string(epfd) 
            + " epoll_wait cost time: " 
            + std::to_string(dur_s) 
            + " s, event number: " + std::to_string(eventsNum));
        if (eventsNum == -1)
        {
            LOG4CPLUS_ERROR(server->errorLogger, "epoll_wait_error");
            continue;
        }
        
        std::vector<std::shared_ptr<TCPSocket>> closed_clients; // 使用智能指针管理

        for (size_t event = 0; event < eventsNum; ++event)
        {
            if(!server->clnts.count(events[event].data.fd)) // 有可能是过期的事件（已经处理）
                continue;
            auto clnt = server->clnts[events[event].data.fd];
            std::string IP = clnt->getIP();
            uint16_t port = clnt->getPort();

            try
            {
                std::string req = clnt->receive();
                std::string ret = server->framework.handleRequest(req);
                clnt->send(ret);
            }
            catch (const std::exception &excp)
            {
                // Client disconnected
                LOG4CPLUS_INFO(server->logger, "Connection with " +  IP + ":" + std::to_string(port) + " closed");
                epoll_ctl(epfd, EPOLL_CTL_DEL, clnt->getSocket(), NULL);
                closed_clients.emplace_back(clnt); // Collect closed client connections
            }

            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        startTime = std::chrono::high_resolution_clock::now();
        auto clntNum = closed_clients.size();

        // Clean up closed client connections
        // When the client is destroyed, it will automatically close the socket
        for (const auto &closed_clnt : closed_clients)
        {
            auto clnt_socket = closed_clnt->getSocket();
            server->clnts.erase(clnt_socket);
            --server->epfds[epfd];
            epoll_ctl(epfd, EPOLL_CTL_DEL, clnt_socket, NULL);
            LOG4CPLUS_DEBUG(server->logger, "Remove clnt from epfd " + std::to_string(epfd));
        }

        endTime = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        dur_s = duration.count() / 1e6;
        if(dur_s > 1.0)
            LOG4CPLUS_WARN(server->logger, "Close " + std::to_string(clntNum) + " clnt cost time: " + std::to_string(dur_s));
    }
}
