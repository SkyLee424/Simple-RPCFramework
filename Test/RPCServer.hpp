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

public:
    static constexpr size_t DEFAULT_THREAD_HOLD = 6;          // 默认线程数
    static constexpr size_t DEFAULT_BACKLOG = 5;              // 默认全连接数，即支持同时连接的用户个数 - 1
    static constexpr size_t DEFAULT_TASK_THREAD_HOLD = 6;     // 默认一个 epoll 实例最多可以开的线程数
    static constexpr size_t DEFAULT_EPOLL_BUFFER_SIZE = 1024; // 默认一个 epoll 实例的 buffer 的大小

    RPCServer(const std::string &ip, uint16_t port,
           size_t backlog = DEFAULT_BACKLOG,
           size_t epoll_buffer_size = DEFAULT_EPOLL_BUFFER_SIZE,
           size_t thread_hold = DEFAULT_THREAD_HOLD, size_t task_thread_hold = DEFAULT_TASK_THREAD_HOLD)
        : serverSock(ip, port, backlog), epoll_buffer_size(epoll_buffer_size)
        ,pool(thread_hold), task_thread(task_thread_hold)
    {
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
            auto clnt = serverSock.accept();
            // std::cout << "[Info] Connect with " << clnt->getIP() << ":" << clnt->getPort() << std::endl;
            int target_epfd = pq.top();
            pq.pop();

            ++epfds[target_epfd];

            // std::cout << "epfd " << target_epfd << ", active connections: " << epfds[target_epfd] << std::endl;

            pq.push(target_epfd);
            clnts[clnt->getSocket()] = clnt;

            event.data.fd = clnt->getSocket();
            event.events = EPOLLIN;

            epoll_ctl(target_epfd, EPOLL_CTL_ADD, clnt->getSocket(), &event);
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
        eventsNum = epoll_wait(epfd, events, sizeof(events), -1);

        if (eventsNum == -1)
        {
            std::cout << "[Error] Thread " << std::this_thread::get_id() << ": epoll_wait error" << std::endl;
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
                // std::cout << "[Info] Connection with " << IP << ":" << port << " closed." << std::endl;
                epoll_ctl(epfd, EPOLL_CTL_DEL, clnt->getSocket(), NULL);
                closed_clients.emplace_back(clnt); // Collect closed client connections
            }

            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        
        // Clean up closed client connections
        for (const auto &closed_clnt : closed_clients)
        {
            auto clnt_socket = closed_clnt->getSocket();
            server->clnts.erase(clnt_socket);
            --server->epfds[epfd];
            epoll_ctl(epfd, EPOLL_CTL_DEL, clnt_socket, NULL);
        }
    }
}
