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

    server.registerProcedure("add", add);                 // 测试对 std::function 对象的支持
    server.registerProcedure("sub", sub);                 // 测试对函数指针的支持
    server.registerProcedure("show", show);               // 测试对 void 返回类型的支持
    server.registerProcedure("func1", func1);             // 测试对嵌套函数的支持
    server.registerProcedure("hello", hello);             // 测试对 std::string 返回类型的支持
    server.registerProcedure("getHeXin", getHeXin);       // 测试参数、返回值类型为自定义类型的情况
    server.registerProcedure("testString", testString);   // 测试参数中含有 std::string 的情况
    server.registerProcedure("testExcp", testExcp);       // 测试在函数中，抛出异常的情况
    server.registerProcedure("testTimeOut", testTimeOut); // 测试函数运行时间过长的情况

    Foo foo;
    server.registerProcedure("Foo::test1", foo, &Foo::test1); // 测试对成员函数的支持

    server.start();
}
