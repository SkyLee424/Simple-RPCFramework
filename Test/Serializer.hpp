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
        os << typeid(object).name() << " ";
        os << object;
        return os.str();
    }

    template <typename T>
    static T Deserialize(const std::string& serializedData) {
        std::istringstream is(serializedData);
        std::string typeName;
        is >> typeName;
        // if (typeName != typeid(T).name()) {
        //     std::istringstream error;
        //     error << "Type mismatch during deserialization." << std::endl;
        //     error << "Original Type: " << typeName << std::endl;
        //     error << "Target Type: " << typeid(T).name() << std::endl;
        //     throw std::runtime_error(error.str());
        // }
        T object;
        is >> object;
        return object;
    }
};