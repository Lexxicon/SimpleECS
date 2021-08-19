#pragma once
#include "World.h"

class Entity
{
public:
    Entity(World* world, EntityID id)
        : Wrld(world),
          ID(id)
    {
    }

    EntityID GetID();

    template<typename T>
    T* Get()
    {
        return Wrld->Get<T>(ID);
    }

    template<typename T>
    Entity& Set(T Data)
    {
        Wrld->Set<T>(ID, Data);
        return *this;
    }
    
    template<typename T>
    void Remove()
    {
        Wrld->Remove<T>(ID);
    }

private:
    World* Wrld;
    EntityID ID;
};
