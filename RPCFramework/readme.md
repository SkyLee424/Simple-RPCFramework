# RPCFramework

----

2024.02.13 更新：重写服务器架构，采用多 Reactor 多线程模型，QPS 较之前提升近一倍

----

9 月 12 日更新：几乎重写了服务器架构，仅采用一个 epoll 实例，解决了之前的段错误问题

---- 

8 月 27 日更新：重写了序列化与反序列化，支持常用 STL，自定义类型的序列化变得更加简单！

----

## 架构概述

该RPC框架包括以下组件：

- **Procedure.hpp**: 封装了「过程」的名称和参数 
- **ReturnPacket.hpp**: 封装了返回结果
- **Serializer.hpp**: 工具类，用于序列化和反序列化对象
- **RPCFramework.hpp**: RPC 框架类，是 RPC 的核心
- **RPCServer.hpp**: 服务端类，基于多 Reactor 多线程模型
- **RPCClient.hpp**: 客户端类，有一个核心模版成员函数 `remoteCall`

## 功能与特点

### 框架部分

- 使用 **模版元编程** ，支持注册任意普通函数、std::function 对象、类成员函数
- 使用 Serializer 类来实现任意对象的 **序列化和反序列化**

### 服务端部分

- 封装 TCPSocket、TaskQueue 类
- 基于 **多 Reactor 多线程** 模型实现 Server 端
- 使用 **epoll** 监听事件，TaskQueue 异步处理客户端的请求
- 使用 log4cplus 记录日志

## 示例

示例代码已经放在 `RPCFramework/Example` 中

### 服务端

```cpp
#include "RPCServer.hpp"
#include "TestClass.hpp"
#include "Procedures.hpp"


int main(void)
{
    RPCServer server("192.168.124.114", 1145, 60000);

    std::function<int(int, int)> add = [](int a, int b)
    {
        return a + b;
    };
    std::function<void(int)> show = [](int a)
    {
        std::cout << a << std::endl;
    };

    server.registerProcedure("add", add);                   // 测试对 std::function 对象的支持
    server.registerProcedure("sub", sub);                   // 测试对函数指针的支持
    server.registerProcedure("show", show);                 // 测试对 void 返回类型的支持
    server.registerProcedure("func1", func1);               // 测试对嵌套函数的支持
    server.registerProcedure("hello", hello);               // 测试对 std::string 返回类型的支持
    server.registerProcedure("getHeXin", getHeXin);         // 测试参数、返回值类型为自定义类型的情况
    server.registerProcedure("testString", testString);     // 测试参数中含有 std::string 的情况
    server.registerProcedure("testExcp", testExcp);         // 测试在函数中，抛出异常的情况
    server.registerProcedure("testTimeOut", testTimeOut);   // 测试函数运行时间过长的情况
    server.registerProcedure("getSum", getSum);             // 测试对容器的支持
    server.registerProcedure("twoSum", twoSum);             // 测试对容器的支持
    server.registerProcedure("getManyHeXin", getManyHeXin); // 测试容器内存储自定义类型的支持

    Foo foo;
    server.registerProcedure("Foo::test1", foo, &Foo::test1); // 测试对成员函数的支持

    server.start(); // 服务器，启动！
}
```

观察示例代码发现：

- 易于使用，只需要提供「过程」的名称，以及「过程」，就可以轻松注册

### 客户端

```cpp
#include <iostream>
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
        std::cout << clnt.remoteCall<std::string>("hello") << std::endl;                                          // std::string return type
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
```

可以发现，向服务端请求调用，只需要提供「过程」的名称，以及对应的参数即可

## 测试

完整的测试代码均在 `RPCFramework/Example/` 下

注意编译的时候，C++ 标准大于等于 C++17，并且链接 log4cplus 和 pthread 库

<!-- 这里可以补充并发量的测试结果 -->
本地配置：

Server 与 Client 共用一台虚拟机，虚拟机配置如下：

- OS：CentOS7（Linux Kernel 3.10）
- CPU：2 Core
- Mem：4G

Server 与 Client 均适用测试代码：

- Server 采用 **一主一从** Reactor，从 Reactor 分配 8 个线程，backlog 为 60000
- Client 使用 **测试方法二**，200 并发线程，单个线程发起 1000*3 个请求

测试结果如下：

![](images/截屏2024-02-13%2019.28.47.png)

计算可知 **QPS 约为 21238**，较之前版本高出近一倍

## 注意事项

对于自定义类型，需要继承 Serializable 类，并重写 `Serialize` 和 `DeSerialize` 方法，例如，对于自定义类型 People：

```cpp
class People : public Serializable
{
public:
    std::string name;
    int age;
    std::string BinZhou;

    People() = default;

    static std::istream &DeSerialize(std::istream &is, People &people)
    {
        Serializable::DeSerialize(is, people.name);
        Serializable::DeSerialize(is, people.age);
        Serializable::DeSerialize(is, people.BinZhou);
        return is;
    }

    static std::ostream &Serialize(std::ostream &os, const People &people)
    {
        Serializable::Serialize(os, people.name);
        Serializable::Serialize(os, people.age);
        Serializable::Serialize(os, people.BinZhou);
        return os;
    }
};
```

在测试并发量时，需要保证 Server 的 backlog 足够大，以及系统的文件描述符限制：

```cpp
RPCServer server("192.168.124.114", 1145, 60000); // backlog == 60000
```

![](images/截屏2023-08-25%2018.10.53.png)

这种情况下，即使 backlog 为 60000，理论上同时连接的客户端最大数量也不能超过 4096（实际上，会更少，因为标准输入、标准输出、服务器套接字、epoll 实例等会占用一部分文件描述符）

----

接下来的部分主要是对代码的关键部分的解释

## 过程参数的保存

客户端向服务器发出的请求，实际上就是序列化后的一个「参数包」

该「参数包」包含两部分内容：

- 过程的名称
- 传递的参数

其中，由于传递的参数可能具有多种类型，因此，使用 tuple 保存

```cpp
template <typename ...Args>
struct ProcedurePacket
{
    std::string name; // 过程的名称
    std::tuple<Args ...> t; // tuple 用于保存传递的参数

    ProcedurePacket() = default; // 缺陷：Args 不能包含引用类型的参数

    ProcedurePacket(const std::string &name, Args... args)
        :name(name), t(std::make_tuple(args...)) {}

    template <typename ...X>
    static std::ostream& Serialize(std::ostream &os, const ProcedurePacket<X ...> &packet);

    template <typename ...X>
    static std::istream& DeSerialize(std::istream &is, ProcedurePacket<X ...> &packet);
};
```

## 支持任意函数（包括 void 返回类型）的实现

### procedures 成员变量

为了不让我们的 RPCFramework 类成为模版类，在存储「过程」的时候，就不能使用模版

例如：

```cpp
template <typename R, typename Args>
std::unordered_map<std::string, std::function<R(Args ...)>> procedures;
```

如果 RPCFramework 包含这个成员的话，就成为模版类了

因此，正确的成员声明如下：

```cpp
std::unordered_map<std::string, std::function<std::string(const std::string&)>> procedures;
```

其中，存储的「过程」为 `std::function<std::string(const std::string&)>`

- 参数是客户端传过来的序列化后的数据
- 返回值是将调用结果序列化后的数据

### registerProcedure 成员函数

该成员函数用于注册一个「过程」，第一个参数是「过程」的名称，第二个参数是一个模版参数，即「过程」

当然，为了实现对成员函数的支持，还重载了一个版本，该版本多了一个参数，用于保存是哪个成员

registerProcedure 成员函数的主要作用是：**将「过程」绑定到 `callProxy` 成员函数上**

```cpp
// bind callProxy 的函数指针，记得传入 this 指针，因为 callProxy 不是静态的
procedures[name] = std::bind(&RPCFramework::callProxy<Func>, this, procedure, std::placeholders::_1);
```

### callProxy 成员函数

该成员函数实际上是一个中间件，是 `registerProcedure` 到 `callProxyHelper` 的桥梁

```cpp
template <typename Func>
std::string RPCFramework::callProxy(Func f, const std::string &req)
{
    return callProxyHelper(f, req);
}
```

### callProxyHelper 成员函数

该成员函数是实现 **支持任意「过程」的关键**

它有三个重载版本，分别对应了三大类「过程」：

```cpp
template <typename R, typename ...Args>
std::string RPCFramework::callProxyHelper(std::function<R(Args ...)> f, const std::string &req);

template <typename R, typename ...Args>
std::string RPCFramework::callProxyHelper(R(*f)(Args ...), const std::string &req);   

template <typename R, typename Obj, typename ...Args>
std::string RPCFramework::callProxyHelper(Obj &obj, R(Obj::*f)(Args...), const std::string &req);
```

可以看出：callProxyHelper 实现了将 callProxy 中抽象的「过程」f 转换为了一个详细的「过程」，包含了返回值类型 `R`、参数列表 `Args...`

而函数体中包含了一个工具函数 invoke，用于调用「过程」 f

### invoke 工具函数

```cpp
// 调用中间件，当 R 不为 void 时，调用该版本
template <typename R, typename Function, typename Tuple>
typename std::enable_if<!std::is_same<R, void>::value, typename RetType<R>::type>::type
invoke(Function &&f, Tuple &&tuple)

// 当 R 为 void 时，调用该版本，返回 0（占位，无实际意义）
template <typename R, typename Function, typename Tuple>
typename std::enable_if<std::is_same<R, void>::value, typename RetType<R>::type>::type;
```

该工具函数有两个重载版本，是 **支持 void 返回类型的「过程」的核心**

使用到了 `std::enable_if` 和 `std::is_same` 两个标准库函数，用于区分到底该调用哪个 invoke

invoke 工具函数内部又涉及到了 apply_tuple 工具函数

### apply_tuple 与 apply_tuple_impl 工具函数

```cpp
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
```

apply_tuple 函数主要的作用就是为 apply_tuple_impl 函数服务的

关键在于 `std::make_index_sequence<tuple_size>()`，该函数生成一个整数序列。这个序列从 0 开始，一直递增到给定的长度减 1。

生成的序列主要是用于传给 `apply_tuple_impl`，这样，`apply_tuple_impl` 就可以根据 index 来解析 tuple 中的每一个参数了

解析完参数后，就可以真正执行目标「过程」了

## 服务端「过程」的调用步骤

经过上面的原理分析，可以得出服务端「过程」的调用步骤：

- 服务端接收到用户序列化后的请求后，将请求传给 RPCFramework 的 `handleRequest`
- `handleRequest` 将该请求反序列化，得到要调用的「过程」的名称
- `handleRequest` 根据「过程」的名称，调用指定的「过程」，即 `procedures[name]`
- 得到调用的结果后，将其序列化，返回给上层，即服务端，由服务端实现数据的传输

而 `procedures[name]` 的调用又可以分为以下步骤：

- `procedures[name]` 绑定了 `callProxy`，因此调用 `callProxy`
- `callProxy` 调用特定版本的 `callProxyHelper`
- `callProxyHelper` 将过程解析后，调用 `invoke` 工具函数
- `invoke` 工具函数调用 `apply_tuple` 工具函数
- `apply_tuple` 工具函数根据 tuple 的大小，生成一个序列，并将该序列传给 `apply_tuple_impl` 工具函数
- `apply_tuple_impl` 工具函数将 tuple 展开，传给真正的「过程」
- 得到返回结果后，逐级返回

## 序列化与反序列化的实现

序列化和反序列化主要是通过 Serializer 工具类来实现的：

```cpp
class Serializer
{
public:
    template <typename T>
    static std::string Serialize(T &object);

    template <typename T>
    static T
    Deserialize(const std::string &serializedData);
};
```

函数体部分可以参看源码部分

支持常用 STL

自定义类型需要继承 Serializable 类，并重写 `Serialize` 和 `DeSerialize` 方法

Serializer 工具类也已经上传到另一个 [仓库](https://github.com/SkyLee424/Simple-Serialize-Tool) 了，可以去看看

## 服务端「接受用户请求--处理用户请求--返回调用结果」的过程

整个服务端的架构如图所示：

![](images/截屏2024-02-13%2020.13.08.png)

### 工作流程

#### 接受用户连接请求

接受用户连接请求这一步由 `主 Reactor` 完成：

- `主 Reactor` accept 客户的请求
- 选择活跃连接数最少的 `从 Reactor`，并将客户端套接字分配到该 Reactor 中

#### 处理用户请求与返回调用结果

处理用户请求与返回调用结果这一步由 `从 Reactor` 完成：

- epoll_wait 返回后，遍历 epoll_events：
- 如果是读事件：
  - 如果是关闭连接请求，关闭即可
  - 否则，将用户请求扔到 TaskQueue
- 如果是写事件，那么写入响应给客户端

虽然 socket 可以保证收到的顺序与客户端的请求顺序一致，但是我们应该按照相同的顺序处理，避免正常请求在关闭连接请求之后处理

TaskQueue 中的 Worker 会获取自己对应队列的请求，并处理，**保证单个 Client 请求的有序性**

处理完毕后，worker 会注册写事件，让所属 `从 Reactor` 完成写入响应的操作
