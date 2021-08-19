#include "World.h"

#include "Entity.h"
#include "ErrorHandling.h"

SetQueue::SetQueue(const Component CmpType)
{
    EntityIDs = new ComponentStorage(GetComponent<EntityID>());
    ComponentBuffer = new ComponentStorage(CmpType);
}

SetQueue::~SetQueue()
{
    delete EntityIDs;
    delete ComponentBuffer;
}

void SetQueue::Enqueue(EntityID Entity, const void* Data) const
{
    auto index = EntityIDs->GetCount();
    EntityIDs->AddValue();
    EntityIDs->SetValue(index, &Entity);
    ComponentBuffer->AddValue();
    ComponentBuffer->SetValue(index, Data);
}

void SetQueue::ForEach(std::function<void(EntityID&, char*)> Handler)
{
    for (int i = 0; i < EntityIDs->GetCount(); i++)
    {
        EntityID* Id = reinterpret_cast<EntityID*>(EntityIDs->GetValue(i));
        char* Data = ComponentBuffer->GetValue(i);
        Handler(*Id, Data);
    }
}

World::World()
{
    Archetype* Empty = new Archetype(MakeSig({}));
    Graveyard = new ComponentStorage(GetComponent<EntityID>());

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
    Archetypes[0]->AddEntity(E);
    return Entity(this, E);
}

void World::Set(const EntityID& Entity, Component Type, const void* Data)
{
    if (WorldLock)
    {
        auto Queue = SetQueues[Type.ID];
        if (Queue == nullptr)
        {
            Queue = SetQueues[Type.ID] = new SetQueue(Type);
        }
        Queue->Enqueue(Entity, &Data);
        return;
    }
    Archetype* CurrentArchetype = Archetypes[EntityArchetypeLookup[Entity]];
    auto ContainsType = CurrentArchetype->GetSignature()->Value.find(Type.ID);

    //Not in current archetype. Move entity to new table.
    if (ContainsType == CurrentArchetype->GetSignature()->Value.end())
    {
        ArchSignature NewSig(CurrentArchetype->GetSignature()->Value);
        NewSig.Value.emplace(Type.ID);
        Archetype* NewArchetype = FindOrAddArchetype(NewSig);
        MoveEntity(Entity, CurrentArchetype, NewArchetype);
        CurrentArchetype = NewArchetype;
    }

    CurrentArchetype->SetValue(Entity, Type.ID, Data);
}

void World::Remove(const EntityID& Entity, Component Type)
{
    if (WorldLock)
    {
        auto Queue = RemoveQueues[Type.ID];
        if (Queue == nullptr)
        {
            Queue = RemoveQueues[Type.ID] = new ComponentStorage(GetComponent<EntityID>());
        }
        auto index = Queue->GetCount();
        Queue->AddValue();
        Queue->SetValue(index, &Entity);
        return;
    }
    Archetype* CurrentArchetype = Archetypes[EntityArchetypeLookup[Entity]];

    ArchSignature NewSig(CurrentArchetype->GetSignature()->Value);
    NewSig.Value.erase(Type.ID);
    Archetype* NewArchetype = FindOrAddArchetype(NewSig);
    MoveEntity(Entity, CurrentArchetype, NewArchetype);
}

void World::Delete(const EntityID& Entity)
{
    if (WorldLock)
    {
        Graveyard->AddValue();
        Graveyard->SetValue(Graveyard->GetCount() - 1, &Entity);
        return;
    }
    Archetype* CurrentArchetype = Archetypes[EntityArchetypeLookup[Entity]];
    CurrentArchetype->FastDelete(Entity);
    EntityArchetypeLookup.erase(Entity);
}

Archetype* World::FindOrAddArchetype(const ArchSignature& Signature)
{
    auto SigExists = ArchetypeLookup.find(Signature);
    size_t ArchIndex;
    if (SigExists == ArchetypeLookup.end())
    {
        if (WorldLock)
        {
            Error("Cannot make new archetypes while world is locked");
        }
        ArchIndex = Archetypes.size();
        Archetype* NewArch = new Archetype(Signature);
        Archetypes.emplace_back(NewArch);
        ArchetypeLookup.emplace(Signature, ArchIndex);
        for (auto& System : Systems)
        {
            System.TryAddMatch(NewArch);
        }
    }
    else
    {
        ArchIndex = SigExists->second;
    }
    return Archetypes[ArchIndex];
}

void World::MoveEntity(const EntityID& Entity, Archetype* Src, Archetype* Dest)
{
    if (WorldLock)
    {
        Error("Move requested for %d while world is locked", Entity);
    }

    if (Src == Dest) return;

    Dest->AddEntity(Entity);
    for (auto CmpID : Src->GetSignature()->Value)
    {
        if (Dest->GetSignature()->Value.find(CmpID) == Dest->GetSignature()->Value.end()) continue;

        const char* CmpData = Src->GetValue(Entity, CmpID);
        Dest->SetValue(Entity, CmpID, CmpData);
    }
    Src->FastDelete(Entity);
    EntityArchetypeLookup[Entity] = ArchetypeLookup[*Dest->GetSignature()];
}

void World::Tick()
{
    for (System& System : Systems)
    {
        WorldLock = true;
        for (auto Archetype : *System.GetMatchedArchetypes())
        {
            auto Entities = Archetype->GetEntityIDs();
            for (int i = Entities->GetCount() - 1; i >= 0; i--)
            {
                EntityID* Id = reinterpret_cast<EntityID*>(Entities->GetValue(i));
                Entity E(this, *Id);
                System.GetHandler()(this, E);
            }
        }
        WorldLock = false;

        for (auto Kvp : SetQueues)
        {
            Component Cmp = GetComponentByID(Kvp.first);
            Kvp.second->ForEach([&](EntityID& Entity, char* Data)
            {
                this->Set(Entity, Cmp, Data);
            });
            Kvp.second->Empty();
        }

        for (auto Kvp : RemoveQueues)
        {
            Component Cmp = GetComponentByID(Kvp.first);
            for (int i = 0; i < Kvp.second->GetCount(); i++)
            {
                EntityID* E = reinterpret_cast<EntityID*>(Kvp.second->GetValue(i));
                Remove(*E, Cmp);
            }
            Kvp.second->Empty();
        }
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
