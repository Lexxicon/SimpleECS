#pragma once
#include <unordered_map>
#include <unordered_set>

#include "Types.h"
#include "VectorStorage.h"

// custom specialization of std::hash can be injected in namespace std
namespace std
{
    template<> struct hash<ArchSignature>
    {
        std::size_t operator()(ArchSignature const& s) const noexcept
        {
            std::size_t hash = 0;
            for (auto && i : s) hash ^= std::hash<ComponentID>()(i);
            return hash;
        }
    };
}

class Archetype
{
public:
    explicit Archetype();
    explicit Archetype(const ArchSignature& Base);
    Archetype(const Archetype& obj) = delete;

    ~Archetype();

    void CopyEntity(const EntityID& Entity, const Archetype* Source, ComponentID AddedType, const void* AddedValue);

    void FastDelete(const EntityID& ID);

    template<typename T>
    void SetValue(const EntityID& ID, const T& Data);
    void SetValue(const EntityID& ID, const ComponentID& CmpID, const void* Data);

    template<typename T>
    T* GetValue(const EntityID& ID);
    void* GetValue(const EntityID& ID, const ComponentID& CmpID) const;

    const ArchSignature* GetSignature() const;

    VectorStorage<EntityID>* GetEntityIDs() const;
private:
    int ID;
    ArchSignature Signature;
    
    std::unordered_map<EntityID, int> EntityToIndex;
    std::unordered_map<ComponentID, int> CmpToStoreIndex;
    
    VectorStorage<EntityID>* EntityIDStorage;
    std::vector<IStorage*> CmpStorage;
};

template <typename T>
void Archetype::SetValue(const EntityID& ID, const T& Data)
{
    auto CmpType = GetComponent<T>();
    SetValue(ID, CmpType, &Data);
}

template <typename T>
T* Archetype::GetValue(const EntityID& ID)
{
    auto CmpType = GetComponent<T>();
    return static_cast<T*>(GetValue(ID, CmpType));
}
