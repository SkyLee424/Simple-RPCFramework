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
#include <fcntl.h>

class RPCServer;

void doEpoll(RPCServer *server, int epfd);

class RPCServer
{
    friend void doEpoll(RPCServer *server, int epfd);
private:
    size_t task_thread;
    size_t epoll_buffer_size;
    int epfd;

    std::unordered_map<int, std::shared_ptr<TCPSocket>> clnts;

    ServerSocket serverSock;
    RPCFramework framework;
    
    log4cplus::Logger logger;                                               // 记录除了错误日志的其它日志
    log4cplus::Logger errorLogger;                                          // 记录错误日志

public:
    static constexpr size_t DEFAULT_BACKLOG = 511;            // 默认全连接数，即支持同时连接的用户个数 - 1
    static constexpr size_t DEFAULT_TASK_THREAD_HOLD = 6;     // 默认一个 epoll 实例最多可以开的线程数
    static constexpr size_t DEFAULT_EPOLL_BUFFER_SIZE = 1024; // 默认一个 epoll 实例的 buffer 的大小

    RPCServer(const std::string &ip, uint16_t port,
           size_t backlog = DEFAULT_BACKLOG,
           size_t epoll_buffer_size = DEFAULT_EPOLL_BUFFER_SIZE,
           size_t procedure_critical_time = RPCFramework::DEFAULT_CRITICAL_TIME,
           size_t task_thread_hold = DEFAULT_TASK_THREAD_HOLD);
        

    ~RPCServer();

    void start(void);

    template <typename Func>
    void registerProcedure(const std::string &name, Func procedure);

    template <typename Obj, typename Func>
    void registerProcedure(const std::string &name, Obj &obj, Func procedure);
};

RPCServer::RPCServer(const std::string &ip, uint16_t port,
                     size_t backlog,
                     size_t epoll_buffer_size,
                     size_t procedure_critical_time,
                     size_t task_thread_hold)
    : serverSock(ip, port, backlog), epoll_buffer_size(epoll_buffer_size), epfd(epoll_create(1024)), task_thread(task_thread_hold), framework(procedure_critical_time)
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
        std::cerr << e.what() << '\n';
        exit(-1);
    }

    std::thread handle_thread(doEpoll, this, epfd); // 子线程处理客户端的请求（包括断开连接请求）
    handle_thread.detach();
}

inline
RPCServer::~RPCServer()
{
    serverSock.close();
    close(epfd); // 关闭 epoll 实例
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

void RPCServer::start(void)
{
    epoll_event event;
    while (true)
    {
        std::shared_ptr<TCPSocket> clnt(nullptr);
        int clnt_sock = -1;
        try
        {
            clnt.reset(serverSock.accept()); // 主线程处理连接请求
            clnt_sock = clnt->getSocket();
            // 设置为非阻塞 IO
            int flag = fcntl(clnt_sock, F_GETFL);
            flag |= O_NONBLOCK;
            fcntl(clnt_sock, F_SETFL, flag);
        }
        catch(const std::exception& e)
        {
            LOG4CPLUS_ERROR(errorLogger, e.what());
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }
        
        LOG4CPLUS_INFO(logger, "Connect with " + clnt->getIP() + ":" + std::to_string(clnt->getPort()));
        
        // 是否需要等待 doEpoll 线程清理套接字？
        clnts.insert({clnt_sock, clnt}); // 使用 insert，而不是 operator[]

        event.data.fd = clnt_sock;
        event.events = EPOLLIN | EPOLLET; // 边缘触发

        if(epoll_ctl(epfd, EPOLL_CTL_ADD, clnt_sock, &event) < 0)
        {
            std::string errorMsg(strerror(errno));
            LOG4CPLUS_ERROR(errorLogger, "Add clnt to epfd failed, " + errorMsg);
            continue;
        }
    }
}

void doEpoll(RPCServer *server, int epfd)
{
    ThreadPool pool(server->task_thread);
    epoll_event events[server->epoll_buffer_size];

    while (true)
    {
        int eventsNum = epoll_wait(epfd, events, server->epoll_buffer_size, -1);

        if (eventsNum == -1)
        {
            std::string errorMsg(strerror(errno));
            LOG4CPLUS_ERROR(server->errorLogger, "epoll_wait_error: " + errorMsg);
            continue;
        }
        
        std::unordered_set<std::shared_ptr<TCPSocket>> closed_clients; // 使用智能指针管理
        std::atomic<int> complete = 0;                          // 完成任务的线程数量
        std::mutex vlock;                                       // 保护 closed_clients;
        std::condition_variable cond;

        // 还有一种情况：由于网络拥塞，客户端的断开连接请求先到，其它请求来得比较晚，导致服务器先断开连接，后续到的请求就无法处理了
        // 可不可以试试设置等待时间，超过该时间，服务器直接释放连接，后续请求不做处理？
        for (size_t event = 0; event < eventsNum; ++event)
        {
            size_t clnt_sock = events[event].data.fd;
            std::shared_ptr<TCPSocket> clnt = nullptr;
            try
            {
                clnt = server->clnts.at(clnt_sock); // 使用 at，而不是 operator[]
            }
            catch(const std::exception& e)
            {
                LOG4CPLUS_WARN(server->logger, "No such clnt, clnt_sock: " + std::to_string(clnt_sock) + ", trying to get it again...");
                --event;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
            
            // 提交任务到线程池，后台执行
            pool.enqueue([&, server](std::shared_ptr<TCPSocket> _clnt, std::mutex &_vlock) 
            {
                std::string IP = _clnt->getIP();
                uint16_t port = _clnt->getPort();
                std::string req = "";
                
                try
                {
                    req = _clnt->receive();
                }
                catch (const std::exception &excp)
                {
                    std::string excp_msg = excp.what();
                    if(excp_msg != "recv error: Success")
                        LOG4CPLUS_WARN(server->logger, "Receive message from clnt error, error message: " + excp_msg);
                    else 
                        LOG4CPLUS_INFO(server->logger, "Connection with " + IP + ":" + std::to_string(port) + " closed");
                    // Client disconnected
                    std::lock_guard<std::mutex> lock(_vlock);
                    closed_clients.insert(_clnt); // Collect closed client connections
                } 

                std::string ret = server->framework.handleRequest(req); // 暂时不处理异常
                if(!closed_clients.count(_clnt)) // 确保该客户仍正常连接，否则会引发管道破裂
                    _clnt->send(ret);

                ++complete;
                if(complete == eventsNum)
                    cond.notify_all();
            }, clnt, std::ref(vlock));
        }

        std::unique_lock<std::mutex> ulock(vlock);
        while (complete != eventsNum) // 等待所有任务执行完毕，否则可能无法正确关闭客户端套接字
        {
            cond.wait(ulock);
        }

        {
            // 这里需不需要与主线程同步？

            // Clean up closed client connections
            // When the client is destroyed, it will automatically close the socket
            for (const auto &closed_clnt : closed_clients)
            {
                auto clnt_sock = closed_clnt->getSocket();
                server->clnts.erase(clnt_sock);
                if(epoll_ctl(epfd, EPOLL_CTL_DEL, clnt_sock, NULL) < 0)
                {
                    std::string errorMsg(strerror(errno));
                    LOG4CPLUS_ERROR(server->errorLogger, "Remove clnt from epfd " 
                    + std::to_string(epfd) + " error: " 
                    + errorMsg);
                }
            }            
        }
    }
}
