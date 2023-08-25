#pragma once

#include <iostream>
#include <string>

class Foo
{
public:
    int age = 0;
    int test0(int a, int b)
    {
        age = a + b;
        return age;
    }

    void test1(int a, int b)
    {
        test0(a, b);
    }
};

class People
{
public:
    std::string name;
    int age;
    std::string BinZhou;

    People() = default;

    friend std::istream &operator>>(std::istream &is, People &people)
    {
        // 自定义的对象类型必须正确读入字符串对象（主要是空格问题)
        // 这里规定所有的字符串对象都不包含 ' 字符
        std::string temp;
        auto parseTo = [&temp](std::string &target) -> int
        {
            if(temp.find('\'') != std::string::npos)
            {
                size_t start = temp.find('\'');
                size_t end = temp.find('\'', start + 1);
                if(end == std::string::npos)
                    return -1;
                target = std::move(temp.substr(start + 1, end - start - 1));
            }
            else target = std::move(temp);
            return 0;
        };

        std::getline(is, temp);
        if(parseTo(people.name) == -1)
            throw std::runtime_error("姓名不合法！输入的姓名为：" + temp);
        
        is >> people.age;
        if(!is)
            throw std::runtime_error("输入年龄不合法！");
        is.seekg(1, std::ios::cur);
        std::getline(is, temp);
        if(parseTo(people.BinZhou) == -1)
            throw std::runtime_error("宾周不合法！输入的宾州为：" + temp);

        return is;
    }

    // 重载的 << 运算符的输出规则必须与 >> 运算符的输入规则一致，否则序列化错误
    friend std::ostream &operator<<(std::ostream &os, const People &people)
    {
        os << people.name << "\n" << people.age << "\n" << people.BinZhou;
        return os;
    }
};