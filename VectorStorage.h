#pragma once
#include <vector>

#include "Component.h"
#include "Types.h"

class IStorage
{
public:
    virtual ~IStorage() = default;
    
    virtual ComponentID GetTypeID() = 0;
    virtual size_t GetSize() = 0;
    virtual void* GetRawData(int Index) = 0;
    virtual void SetRawData(int Index, void* Data) = 0;
    virtual void RemoveRawData(int Index) = 0;
    virtual void AddRawData(void* Data) = 0;
    virtual void Empty() = 0;
    virtual void FastDelete(size_t Index) = 0;
};

template<typename T>
class VectorStorage : public IStorage
{
public:
    VectorStorage()
        :TypeID(GetComponent<T>().ID)
    {
    }
    ~VectorStorage() override = default;
    
    ComponentID GetTypeID() override { return TypeID; }
    size_t GetSize() override { return Store.size(); }
    void* GetRawData(int Index) override { return &Store[Index]; }
    void SetRawData(int Index, void* Data) override { Store[Index] = *static_cast<T*>(Data); }
    void RemoveRawData(int Index) override { Store.erase(Store.begin() + Index); }
    void AddRawData(void* Data) override { Store.push_back(*static_cast<T*>(Data)); }
    void Empty() override { Store.empty(); }
    void FastDelete(size_t Index) override
    {
        if(Index < Store.size() - 1)
        {
            Store[Index] = Store[Store.size() - 1];
        }
        Store.pop_back();
    }
private:
    std::vector<T> Store;
    const ComponentID TypeID;
};