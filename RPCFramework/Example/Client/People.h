#pragma once

#include <iostream>
#include <string>
#include "Serializable.hpp"

class People : public Serializable
{
public:
    std::string name;
    int age;
    std::string BinZhou;

    People() = default;

    static std::istream &DeSerialize(std::istream &is, People &people)
    {
        Serializable::DeSerialize(is, people.name);
        Serializable::DeSerialize(is, people.age);
        Serializable::DeSerialize(is, people.BinZhou);
        return is;
    }

    static std::ostream &Serialize(std::ostream &os, const People &people)
    {
        Serializable::Serialize(os, people.name);
        Serializable::Serialize(os, people.age);
        Serializable::Serialize(os, people.BinZhou);
        return os;
    }

    bool operator<(const People &p) const
    {
        return this->age < p.age;
    }
};