#pragma once
#include <functional>
#include <unordered_map>
#include <vector>

#include "Types.h"

class IStorage;
template<typename  T>
class VectorStorage;


std::unordered_map<ComponentID, std::function<IStorage*()>>* GetStoreFactory();
ComponentID GetNextID();

template<typename  T>
ComponentID GetComponent()
{
    static ComponentID Cmp = GetNextID();
    static auto Stored = GetStoreFactory()->emplace(Cmp, [](){return new VectorStorage<T>();});
    return Cmp;
}

IStorage* MakeStorageForID(ComponentID ID);

class IStorage
{
public:
    virtual ~IStorage() = default;
    
    virtual ComponentID GetTypeID() = 0;
    
    virtual size_t GetSize() const = 0;
    virtual void* GetRawData(int Index) = 0;
    virtual void SetRawData(int Index, const void* Data) = 0;
    virtual void RemoveRawData(int Index) = 0;
    virtual void AddRawData(const void* Data) = 0;
    virtual void Empty() = 0;
    virtual void FastDelete(size_t Index) = 0;
};

template<typename T>
class VectorStorage : public IStorage
{
public:
    VectorStorage()
        :TypeID(GetComponent<T>())
    {
    }
    ~VectorStorage() override = default;
    
    ComponentID GetTypeID() override { return TypeID; }
    
    size_t GetSize() const override { return Store.size(); }
    void* GetRawData(int Index) override { return &Store[Index]; }
    void SetRawData(int Index, const void* Data) override { Store[Index] = *static_cast<const T*>(Data); }
    void RemoveRawData(int Index) override { Store.erase(Store.begin() + Index); }
    void AddRawData(const void* Data) override { Store.push_back(*static_cast<const T*>(Data)); }
    void Empty() override { Store.clear(); }
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