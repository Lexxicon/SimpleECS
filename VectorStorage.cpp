#include "VectorStorage.h"

std::unordered_map<ComponentID, std::function<IStorage*()>>* GetStoreFactory()
{
    static std::unordered_map<ComponentID, std::function<IStorage*()>> Factory;
    return &Factory;
}

ComponentID GetNextID()
{
    static ComponentID NextComponentID = 0;
    return NextComponentID++;
}

IStorage* MakeStorageForID(ComponentID ID)
{
    return GetStoreFactory()->find(ID)->second();
}