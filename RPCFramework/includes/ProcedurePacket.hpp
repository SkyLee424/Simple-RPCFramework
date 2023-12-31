#pragma once
#include <iostream>
#include <string>
#include <memory>
#include <tuple>
#include <utility>
#include "Serializable.hpp"


// 递归终点
// 使用 std::enable_if 来判断是否将该模版函数加入到重载函数集中
// 当 Index == sizeof...(X)，即 tuple 的 end()，即为递归终点
template <size_t Index = 0, typename ...X>
typename std::enable_if<Index == sizeof...(X), void>::type
expand_tuple(std::ostream& os, const std::tuple<X...>& t)
{

}

// 递归终点
template <size_t Index = 0, typename ...X>
typename std::enable_if<Index == sizeof...(X), void>::type
expand_tuple(std::istream &is, std::tuple<X...>&t)
{

}

// 工具函数，用于序列化 tuple
// 使用 std::enable_if 来判断是否将该模版函数加入到重载函数集中
// 当 Index < sizeof...(X)，即没有到达 tuple 的 end()，可以执行该版本
template <size_t Index = 0, typename ...X>
typename std::enable_if<Index < sizeof...(X), void>::type
expand_tuple(std::ostream& os, const std::tuple<X...>& t)
{
    Serializable::Serialize(os, std::get<Index>(t));
    expand_tuple<Index + 1, X...>(os, t);
}

// 工具函数，用于反序列化 tuple
template <size_t Index = 0, typename ...X>
typename std::enable_if<Index < sizeof...(X), void>::type
expand_tuple(std::istream& is, std::tuple<X...>&t)
{
    Serializable::DeSerialize(is, std::get<Index>(t));
    expand_tuple<Index + 1, X...>(is, t);
}

template <typename ...Args>
struct ProcedurePacket : public Serializable
{
    std::string name; // 过程的名称
    std::tuple<Args ...> t; // tuple 用于保存传递的参数

    ProcedurePacket() = default; // 缺陷：Args 不能包含引用类型的参数

    ProcedurePacket(const std::string &name, Args... args)
        :name(name), t(std::make_tuple(args...)) {}

    template <typename ...X>
    static std::ostream& Serialize(std::ostream &os, const ProcedurePacket<X ...> &packet)
    {
        os << packet.name << " ";
        expand_tuple(os, packet.t);
        return os;
    }

    template <typename ...X>
    static std::istream& DeSerialize(std::istream &is, ProcedurePacket<X ...> &packet)
    {
        is >> packet.name;
        expand_tuple(is, packet.t);
        return is;
    }
};