#include "World.h"

#include "Types.h"
#include "Entity.h"
#include "ErrorHandling.h"

SetQueue::SetQueue(IStorage* Storage)
{
    EntityIDs = new VectorStorage<EntityID>();
    ComponentBuffer = Storage;
}

SetQueue::~SetQueue()
{
    delete EntityIDs;
    delete ComponentBuffer;
}

void SetQueue::Enqueue(EntityID Entity, const void* Data) const
{
    EntityIDs->AddRawData(&Entity);
    ComponentBuffer->AddRawData(Data);
}

void SetQueue::ForEach(std::function<void(EntityID&, void*)> Handler)
{
    for (size_t i = 0; i < EntityIDs->GetSize(); i++)
    {
        EntityID* Id = static_cast<EntityID*>(EntityIDs->GetRawData(i));
        void* Data = ComponentBuffer->GetRawData(i);
        Handler(*Id, Data);
    }
}

World::World()
{
    Archetype* Empty = new Archetype();
    Graveyard = new VectorStorage<EntityID>();

    Archetypes.emplace_back(Empty);
    ArchSignature Sig = *Empty->GetSignature();
    ArchetypeLookup.emplace(Sig, 0);
}

World::~World()
{
    for (auto Archetype : Archetypes)
    {
        delete Archetype;
    }
    delete Graveyard;
    for (auto Kvp : SetQueues)
    {
        delete Kvp.second;
    }
    for (auto Kvp : RemoveQueues)
    {
        delete Kvp.second;
    }
}

Entity World::NewEntity()
{
    EntityID E = NextEntityID++;
    EntityArchetypeLookup.emplace(E, 0);
    Archetypes[0]->CopyEntity(E, nullptr, 0, nullptr);
    return Entity(this, E);
}

void World::Set(EntityID Entity, ComponentID Type, const void* Data)
{
    if (WorldLock)
    {
        auto Queue = SetQueues[Type];
        if (Queue == nullptr)
        {
            Queue = SetQueues[Type] = new SetQueue(MakeStorageForID(Type));
        }
        Queue->Enqueue(Entity, &Data);
        return;
    }
        
    Archetype* CurrentArchetype = Archetypes[EntityArchetypeLookup[Entity]];
    auto ContainsType = CurrentArchetype->GetSignature()->find(Type);

    //Not in current archetype. Move entity to new table.
    if (ContainsType == CurrentArchetype->GetSignature()->end())
    {
        CurrentArchetype = ChangeEntityType(Entity, Type, &Data);
    }

    CurrentArchetype->SetValue(Entity, Type, &Data);
}

void* World::Get(const EntityID& Entity, ComponentID Type)
{
    Archetype* CurrentArchetype = Archetypes[EntityArchetypeLookup[Entity]];
    auto ContainsType = CurrentArchetype->GetSignature()->find(Type);
        
    if(ContainsType == CurrentArchetype->GetSignature()->end())
    {
        return nullptr;
    }
    void* result = CurrentArchetype->GetValue(Entity, Type);
    return result;
}

void World::Remove(const EntityID& Entity, ComponentID Type)
{
    if (WorldLock)
    {
        auto Queue = RemoveQueues[Type];
        if (Queue == nullptr)
        {
            Queue = RemoveQueues[Type] = new VectorStorage<EntityID>();
        }
        Queue->AddRawData(&Entity);
        return;
    }
    ChangeEntityType(Entity, Type, nullptr);
}

void World::Delete(const EntityID& Entity)
{
    if (WorldLock)
    {
        Graveyard->AddRawData(&Entity);
        return;
    }
    Archetype* CurrentArchetype = Archetypes[EntityArchetypeLookup[Entity]];
    CurrentArchetype->FastDelete(Entity);
    EntityArchetypeLookup.erase(Entity);
}

Archetype* World::FindOrAddArchetype(const ArchSignature* Signature)
{
    if (WorldLock)
    {
        Error("Cannot make new archetypes while world is locked");
    }
    auto Found = ArchetypeLookup.find(*Signature);
    if (Found == ArchetypeLookup.end())
    {
        Archetype* NewArch = new Archetype(*Signature);
        ArchetypeLookup.emplace(*Signature, Archetypes.size());
        Archetypes.emplace_back(NewArch);
        for (auto& System : Systems)
        {
            System.TryAddMatch(NewArch);
        }
        return NewArch;
    }
    return Archetypes[Found->second];
}

Archetype* World::ChangeEntityType(const EntityID& Entity, ComponentID Type, const void* Data)
{
    
    Archetype* CurrentArchetype = Archetypes[EntityArchetypeLookup[Entity]];

    ArchSignature NewSig = *CurrentArchetype->GetSignature();
    if(Data == nullptr)
    {
        NewSig.erase(Type);
    }else
    {
        NewSig.emplace(Type);
    }
    Archetype* NewArchetype = FindOrAddArchetype(&NewSig);
    NewArchetype->CopyEntity(Entity, CurrentArchetype, Type, Data);
    CurrentArchetype->FastDelete(Entity);
    EntityArchetypeLookup[Entity] = ArchetypeLookup[*NewArchetype->GetSignature()];
    
    return NewArchetype;
}

void World::Tick()
{
    for (System& System : Systems)
    {
        WorldLock = true;
        for (auto Archetype : *System.GetMatchedArchetypes())
        {
            auto Entities = Archetype->GetEntityIDs();
            for (int i = Entities->GetSize() - 1; i >= 0; i--)
            {
                EntityID* Id = static_cast<EntityID*>(Entities->GetRawData(i));
                Entity E(this, *Id);
                System.GetHandler()(this, E);
            }
        }
        WorldLock = false;

        for (auto Kvp : SetQueues)
        {
            Kvp.second->ForEach([&](EntityID& Entity, const void* Data)
            {
                this->Set(Entity, Kvp.first, Data);
            });
            Kvp.second->Empty();
        }

        for (auto Kvp : RemoveQueues)
        {
            for (size_t i = 0; i < Kvp.second->GetSize(); i++)
            {
                EntityID* E = static_cast<EntityID*>(Kvp.second->GetRawData(i));
                Remove(*E, Kvp.first);
            }
            Kvp.second->Empty();
        }

        for(int i = Graveyard->GetSize() - 1; i >= 0; i--)
        {
            EntityID* E = static_cast<EntityID*>(Graveyard->GetRawData(i));
            Delete(*E);
        }
        Graveyard->Empty();
    }
}

void World::AddSystem(System System)
{
    for (auto Archetype : Archetypes)
    {
        System.TryAddMatch(Archetype);
    }
    Systems.emplace_back(System);
}
