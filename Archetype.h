#pragma once
#include <unordered_map>
#include <unordered_set>

#include "Component.h"
#include "ComponentStorage.h"
#include "Types.h"

struct ArchSignature
{
    ArchSignature(const ArchSignature& other);

    ArchSignature(ArchSignature&& other) noexcept;

    ArchSignature& operator=(const ArchSignature& other);

    ArchSignature& operator=(ArchSignature&& other) noexcept;

    explicit ArchSignature(const std::unordered_set<ComponentID>& value);

    std::unordered_set<ComponentID> Value;

    friend bool operator==(const ArchSignature& lhs, const ArchSignature& rhs)
    {
        return lhs.Value == rhs.Value;
    }

    friend bool operator!=(const ArchSignature& lhs, const ArchSignature& rhs)
    {
        return !(lhs == rhs);
    }
};

// custom specialization of std::hash can be injected in namespace std
namespace std
{
    template<> struct hash<ArchSignature>
    {
        std::size_t operator()(ArchSignature const& s) const noexcept
        {
            std::size_t hash = 0;
            for (auto && i : s.Value) hash ^= std::hash<ComponentID>()(i);
            return hash;
        }
    };
}

ArchSignature MakeSig(std::vector<ComponentID> Values);

class Archetype
{
public:
    explicit Archetype(const ArchSignature signature);

    ~Archetype();

    void AddEntity(const EntityID& Entity);

    void FastDelete(const EntityID& ID);

    template<typename T>
    void SetValue(const EntityID& ID, const T& Data);

    void SetValue(const EntityID& ID, const ComponentID& CmpID, const void* Data);

    template<typename T>
    T* GetValue(const EntityID& ID);

    char* GetValue(const EntityID& ID, const ComponentID& CmpID);

    const ArchSignature* GetSignature() const;

    const ComponentStorage* GetEntityIDs() const;
private:
    const ArchSignature Signature;
    std::unordered_map<EntityID, int> EntityToIndex;
    std::unordered_map<ComponentID, int> CmpToStoreIndex;
    ComponentStorage* EntityIDStorage;
    std::vector<ComponentStorage*> CmpStorage;
};

template <typename T>
void Archetype::SetValue(const EntityID& ID, const T& Data)
{
    auto CmpType = GetComponent<T>();
    SetValue(ID, CmpType.ID, &Data);
}

template <typename T>
T* Archetype::GetValue(const EntityID& ID)
{
    auto CmpType = GetComponent<T>();
    return reinterpret_cast<T*>(GetValue(ID, CmpType.ID));
}
