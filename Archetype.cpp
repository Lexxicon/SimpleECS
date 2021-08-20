
#include "Types.h"
#include "Archetype.h"
#include "ErrorHandling.h"
int NextArchID()
{
    static int NextID = 1;
    return NextID++;
}
Archetype::Archetype(): ID(NextArchID()), Signature({})
{
    EntityIDStorage = new VectorStorage<EntityID>();
}

Archetype::Archetype(const ArchSignature& Base): ID(NextArchID()) 
{
    EntityIDStorage = new VectorStorage<EntityID>();
    for(const auto Type : Base)
    {
        Signature.emplace(Type);
        CmpToStoreIndex.emplace(Type, CmpStorage.size());
        CmpStorage.push_back(MakeStorageForID(Type));
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

void Archetype::CopyEntity(const EntityID& Entity, const Archetype* Source, ComponentID AddedType, const void* AddedValue)
{
    Trace("Copy Entity %d from %d to %d\n", Entity, Source == nullptr? -1 : Source->ID, this->ID);
    if(this == Source)
    {
        Error("Tried to copy entity into same archetype\n");
    }
    
    auto FoundEntity = EntityToIndex.find(Entity);
    if (FoundEntity != EntityToIndex.end())
    {
        Error("Attempt to add already present entity %d\n", Entity);
    }
    auto FoundNewType = CmpToStoreIndex.find(AddedType);
    if(AddedType != 0 && FoundNewType == CmpToStoreIndex.end())
    {
        Error("Added type %d not present in destination\n", AddedType);
    }
    
    EntityToIndex.emplace(Entity, EntityIDStorage->GetSize());
    EntityIDStorage->AddRawData(&Entity);

    if(AddedType > 0)
    {
        CmpStorage[FoundNewType->second]->AddRawData(AddedValue);
    }
    
    for (auto Store : CmpStorage)
    {
        if(Store->GetTypeID() == AddedType) continue;
        
        Store->AddRawData(Source->GetValue(Entity, Store->GetTypeID()));
    }
}

void Archetype::FastDelete(const EntityID& ID)
{
    Trace("Delete Entity %d from %d\n", ID, this->ID);
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
    if (Index != EntityIDStorage->GetSize())
    {
        EntityID MovedID = *static_cast<EntityID*>(EntityIDStorage->GetRawData(Index));
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
    CmpStorage[FoundCmp->second]->SetRawData(FoundEntity->second, Data);
}

void* Archetype::GetValue(const EntityID& ID, const ComponentID& CmpID) const 
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
    return CmpStorage[FoundCmp->second]->GetRawData(FoundEntity->second);
}

const ArchSignature* Archetype::GetSignature() const
{
    return &Signature;
}

VectorStorage<EntityID>* Archetype::GetEntityIDs() const
{
    return EntityIDStorage;
}
