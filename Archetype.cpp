#include "Archetype.h"

#include "ErrorHandling.h"
#include "Types.h"

ArchSignature::ArchSignature(const ArchSignature& other): Value(other.Value)
{
}

ArchSignature::ArchSignature(ArchSignature&& other) noexcept: Value(std::move(other.Value))
{
}

ArchSignature& ArchSignature::operator=(const ArchSignature& other)
{
    if (this == &other)
        return *this;
    Value = other.Value;
    return *this;
}

ArchSignature& ArchSignature::operator=(ArchSignature&& other) noexcept
{
    if (this == &other)
        return *this;
    Value = std::move(other.Value);
    return *this;
}

ArchSignature::ArchSignature(const std::unordered_set<ComponentID>& value): Value(value)
{
}

ArchSignature MakeSig(std::vector<ComponentID> Values)
{
    std::unordered_set<ComponentID> IDs;
    for (auto C : Values)
    {
        IDs.emplace(C);
    }
    return ArchSignature(IDs);
}

Archetype::Archetype(const ArchSignature signature): Signature(signature)
{
    EntityIDStorage = new ComponentStorage(GetComponent<EntityID>());
    int Index = 0;
    for (auto CompID : Signature.Value)
    {
        CmpToStoreIndex.emplace(CompID, Index++);
        CmpStorage.emplace_back(new ComponentStorage(GetComponentByID(CompID)));
    }
}

Archetype::~Archetype()
{
    delete EntityIDStorage;
    for (auto Store : CmpStorage)
    {
        delete Store;
    }
}

void Archetype::AddEntity(const EntityID& Entity)
{
    const auto Found = EntityToIndex.find(Entity);
    if (Found != EntityToIndex.end())
    {
        Error("Attempt to add already present entity %d\n", Entity);
    }
    EntityToIndex.emplace(Entity, EntityIDStorage->GetCount());
    EntityIDStorage->AddValue();
    EntityIDStorage->SetValue(EntityIDStorage->GetCount() - 1, &Entity);
    for (auto Store : CmpStorage)
    {
        Store->AddValue();
    }
}

void Archetype::FastDelete(const EntityID& ID)
{
    auto Found = EntityToIndex.find(ID);
    if (Found == EntityToIndex.end())
    {
        Error("Failed to find entity to delete %d\n", ID);
    }
    const int Index = Found->second;

    EntityToIndex.erase(ID);
    EntityIDStorage->FastDelete(Index);
    for (auto Store : CmpStorage)
    {
        Store->FastDelete(Index);
    }
    //we swapped the entity ID, so we need to update the index
    if (Index != EntityIDStorage->GetCount())
    {
        EntityID MovedID = *reinterpret_cast<EntityID*>(EntityIDStorage->GetValue(Index));
        EntityToIndex[MovedID] = Index;
    }
}

void Archetype::SetValue(const EntityID& ID, const ComponentID& CmpID, const void* Data)
{
    auto FoundEntity = EntityToIndex.find(ID);
    if (FoundEntity == EntityToIndex.end())
    {
        Error("Failed to find entity to set %d\n", ID);
    }
    auto FoundCmp = CmpToStoreIndex.find(CmpID);
    if (FoundCmp == CmpToStoreIndex.end())
    {
        Error("Failed to find Component to set %d\n", CmpID);
    }
    CmpStorage[FoundCmp->second]->SetValue(FoundEntity->second, Data);
}

char* Archetype::GetValue(const EntityID& ID, const ComponentID& CmpID)
{
    auto FoundEntity = EntityToIndex.find(ID);
    if (FoundEntity == EntityToIndex.end())
    {
        Error("Failed to find entity to set %d\n", ID);
    }
    auto FoundCmp = CmpToStoreIndex.find(CmpID);
    if (FoundCmp == CmpToStoreIndex.end())
    {
        Error("Failed to find Component to set %d\n", CmpID);
    }
    return CmpStorage[FoundCmp->second]->GetValue(FoundEntity->second);
}

const ArchSignature* Archetype::GetSignature() const
{
    return &Signature;
}

const ComponentStorage* Archetype::GetEntityIDs() const
{
    return EntityIDStorage;
}
