#pragma once
#include <stdexcept>

int add(int a, int b)
{
    return a + b;
}

int subtract(int a, int b)
{
    return a - b;
}

int multiply(int a, int b)
{
    return a * b;
}

int divide(int a, int b)
{
    if(b == 0)
    {
        throw std::runtime_error("除数不能为 0");
    }
    return a / b;
}