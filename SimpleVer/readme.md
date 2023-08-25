# 简单 RPC 框架

这是一个简单的RPC（远程过程调用）框架，它允许客户端调用服务器上注册的远程过程，并接收结果。

## 架构概述

该RPC框架包括以下组件：

- **RPCServer**: 服务器端组件，负责注册远程过程、监听客户端连接、处理客户端请求并发送响应。
- **RPCClient**: 客户端组件，负责连接服务器、发起远程过程调用请求并处理服务器的响应。

## 服务器端

### RPCServer类

- `RPCServer(const std::string &ip, uint16_t port)`: 构造函数，初始化服务器的IP地址和端口。
- `~RPCServer()`: 析构函数，关闭服务器。
- `void start()`: 启动服务器，监听客户端连接，处理客户端请求。
- `void registerProcedure(const std::string &name, std::function<int(int, int)> procedure)`: 注册远程过程。
- `std::string handleRequest(const std::string &request)`: 处理客户端请求并返回响应。

### 使用示例

```cpp
#include "RPCServer.hpp"
#include "Procedure.hpp"

int main(void)
{
    RPCServer server("192.168.124.114", 1145); // 指定服务器的 IP 和端口号

    // 注册「过程」
    server.registerProcedure("add", add);
    server.registerProcedure("subtract", subtract);
    server.registerProcedure("multiply", multiply);
    server.registerProcedure("divide", divide);

    server.start(); // RPC 服务器，启动！
}
```

## 客户端

### RPCClient类

- `RPCClient(const std::string &ip, uint16_t port)`: 构造函数，初始化客户端，连接服务器。
- `~RPCClient()`: 析构函数，关闭客户端。
- `int remoteCall(const std::string &procedureName, int arg0, int arg1)`: 远程调用远程过程。

### 使用示例

```cpp
#include <iostream>
#include "RPCClient.hpp"

int main(void)
{
    RPCClient clnt("192.168.124.114", 1145); // 指定服务器的 IP 和端口，并连接

    // 向服务器发出调用请求
    int ret0 = clnt.remoteCall("add", 1, 1);
    int ret1 = clnt.remoteCall("subtract", 1, 1);
    int ret2 = clnt.remoteCall("multiply", 4, 2);
    int ret3 = clnt.remoteCall("divide", 4, 2);

    // 输出结果
    std::cout << "1 + 1 = " << ret0 << std::endl;
    std::cout << "1 - 1 = " << ret1 << std::endl;
    std::cout << "4 * 2 = " << ret2 << std::endl;
    std::cout << "4 / 2 = " << ret3 << std::endl;

    // 异常情况
    try
    {
        clnt.remoteCall("???", 1, 1); // 服务器不存在（未注册该过程）
    }
    catch(std::runtime_error &e)
    {
        std::cout << e.what() << std::endl;
    }

    try
    {
        clnt.remoteCall("divide", 1, 0); // 除数为 0，即过程的调用中发生异常
    }
    catch(const std::runtime_error& e)
    {
        std::cerr << e.what() << std::endl;
    } 
}
```

## 运行结果

服务端：

![](images/截屏2023-08-25%2017.33.02.png)

客户端：

![](images/截屏2023-08-25%2017.33.44.png)

## 注意事项

- 该框架仅用于演示目的，不适用于生产环境。
- 框架中的 Socket 类是自己封装的，你可以根据实际需要选择更适合的库。