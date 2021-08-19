#pragma once
#include <unordered_map>

#include "Types.h"

struct Component
{

    ComponentID ID;
    int Size;

    friend bool operator<(const Component& lhs, const Component& rhs)
    {
        return lhs.ID < rhs.ID;
    }

    friend bool operator<=(const Component& lhs, const Component& rhs)
    {
        return !(rhs < lhs);
    }

    friend bool operator>(const Component& lhs, const Component& rhs)
    {
        return rhs < lhs;
    }

    friend bool operator>=(const Component& lhs, const Component& rhs)
    {
        return !(lhs < rhs);
    }

    friend bool operator==(const Component& lhs, const Component& rhs)
    {
        return lhs.ID == rhs.ID
            && lhs.Size == rhs.Size;
    }

    friend bool operator!=(const Component& lhs, const Component& rhs)
    {
        return !(lhs == rhs);
    }
};

ComponentID GetNextID();
std::unordered_map<ComponentID, Component>* GetRegisteredComponents();

template<typename  T>
Component GetComponent()
{
    static Component Cmp = {GetNextID(), sizeof(T)};
    static auto Registered = GetRegisteredComponents()->emplace(Cmp.ID, Cmp);
    return Cmp;
}

Component GetComponentByID(ComponentID ID);
