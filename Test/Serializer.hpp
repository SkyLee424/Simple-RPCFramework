#pragma once
#include <iostream>
#include <sstream>
#include <string>
#include <typeinfo>

/**
 * @brief 简单的序列化与反序列化工具，可以实现任意对象的序列化和反序列化过程，但该对象必须重载 << 和 >> 运算符
 * 
 */
class Serializer {
public:
    template <typename T>
    static std::string Serialize(const T& object) {
        std::ostringstream os;
        os << object;
        return os.str();
    }

    template <typename T>
    static typename
    std::enable_if<!std::is_same<T, std::string>::value, T>::type
    Deserialize(const std::string& serializedData) {
        std::istringstream is(serializedData);
        T object;
        is >> object;
        return object;
    }

    //  对于反序列化对象为 string 的，特殊处理
    template <typename T>
    static typename
    std::enable_if<std::is_same<T, std::string>::value, std::string>::type
    Deserialize(const std::string& serializedData) {
        std::istringstream is(serializedData);
        T object;
        std::string temp;
        while (std::getline(is, temp))
        {
            object += temp;
            if(!is.eof())
                object += '\n';
        }
        
        return object;
    }
};