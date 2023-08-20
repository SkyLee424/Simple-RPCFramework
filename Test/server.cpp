#include "RPCServer.hpp"

int sub(int a, int b)
{
    return a - b;
}

void func0(void)
{
    std::cout << "func!!!" << std::endl;
}

void func1(void)
{
    return func0();
}

int main(void)
{
    RPCServer server("192.168.124.114", 1145);
    std::function<int(int, int)> add = [](int a, int b)
    {
        return a + b;
    };
    std::function<void(int)> show = [](int a)
    {
        std::cout << a << std::endl;
    };
    server.registerProcedure("add", add);
    server.registerProcedure("sub", sub);
    // server.registerProcedure("show", show); // 暂时不支持返回值为 void 的函数
    server.start();
}