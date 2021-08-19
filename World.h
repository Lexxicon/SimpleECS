#pragma once
#include <functional>

#include "Archetype.h"
#include "Component.h"
#include "ComponentStorage.h"
#include "System.h"

class Entity;

class SetQueue
{
public:
    SetQueue(const Component CmpType);

    ~SetQueue();
    SetQueue(const SetQueue& obj) = delete;

    void Enqueue(EntityID Entity, const void* Data) const;

    void ForEach(std::function<void(EntityID&, char*)> Handler);

    void Empty()
    {
        EntityIDs->Empty();
        ComponentBuffer->Empty();
    }

private:
    ComponentStorage* EntityIDs;
    ComponentStorage* ComponentBuffer;
};

class World
{
    
public:
    World();

    ~World();
    World(const World& obj) = delete;

    Entity NewEntity();

    template<typename T>
    void Set(const EntityID& Entity, const T& Data)
    {
        Set(Entity, GetComponent<T>(), &Data);
    }

    void Set(const EntityID& Entity, Component Type, const void* Data);

    template<typename T>
    T* Get(const EntityID& Entity)
    {
        Archetype* CurrentArchetype = Archetypes[EntityArchetypeLookup[Entity]];
        Component Type = GetComponent<T>();
        auto ContainsType = CurrentArchetype->GetSignature()->Value.find(Type.ID);
        
        //Not in current archetype. Move entity to new table.
        if(ContainsType == CurrentArchetype->GetSignature()->Value.end())
        {
            return nullptr;
        }
        return CurrentArchetype->GetValue<T>(Entity);
    }

    

    template<typename T>
    void Remove(const EntityID& Entity)
    {
        Remove(Entity, GetComponent<T>());
    }

    void Remove(const EntityID& Entity, Component Type);
    void Delete(const EntityID& Entity);

private:
    Archetype* FindOrAddArchetype(const ArchSignature& Signature);
    void MoveEntity(const EntityID& Entity, Archetype* Src, Archetype* Dest);
public:
    void Tick();
    void AddSystem(System System);

private:
    bool WorldLock = false;
    EntityID NextEntityID = 1;
    std::vector<Archetype*> Archetypes;
    std::unordered_map<ArchSignature, size_t> ArchetypeLookup;
    std::unordered_map<EntityID, size_t> EntityArchetypeLookup;

    std::unordered_map<ComponentID, SetQueue*> SetQueues;
    std::unordered_map<ComponentID, ComponentStorage*> RemoveQueues;
    ComponentStorage* Graveyard;
    
    std::vector<System> Systems;
};