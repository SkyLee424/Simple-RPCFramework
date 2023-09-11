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
    std::unordered_map<int, int> epfds;                                                 // epfds[i]: 监视的 socket 的数量
    std::unordered_map<int, std::unordered_map<int, std::shared_ptr<TCPSocket>>> clnts; // 每个 epfd 单独有一个 unordered_map 实例
    std::unordered_map<int, std::pair<std::mutex, std::condition_variable>> clnts_lock; // 每个 epfd 对应的客户的锁
    std::priority_queue<int, std::vector<int>, decltype(cmp)> pq{cmp};      // 小根堆，每次选择监视 socket 数量最小的 epfd
    log4cplus::Logger logger;                                               // 记录除了错误日志的其它日志
    log4cplus::Logger errorLogger;                                          // 记录错误日志

public:
    static constexpr size_t DEFAULT_EPOLL_HOLD = 6;           // 默认创建 epoll 实例数
    static constexpr size_t DEFAULT_BACKLOG = 511;            // 默认全连接数，即支持同时连接的用户个数 - 1
    static constexpr size_t DEFAULT_TASK_THREAD_HOLD = 6;     // 默认一个 epoll 实例最多可以开的线程数
    static constexpr size_t DEFAULT_EPOLL_BUFFER_SIZE = 1024; // 默认一个 epoll 实例的 buffer 的大小

    RPCServer(const std::string &ip, uint16_t port,
           size_t backlog = DEFAULT_BACKLOG,
           size_t epoll_buffer_size = DEFAULT_EPOLL_BUFFER_SIZE,
           size_t procedure_critical_time = RPCFramework::DEFAULT_CRITICAL_TIME,
           size_t epoll_hold = DEFAULT_EPOLL_HOLD, size_t task_thread_hold = DEFAULT_TASK_THREAD_HOLD)
        : serverSock(ip, port, backlog), epoll_buffer_size(epoll_buffer_size)
        ,pool(epoll_hold), task_thread(task_thread_hold)
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

        for (size_t i = 0; i < epoll_hold; ++i) // 创建 epfd
        {
            int epfd = epoll_create(1024);
            epfds[epfd] = 0;
            pq.push(epfd);
            clnts[epfd].clear();
            clnts_lock[epfd];  // 初始化锁和条件变量
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
            std::shared_ptr<TCPSocket> clnt(nullptr);
            uint16_t clnt_sock = -1;
            try
            {
                clnt.reset(serverSock.accept());
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
            for(auto & e : clnts)
            {
                int cur_epfd = e.first;
                std::unique_lock<std::mutex> ulock(clnts_lock[cur_epfd].first);
                while (e.second.count(clnt_sock))
                {
                    clnts_lock[cur_epfd].second.wait(ulock);
                }
            }
            
            int target_epfd = pq.top();
            pq.pop();

            ++epfds[target_epfd];

            pq.push(target_epfd);
            clnts[target_epfd][clnt_sock] = clnt;

            event.data.fd = clnt_sock;
            event.events = EPOLLIN | EPOLLET; // 边缘触发

            if(epoll_ctl(target_epfd, EPOLL_CTL_ADD, clnt_sock, &event) < 0)
            {
                std::string errorMsg(strerror(errno));
                LOG4CPLUS_ERROR(errorLogger, "Add clnt to epfd failed, " + errorMsg);
                continue;
            }
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
    std::unordered_map<int, std::shared_ptr<TCPSocket>> &clnts = server->clnts[epfd];
    std::mutex &clnt_lock = std::ref(server->clnts_lock[epfd].first);
    std::condition_variable &clnt_cond = std::ref(server->clnts_lock[epfd].second);

    while (true)
    {
        int eventsNum = 0;

        auto startTime = std::chrono::steady_clock::now();
        eventsNum = epoll_wait(epfd, events, sizeof(events), -1);
        auto endTime = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
        size_t dur_ms = duration.count();

        if (eventsNum == -1)
        {
            std::string errorMsg(strerror(errno));
            LOG4CPLUS_ERROR(server->errorLogger, "epoll_wait_error: " + errorMsg);
            continue;
        }
        
        std::vector<std::shared_ptr<TCPSocket>> closed_clients; // 使用智能指针管理
        std::atomic<int> complete = 0;                          // 完成任务的线程数量
        std::mutex vlock;                                       // 保护 closed_clients;
        std::condition_variable cond;

        // 还有一种情况：由于网络拥塞，客户端的断开连接请求先到，其它请求来得比较晚，导致服务器先断开连接，后续到的请求就无法处理了
        // 可不可以试试设置等待时间，超过该时间，服务器直接释放连接，后续请求不做处理？
        for (size_t event = 0; event < eventsNum; ++event)
        {
            size_t clnt_sock = events[event].data.fd; // SIGSEGV ？？
            auto clnt = clnts[clnt_sock]; // 这里需要线程同步，确保有 clnt_sock，让该线程先执行清理操作，再让主线程将新的客户端套接字添加到 clnts 中
            std::string IP = clnt->getIP();
            uint16_t port = clnt->getPort();
            // 提交任务到线程池，后台执行
            pool.enqueue([&, IP, port, clnt, server](std::mutex &_vlock) 
            {
                try
                {
                    std::string req = clnt->receive();
                    std::string ret = server->framework.handleRequest(req);
                    clnt->send(ret);
                    ++complete;
                    if(complete == eventsNum)
                        cond.notify_all();
                }
                catch (const std::exception &excp)
                {
                    // Client disconnected
                    LOG4CPLUS_INFO(server->logger, "Connection with " + IP + ":" + std::to_string(port) + " closed");
                    std::lock_guard<std::mutex> lock(_vlock);
                    closed_clients.emplace_back(clnt); // Collect closed client connections
                    ++complete;
                    if(complete == eventsNum)
                        cond.notify_all();
                } 
            },std::ref(vlock));
        }

        std::unique_lock<std::mutex> ulock(vlock);
        while (complete != eventsNum) // 等待所有任务执行完毕，否则可能无法正确关闭客户端套接字
        {
            cond.wait(ulock);
        }

        {
            std::lock_guard<std::mutex> lock(clnt_lock);

            // Clean up closed client connections
            // When the client is destroyed, it will automatically close the socket
            for (const auto &closed_clnt : closed_clients)
            {
                auto clnt_sock = closed_clnt->getSocket();
                clnts.erase(clnt_sock);
                --server->epfds[epfd];
                if(epoll_ctl(epfd, EPOLL_CTL_DEL, clnt_sock, NULL) < 0)
                {
                    std::string errorMsg(strerror(errno));
                    LOG4CPLUS_ERROR(server->errorLogger, "Remove clnt from epfd " 
                    + std::to_string(epfd) + " error: " 
                    + errorMsg);
                }
            }            
        }
        clnt_cond.notify_all();
    }
}
