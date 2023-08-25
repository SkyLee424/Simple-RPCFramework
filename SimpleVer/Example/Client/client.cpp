#include <iostream>
#include "RPCClient.hpp"

int main(void)
{
    RPCClient clnt("192.168.124.114", 1145);

    int ret0 = clnt.remoteCall("add", 1, 1);
    int ret1 = clnt.remoteCall("subtract", 1, 1);
    int ret2 = clnt.remoteCall("multiply", 4, 2);
    int ret3 = clnt.remoteCall("divide", 4, 2);
    std::cout << "1 + 1 = " << ret0 << std::endl;
    std::cout << "1 - 1 = " << ret1 << std::endl;
    std::cout << "4 * 2 = " << ret2 << std::endl;
    std::cout << "4 / 2 = " << ret3 << std::endl;

    // 异常情况
    try
    {
        clnt.remoteCall("???", 1, 1);
    }
    catch(std::runtime_error &e)
    {
        std::cout << e.what() << std::endl;
    }

    try
    {
        clnt.remoteCall("divide", 1, 0);
    }
    catch(const std::runtime_error& e)
    {
        std::cerr << e.what() << std::endl;
    }
}