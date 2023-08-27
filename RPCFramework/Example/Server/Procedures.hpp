#pragma once

#include <sstream>
#include <chrono>
#include <thread>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <numeric>
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

int getSum(std::vector<int> nums) // 测试对 vector 容器的支持
{
    return std::accumulate(nums.begin(), nums.end(), 0);
}

std::vector<int> twoSum(std::vector<int> nums, int target)  // 测试对 unordered_map 的支持
{
    std::unordered_map<int, int> mapping;
    int n = nums.size();
    for(int i = 0; i < n; ++i)
    {
        if(mapping.count(target - nums[i]))
            return {mapping[target - nums[i]], i};
        mapping[nums[i]] = i;
    }
    return {};
}

People getHeXin(People people) 
{
    people.name = "He Xin";
    people.age = 19;
    people.BinZhou = "0.618 cm";
    return people;
}

std::vector<People> getManyHeXin(int num)
{
    People people;
    people.name = "He Xin";
    people.age = 19;
    people.BinZhou = "0.618 cm";
    std::vector<People> res;
    while (num--)
        res.push_back(people);
    return res;    
}