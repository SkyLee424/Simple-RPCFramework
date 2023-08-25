#pragma once

#include <sstream>
#include <chrono>
#include <thread>
#include "TestClass.hpp"

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

std::string hello(void)
{
    return std::string("hello, clnt!\nhahaha");
}

void testString(int a, std::string s1, double b, std::string s2)
{
    std::cout << "a = " << a << "\n"
              << "b = " << b << "\n"
              << "s1 = " << s1 << "\n"
              << "s2 = " << s2 << "\n";
}

void testExcp(void)
{
    throw std::runtime_error("in excp, test exception..");
}

int testTimeOut(void)
{
    std::this_thread::sleep_for(std::chrono::seconds(5));
    return 114514;
}

People getHeXin(People people) 
{
    std::stringstream iss("'He Xin'\n19\n'0.618 cm'");
    iss >> people;
    return people;
}