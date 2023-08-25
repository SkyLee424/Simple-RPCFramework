#pragma once
#include "Serializer.hpp"


// 存储返回值类型的结构体（主要是用于实现支持返回值为 void 的函数）
template <typename R>
struct RetType
{
    typedef R type;
};

// 特例化 void 模版
template <>
struct RetType<void>
{
    typedef int type;
};

template <typename R>
class ReturnPacket
{
public:
    using code_t = short;
    using ret_t = typename RetType<R>::type;

    static constexpr code_t SUCCESS = 0;
    static constexpr code_t UNKNOWN = 1;    
    static constexpr code_t NO_SUCH_PROCEDURE = 2;

    template <typename X>
    friend std::ostream& operator<<(std::ostream &os, const ReturnPacket<X> &retPack);

    template <typename X>
    friend std::istream& operator>>(std::istream &is, ReturnPacket<X> &retPack);

    ReturnPacket()
        :code(UNKNOWN) {}

    ReturnPacket(code_t code)
        :code(code) {}

    ReturnPacket(code_t code, const ret_t &ret)
        :code(code), ret(ret) {}

    // 通用拷贝构造函数，主要是为了重载拷贝构造函数
    template <typename U = R, 
              typename = typename std::enable_if<!std::is_void<U>::value>::type>
    ReturnPacket(const ReturnPacket &p)
        :code(p.code), ret(p.ret) {}
    
    ReturnPacket(const ReturnPacket<void> &vp)
        :code(vp.getCode()) {}

    code_t getCode() const
    {
        return code;
    }

    bool vaild() const
    {
        return code == SUCCESS;
    }

    ret_t getRet()
    {
        return ret;
    }

private:
    code_t code;
    ret_t ret;
};

// 还要实现 << 和 >> 
template <typename R>
std::ostream& operator<<(std::ostream &os, const ReturnPacket<R> &retPack)
{
    std::string temp = Serializer::Serialize(retPack.ret);
    os << retPack.code << " " << temp.size() << " " << temp;
    return os;
}

template <typename R>
std::istream& operator>>(std::istream &is, ReturnPacket<R> &retPack)
{
    size_t len;
    std::string temp;
    is >> retPack.code >> len;
    is.seekg(1, std::ios::cur);

    char *buffer = new char[len + 1];
    is.read(buffer, len);
    buffer[len] = '\0';
    temp = buffer;

    retPack.ret = Serializer::Deserialize<typename RetType<R>::type>(temp);
    return is;
}