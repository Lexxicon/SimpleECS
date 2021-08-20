#pragma once
#include <functional>

#include "Types.h"
#include "Archetype.h"
#include "Component.h"
#include "ErrorHandling.h"
#include "System.h"

class Entity;

class SetQueue
{
public:
    SetQueue(IStorage* Storage);

    ~SetQueue();
    SetQueue(const SetQueue& obj) = delete;

    void Enqueue(EntityID Entity, const void* Data) const;

    void ForEach(std::function<void(EntityID&, void*)> Handler);

    void Empty()
    {
        EntityIDs->Empty();
        ComponentBuffer->Empty();
    }

private:
    VectorStorage<EntityID>* EntityIDs;
    IStorage* ComponentBuffer;
};

class World
{
    
public:
    World();

    ~World();
    World(const World& obj) = delete;

    Entity NewEntity();

    template<typename T>
    void Set(const EntityID& Entity, const T Data)
    {
        ComponentID Type = GetComponent<T>();
        Set(Entity, Type, &Data);
    }
    void Set(EntityID Entity, ComponentID Type, const void* Data);

    template<typename T>
    T* Get(const EntityID& Entity)
    {
        ComponentID Type = GetComponent<T>();
        void* R = Get(Entity, Type);
        return static_cast<T*>(R);
    }
    void* Get(const EntityID& Entity, ComponentID Type);
    
    template<typename T>
    void Remove(const EntityID& Entity)
    {
        Remove(Entity, GetComponent<T>());
    }
    void Remove(const EntityID& Entity, ComponentID Type);
    
    void Delete(const EntityID& Entity);

private:
    Archetype* FindOrAddArchetype(const ArchSignature* Signature);
    Archetype* ChangeEntityType(const EntityID& Entity, ComponentID Type, const void* Data);

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
    std::unordered_map<ComponentID, IStorage*> RemoveQueues;
    VectorStorage<EntityID>* Graveyard;
    
    std::vector<System> Systems;
};