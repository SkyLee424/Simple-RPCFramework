# RPC 框架

本仓库提供了两个 RPC 框架

- SimpleVer：一个简单的 RPC 框架
- RPCFramework：完整的 RPC 框架

## SimpleVer

- 仅 **用于演示 RPC 的整个过程**
- 仅支持注册特定的「过程」，**不支持任意过程**
- 仅支持一个客户端的请求
- 利用 nlohmann::json 实现 *过程以及参数* 的序列化和反序列化

## RPCFramework

- 使用 **模版元编程** ，支持注册任意普通函数、std::function 对象、类成员函数
- 使用 Serializer 类来实现任意对象的 **序列化和反序列化**
- 使用 **ThreadPool + epoll** 处理客户端的请求
- 使用 std::priority_queue 实现简单的 **负载均衡**
- 使用 shared_ptr 管理客户端套接字，确保连接正确释放
- 使用 log4cplus 记录日志

## 注意事项

- SimpleVer 的代码使用 C++11 以及 nlohmann::json，确保你的编译器支持 C++11，并且有 nlohmann 库
- RPCFramework 的代码由于使用到了 C++14 模版元编程的新特性，因此编译器至少要支持 C++14，此外，由于使用了 log4cplus 记录日志，因此，需要 log4plus 库的支持
- 使用 RPCFrameWork 时，如果注册的过程包含自定义类型，或者其它没有重载 `operator<<` 和 `operator>>` 的类型，需要重载，这是为了确保对象能够正确序列化
- 使用 RPCFrameWork 时，需要注意系统允许的最大描述符的数量（可以使用 `ulimit -a` 查看），如果过小，会导致同时连接的客户端数量较小

## 已知问题

- RPCFramework 在释放连接的过程中，有较低可能出现段错误（待解决）

详细的介绍分别在 `SimpleVer/readme.md` 和 `RPCFramework/readme.md` 中

由于测试可能不够完善，如果使用或者测试的时候，发现 bug ，欢迎在 issue 提出