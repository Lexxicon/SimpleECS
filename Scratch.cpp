#include <algorithm>
#include <functional>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <windows.h>

#include "flecs.h"

void Error(const char* Format, ...)
{
    va_list Arglist;
    va_start( Arglist, Format );
    vprintf( Format, Arglist );
    va_end( Arglist );
    abort();
}

typedef int EntityID;
typedef int ComponentID;

struct Component
{

    ComponentID ID;
    int Size;

    friend bool operator<(const Component& lhs, const Component& rhs)
    {
        return lhs.ID < rhs.ID;
    }

    friend bool operator<=(const Component& lhs, const Component& rhs)
    {
        return !(rhs < lhs);
    }

    friend bool operator>(const Component& lhs, const Component& rhs)
    {
        return rhs < lhs;
    }

    friend bool operator>=(const Component& lhs, const Component& rhs)
    {
        return !(lhs < rhs);
    }

    friend bool operator==(const Component& lhs, const Component& rhs)
    {
        return lhs.ID == rhs.ID
            && lhs.Size == rhs.Size;
    }

    friend bool operator!=(const Component& lhs, const Component& rhs)
    {
        return !(lhs == rhs);
    }
};

struct ArchSignature
{
    ArchSignature(const ArchSignature& other)
        : Value(other.Value)
    {
    }

    ArchSignature(ArchSignature&& other) noexcept
        : Value(std::move(other.Value))
    {
    }

    ArchSignature& operator=(const ArchSignature& other)
    {
        if (this == &other)
            return *this;
        Value = other.Value;
        return *this;
    }

    ArchSignature& operator=(ArchSignature&& other) noexcept
    {
        if (this == &other)
            return *this;
        Value = std::move(other.Value);
        return *this;
    }

    explicit ArchSignature(const std::unordered_set<ComponentID>& value)
        : Value(value)
    {
    }

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

ComponentID NextComponentID = 1;
std::unordered_map<ComponentID, Component> RegisteredComponents;

template<typename  T>
Component GetComponent()
{
    static Component Cmp = {NextComponentID++, sizeof(T)};
    static auto Registered = RegisteredComponents.emplace(Cmp.ID, Cmp);
    return Cmp;
}
Component GetComponentByID(ComponentID ID)
{
    const auto Found = RegisteredComponents.find(ID);
    if(Found == RegisteredComponents.end())
    {
        Error("Unrecognized Component for %d\n", ID);
    }
    return Found->second;
}

ArchSignature MakeSig(std::vector<ComponentID> Values)
{
    std::unordered_set<ComponentID> IDs;
    for(auto C : Values)
    {
        IDs.emplace(C);
    }
    return ArchSignature(IDs);
}

class ComponentStorage
{
#define IND_TO_D_PTR(Index) \
    Data + (Index * Type.Size)

public:
    ComponentStorage(const Component& type)
        : Type(type),
          MaxCount(10),
          Count(0)
    {
        Data = new char[MaxCount * Type.Size];
        SwapBuffer = new char[Type.Size];
    }
    ~ComponentStorage()
    {
        free(Data);
        free(SwapBuffer);
    }
    ComponentStorage(const ComponentStorage& obj) = delete;

private:
    void Grow()
    {
        const int Growth = min(static_cast<uint32_t>(MaxCount * 0.5), 1000);
        if(MaxCount > INT_MAX - Growth)
        {
            Error("Grow would overflow %d+%d\n", MaxCount, Growth);
        }
        MaxCount += Growth;
        char* NewData = new char[MaxCount * Type.Size];
        if(!NewData)
        {
            Error("Failed to malloc\n");
        }
        delete [] Data;
        memmove(NewData, Data, Count * Type.Size);
        Data = NewData;
    }

public:
    char* GetValue(const int Index) const
    {
        if(Index >= Count || Index < 0)
        {
            Error("Request Get OOB [%d] %d\n", Index, Count);
        }
        return IND_TO_D_PTR(Index);
    }

    void SetValue(const int Index, const void* Value) const
    {
        if(Index >= Count || Index < 0)
        {
            Error("Request Set OOB [%d] %d\n", Index, Count);
        }
        memcpy(IND_TO_D_PTR(Index), Value, Type.Size);
    }

    void AddValue()
    {
        if(Count == MaxCount)
        {
            Grow();
        }
        memset(IND_TO_D_PTR(Count), 0, Type.Size);
        Count++;
    }

    void Swap(int IndexOne, int IndexTwo) const
    {
        if(IndexOne >= Count || IndexTwo >= Count || IndexOne < 0 || IndexTwo < 0)
        {
            Error("Swap is OOB [%d]: %d %d\n", Count, IndexOne, IndexTwo);
        }
        memcpy(SwapBuffer, IND_TO_D_PTR(IndexOne), Type.Size);
        memcpy(IND_TO_D_PTR(IndexOne), IND_TO_D_PTR(IndexTwo), Type.Size);
        memcpy(IND_TO_D_PTR(IndexTwo), SwapBuffer, Type.Size);
    }

    void FastDelete(int Index)
    {
        if(Index >= Count || Index < 0)
        {
            Error("FastDelete is OOB [%d]: %d\n", Count, Index);
        }
        if(Index != Count - 1)
        {
            Swap(Index, Count - 1);
        }
        Count--;
    }

    void Empty()
    {
        Count = 0;
    }

    int GetCount() const
    {
        return Count;
    }
    const Component* GetType() const
    {
        return &Type;
    }
#undef IND_TO_D_PTR

private:
    const Component Type;
    int MaxCount;
    int Count;
    char* Data;
    char* SwapBuffer;
};

class Archetype
{
public:
    explicit Archetype(const ArchSignature signature)
        : Signature(signature)
    {
        EntityIDStorage = new ComponentStorage(GetComponent<EntityID>());
        int Index = 0;
        for(auto CompID : Signature.Value)
        {
            CmpToStoreIndex.emplace(CompID, Index++);
            CmpStorage.emplace_back(new ComponentStorage(GetComponentByID(CompID)));
        }
    }
    
    ~Archetype()
    {
        delete EntityIDStorage;
        for(auto Store : CmpStorage)
        {
            delete Store;
        }
    }

    void AddEntity(const EntityID& Entity)
    {
        const auto Found = EntityToIndex.find(Entity);
        if(Found != EntityToIndex.end())
        {
            Error("Attempt to add already present entity %d\n", Entity);
        }
        EntityToIndex.emplace(Entity, EntityIDStorage->GetCount());
        EntityIDStorage->AddValue();
        EntityIDStorage->SetValue(EntityIDStorage->GetCount() - 1, &Entity);
        for(auto Store : CmpStorage)
        {
            Store->AddValue();
        }
    }

    void FastDelete(const EntityID& ID)
    {
        auto Found = EntityToIndex.find(ID);
        if(Found == EntityToIndex.end())
        {
            Error("Failed to find entity to delete %d\n", ID);
        }
        const int Index = Found->second;

        EntityToIndex.erase(ID);
        EntityIDStorage->FastDelete(Index);
        for(auto Store : CmpStorage)
        {
            Store->FastDelete(Index);
        }
    }

    template<typename T>
    void SetValue(const EntityID& ID, const T& Data)
    {
        auto CmpType = GetComponent<T>();
        SetValue(ID, CmpType.ID, &Data);
    }
    
    void SetValue(const EntityID& ID, const ComponentID& CmpID, const void* Data)
    {
        auto FoundEntity = EntityToIndex.find(ID);
        if(FoundEntity == EntityToIndex.end())
        {
            Error("Failed to find entity to set %d\n", ID);
        }
        auto FoundCmp = CmpToStoreIndex.find(CmpID);
        if(FoundCmp == CmpToStoreIndex.end())
        {
            Error("Failed to find Component to set %d\n", CmpID);
        }
        CmpStorage[FoundCmp->second]->SetValue(FoundEntity->second, Data);
    }
    
    template<typename T>
    T* GetValue(const EntityID& ID)
    {
        auto CmpType = GetComponent<T>();
        return reinterpret_cast<T*>(GetValue(ID, CmpType.ID));
    }
    
    char* GetValue(const EntityID& ID, const ComponentID& CmpID)
    {
        auto FoundEntity = EntityToIndex.find(ID);
        if(FoundEntity == EntityToIndex.end())
        {
            Error("Failed to find entity to set %d\n", ID);
        }
        auto FoundCmp = CmpToStoreIndex.find(CmpID);
        if(FoundCmp == CmpToStoreIndex.end())
        {
            Error("Failed to find Component to set %d\n", CmpID);
        }
        return CmpStorage[FoundCmp->second]->GetValue(FoundEntity->second);
    }

    const ArchSignature* GetSignature() const
    {
        return &Signature;
    }

    const ComponentStorage* GetEntityIDs()
    {
        return EntityIDStorage;
    }
private:
    const ArchSignature Signature;
    std::unordered_map<EntityID, int> EntityToIndex;
    std::unordered_map<ComponentID, int> CmpToStoreIndex;
    ComponentStorage* EntityIDStorage;
    std::vector<ComponentStorage*> CmpStorage;
};

class System
{
public:
    System(const ArchSignature& signature, const std::function<void(class World*, EntityID&)>& handler)
        : Signature(signature),
          Handler(handler)
    {
    }

    ArchSignature GetSignature() const
    {
        return Signature;
    }

    std::function<void(World*, EntityID&)> GetHandler() const
    {
        return Handler;
    }

    void TryAddMatch(Archetype* Arch)
    {
        const ArchSignature& ArchSig = *Arch->GetSignature();
        for(auto CmpID : Signature.Value)
        {
            if(ArchSig.Value.find(CmpID) == ArchSig.Value.end())
            {
                return;
            }
        }
        MatchedArchetypes.emplace_back(Arch);
    }

    std::vector<Archetype*>* GetMatchedArchetypes()
    {
        return &MatchedArchetypes;
    }

private:
    const ArchSignature Signature;
    std::function<void(World*, EntityID&)> Handler;
    std::vector<Archetype*> MatchedArchetypes;
};

class SetQueue
{
public:
    SetQueue(const Component CmpType)
    {
        EntityIDs = new ComponentStorage(GetComponent<EntityID>());
        ComponentBuffer = new ComponentStorage(CmpType);
    }
    ~SetQueue()
    {
        delete EntityIDs;
        delete ComponentBuffer;
    }
    SetQueue(const SetQueue& obj) = delete;

    void Enqueue(EntityID Entity, const void* Data) const
    {
        auto index = EntityIDs->GetCount();
        EntityIDs->AddValue();
        EntityIDs->SetValue(index, &Entity);
        ComponentBuffer->AddValue();
        ComponentBuffer->SetValue(index, Data);
    }

    void ForEach(std::function<void(EntityID&, char*)> Handler)
    {
        for(int i = 0; i < EntityIDs->GetCount(); i++)
        {
            EntityID* Id = reinterpret_cast<EntityID*>(EntityIDs->GetValue(i));
            char* Data = ComponentBuffer->GetValue(i);
            Handler(*Id, Data);
        }
    }

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
    World()
    {
        Archetype* Empty = new Archetype(MakeSig({}));
        Graveyard = new ComponentStorage(GetComponent<EntityID>());
        
        Archetypes.emplace_back(Empty);
        ArchSignature Sig = *Empty->GetSignature();
        ArchetypeLookup.emplace(Sig, 0);
    }

    ~World()
    {
        for(auto Archetype : Archetypes)
        {
            delete Archetype;
        }
        delete Graveyard;
        for(auto Kvp : SetQueues)
        {
            delete Kvp.second;
        }
        for(auto Kvp : RemoveQueues)
        {
            delete Kvp.second;
        }
    }
    World(const World& obj) = delete;
    
    EntityID NewEntity()
    {
        EntityID E = NextEntityID++;
        EntityArchetypeLookup.emplace(E, 0);
        Archetypes[0]->AddEntity(E);
        return E;
    }

    template<typename T>
    void Set(const EntityID& Entity, const T& Data)
    {
        Component Type = GetComponent<T>();
        Set(Entity, Type, &Data);
    }
    
    void Set(const EntityID& Entity, Component Type, const void* Data)
    {
        if(WorldLock)
        {
            auto Queue = SetQueues[Type.ID];
            if(Queue == nullptr)
            {
                Queue = SetQueues[Type.ID] = new SetQueue(Type);
            }
            Queue->Enqueue(Entity, &Data);
            return;
        }
        Archetype* CurrentArchetype = Archetypes[EntityArchetypeLookup[Entity]];
        auto ContainsType = CurrentArchetype->GetSignature()->Value.find(Type.ID);
        
        //Not in current archetype. Move entity to new table.
        if(ContainsType == CurrentArchetype->GetSignature()->Value.end())
        {
            ArchSignature NewSig(CurrentArchetype->GetSignature()->Value);
            NewSig.Value.emplace(Type.ID);
            Archetype* NewArchetype = FindOrAddArchetype(NewSig);
            MoveEntity(Entity, CurrentArchetype, NewArchetype);
            CurrentArchetype = NewArchetype;
        }
        
        CurrentArchetype->SetValue(Entity,Type.ID, Data);
    }

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
        Component Type = GetComponent<T>();
        Remove(Entity, Type);
    }
    
    void Remove(const EntityID& Entity, Component Type)
    {
        if(WorldLock)
        {
            auto Queue = RemoveQueues[Type.ID];
            if(Queue == nullptr)
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

    void Delete(const EntityID& Entity)
    {
        if(WorldLock)
        {
            Graveyard->AddValue();
            Graveyard->SetValue(Graveyard->GetCount() - 1, &Entity);
            return;
        }
        Archetype* CurrentArchetype = Archetypes[EntityArchetypeLookup[Entity]];
        CurrentArchetype->FastDelete(Entity);
        EntityArchetypeLookup.erase(Entity);
    }

private:
    Archetype* FindOrAddArchetype(const ArchSignature& Signature)
    {
        auto SigExists = ArchetypeLookup.find(Signature);
        size_t ArchIndex;
        if(SigExists == ArchetypeLookup.end())
        {
            if(WorldLock)
            {
                Error("Cannot make new archetypes while world is locked");
            }
            ArchIndex = Archetypes.size();
            Archetype* NewArch = new Archetype(Signature);
            Archetypes.emplace_back(NewArch);
            ArchetypeLookup.emplace(Signature, ArchIndex);
            for(auto& System : Systems)
            {
                System.TryAddMatch(NewArch);
            }
        }else
        {
            ArchIndex = SigExists->second;
        }
        return Archetypes[ArchIndex];
    }
    
    void MoveEntity(const EntityID& Entity, Archetype* Src, Archetype* Dest)
    {
        if(WorldLock)
        {
            Error("Move requested for %d while world is locked", Entity);
        }
        
        if(Src == Dest) return;
        
        Dest->AddEntity(Entity);
        for(auto CmpID : Src->GetSignature()->Value)
        {
            if(Dest->GetSignature()->Value.find(CmpID) == Dest->GetSignature()->Value.end()) continue;
            
            const char* CmpData = Src->GetValue(Entity, CmpID);
            Dest->SetValue(Entity, CmpID, CmpData);
        }
        Src->FastDelete(Entity);
        EntityArchetypeLookup[Entity] = ArchetypeLookup[*Dest->GetSignature()];
    }
public:
    void Tick()
    {
        for(System& System : Systems)
        {
            WorldLock = true;
            for(auto Archetype : *System.GetMatchedArchetypes())
            {
                auto Entities = Archetype->GetEntityIDs();
                for(int i = Entities->GetCount() - 1; i >= 0; i--)
                {
                    EntityID* Entity = reinterpret_cast<EntityID*>(Entities->GetValue(i));
                    System.GetHandler()(this, *Entity);
                }
            }
            WorldLock = false;
            
            for(auto Kvp : SetQueues)
            {
                Component Cmp = GetComponentByID(Kvp.first);
                Kvp.second->ForEach([&](EntityID& Entity, char* Data)
                {
                   this->Set(Entity, Cmp, Data); 
                });
                Kvp.second->Empty();
            }

            for(auto Kvp : RemoveQueues)
            {
                Component Cmp = GetComponentByID(Kvp.first);
                for(int i = 0; i < Kvp.second->GetCount(); i++)
                {
                    EntityID* E = reinterpret_cast<EntityID*>(Kvp.second->GetValue(i));
                    Remove(*E, Cmp);
                }
                Kvp.second->Empty();
            }
        }
    }

    void AddSystem(System System)
    {
        for(auto Archetype : Archetypes)
        {
            System.TryAddMatch(Archetype);
        }
        Systems.emplace_back(System);
    }

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

struct TestData
{
    float Abc;
    int Def;
};

struct OtherData
{
    int First;
    float Second;
};

int main()
{
    World Wld;

    auto E = Wld.NewEntity();
    auto E2 = Wld.NewEntity();
    Wld.Set<TestData>(E, {5.5f, 10});
    Wld.Set<TestData>(E2, {7.5f, 14});

    System IncData(MakeSig({GetComponent<TestData>().ID}), [](World* w, EntityID e)
    {
        auto Data = w->Get<TestData>(e);
        Data->Abc += 0.1f;
        Data->Def += 2;
    });
    Wld.AddSystem(IncData);
    System PrintData(MakeSig({GetComponent<TestData>().ID}), [](World* w, EntityID e)
    {
        auto Data = w->Get<TestData>(e);
        printf("Data for %d: %f, %d\n", e, Data->Abc, Data->Def);
    });
    Wld.AddSystem(PrintData);

    Wld.Tick();
    Wld.Tick();
    Wld.Remove<TestData>(E2);
    Wld.Tick();
    
    return 0;
}
#include <algorithm>
#include <functional>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <windows.h>

#include "flecs.h"

void Error(const char* Format, ...)
{
    va_list Arglist;
    va_start( Arglist, Format );
    vprintf( Format, Arglist );
    va_end( Arglist );
    abort();
}

typedef int EntityID;
typedef int ComponentID;

struct Component
{

    ComponentID ID;
    int Size;

    friend bool operator<(const Component& lhs, const Component& rhs)
    {
        return lhs.ID < rhs.ID;
    }

    friend bool operator<=(const Component& lhs, const Component& rhs)
    {
        return !(rhs < lhs);
    }

    friend bool operator>(const Component& lhs, const Component& rhs)
    {
        return rhs < lhs;
    }

    friend bool operator>=(const Component& lhs, const Component& rhs)
    {
        return !(lhs < rhs);
    }

    friend bool operator==(const Component& lhs, const Component& rhs)
    {
        return lhs.ID == rhs.ID
            && lhs.Size == rhs.Size;
    }

    friend bool operator!=(const Component& lhs, const Component& rhs)
    {
        return !(lhs == rhs);
    }
};

struct ArchSignature
{
    ArchSignature(const ArchSignature& other)
        : Value(other.Value)
    {
    }

    ArchSignature(ArchSignature&& other) noexcept
        : Value(std::move(other.Value))
    {
    }

    ArchSignature& operator=(const ArchSignature& other)
    {
        if (this == &other)
            return *this;
        Value = other.Value;
        return *this;
    }

    ArchSignature& operator=(ArchSignature&& other) noexcept
    {
        if (this == &other)
            return *this;
        Value = std::move(other.Value);
        return *this;
    }

    explicit ArchSignature(const std::unordered_set<ComponentID>& value)
        : Value(value)
    {
    }

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

ComponentID NextComponentID = 1;
std::unordered_map<ComponentID, Component> RegisteredComponents;

template<typename  T>
Component GetComponent()
{
    static Component Cmp = {NextComponentID++, sizeof(T)};
    static auto Registered = RegisteredComponents.emplace(Cmp.ID, Cmp);
    return Cmp;
}
Component GetComponentByID(ComponentID ID)
{
    const auto Found = RegisteredComponents.find(ID);
    if(Found == RegisteredComponents.end())
    {
        Error("Unrecognized Component for %d\n", ID);
    }
    return Found->second;
}

ArchSignature MakeSig(std::vector<ComponentID> Values)
{
    std::unordered_set<ComponentID> IDs;
    for(auto C : Values)
    {
        IDs.emplace(C);
    }
    return ArchSignature(IDs);
}

class ComponentStorage
{
#define IND_TO_D_PTR(Index) \
    Data + (Index * Type.Size)

public:
    ComponentStorage(const Component& type)
        : Type(type),
          MaxCount(10),
          Count(0)
    {
        Data = new char[MaxCount * Type.Size];
        SwapBuffer = new char[Type.Size];
    }
    ~ComponentStorage()
    {
        free(Data);
        free(SwapBuffer);
    }
    ComponentStorage(const ComponentStorage& obj) = delete;

private:
    void Grow()
    {
        const int Growth = min(static_cast<uint32_t>(MaxCount * 0.5), 1000);
        if(MaxCount > INT_MAX - Growth)
        {
            Error("Grow would overflow %d+%d\n", MaxCount, Growth);
        }
        MaxCount += Growth;
        char* NewData = new char[MaxCount * Type.Size];
        if(!NewData)
        {
            Error("Failed to malloc\n");
        }
        delete [] Data;
        memmove(NewData, Data, Count * Type.Size);
        Data = NewData;
    }

public:
    char* GetValue(const int Index) const
    {
        if(Index >= Count || Index < 0)
        {
            Error("Request Get OOB [%d] %d\n", Index, Count);
        }
        return IND_TO_D_PTR(Index);
    }

    void SetValue(const int Index, const void* Value) const
    {
        if(Index >= Count || Index < 0)
        {
            Error("Request Set OOB [%d] %d\n", Index, Count);
        }
        memcpy(IND_TO_D_PTR(Index), Value, Type.Size);
    }

    void AddValue()
    {
        if(Count == MaxCount)
        {
            Grow();
        }
        memset(IND_TO_D_PTR(Count), 0, Type.Size);
        Count++;
    }

    void Swap(int IndexOne, int IndexTwo) const
    {
        if(IndexOne >= Count || IndexTwo >= Count || IndexOne < 0 || IndexTwo < 0)
        {
            Error("Swap is OOB [%d]: %d %d\n", Count, IndexOne, IndexTwo);
        }
        memcpy(SwapBuffer, IND_TO_D_PTR(IndexOne), Type.Size);
        memcpy(IND_TO_D_PTR(IndexOne), IND_TO_D_PTR(IndexTwo), Type.Size);
        memcpy(IND_TO_D_PTR(IndexTwo), SwapBuffer, Type.Size);
    }

    void FastDelete(int Index)
    {
        if(Index >= Count || Index < 0)
        {
            Error("FastDelete is OOB [%d]: %d\n", Count, Index);
        }
        if(Index != Count - 1)
        {
            Swap(Index, Count - 1);
        }
        Count--;
    }

    void Empty()
    {
        Count = 0;
    }

    int GetCount() const
    {
        return Count;
    }
    const Component* GetType() const
    {
        return &Type;
    }
#undef IND_TO_D_PTR

private:
    const Component Type;
    int MaxCount;
    int Count;
    char* Data;
    char* SwapBuffer;
};

class Archetype
{
public:
    explicit Archetype(const ArchSignature signature)
        : Signature(signature)
    {
        EntityIDStorage = new ComponentStorage(GetComponent<EntityID>());
        int Index = 0;
        for(auto CompID : Signature.Value)
        {
            CmpToStoreIndex.emplace(CompID, Index++);
            CmpStorage.emplace_back(new ComponentStorage(GetComponentByID(CompID)));
        }
    }
    
    ~Archetype()
    {
        delete EntityIDStorage;
        for(auto Store : CmpStorage)
        {
            delete Store;
        }
    }

    void AddEntity(const EntityID& Entity)
    {
        const auto Found = EntityToIndex.find(Entity);
        if(Found != EntityToIndex.end())
        {
            Error("Attempt to add already present entity %d\n", Entity);
        }
        EntityToIndex.emplace(Entity, EntityIDStorage->GetCount());
        EntityIDStorage->AddValue();
        EntityIDStorage->SetValue(EntityIDStorage->GetCount() - 1, &Entity);
        for(auto Store : CmpStorage)
        {
            Store->AddValue();
        }
    }

    void FastDelete(const EntityID& ID)
    {
        auto Found = EntityToIndex.find(ID);
        if(Found == EntityToIndex.end())
        {
            Error("Failed to find entity to delete %d\n", ID);
        }
        const int Index = Found->second;

        EntityToIndex.erase(ID);
        EntityIDStorage->FastDelete(Index);
        for(auto Store : CmpStorage)
        {
            Store->FastDelete(Index);
        }
    }

    template<typename T>
    void SetValue(const EntityID& ID, const T& Data)
    {
        auto CmpType = GetComponent<T>();
        SetValue(ID, CmpType.ID, &Data);
    }
    
    void SetValue(const EntityID& ID, const ComponentID& CmpID, const void* Data)
    {
        auto FoundEntity = EntityToIndex.find(ID);
        if(FoundEntity == EntityToIndex.end())
        {
            Error("Failed to find entity to set %d\n", ID);
        }
        auto FoundCmp = CmpToStoreIndex.find(CmpID);
        if(FoundCmp == CmpToStoreIndex.end())
        {
            Error("Failed to find Component to set %d\n", CmpID);
        }
        CmpStorage[FoundCmp->second]->SetValue(FoundEntity->second, Data);
    }
    
    template<typename T>
    T* GetValue(const EntityID& ID)
    {
        auto CmpType = GetComponent<T>();
        return reinterpret_cast<T*>(GetValue(ID, CmpType.ID));
    }
    
    char* GetValue(const EntityID& ID, const ComponentID& CmpID)
    {
        auto FoundEntity = EntityToIndex.find(ID);
        if(FoundEntity == EntityToIndex.end())
        {
            Error("Failed to find entity to set %d\n", ID);
        }
        auto FoundCmp = CmpToStoreIndex.find(CmpID);
        if(FoundCmp == CmpToStoreIndex.end())
        {
            Error("Failed to find Component to set %d\n", CmpID);
        }
        return CmpStorage[FoundCmp->second]->GetValue(FoundEntity->second);
    }

    const ArchSignature* GetSignature() const
    {
        return &Signature;
    }

    const ComponentStorage* GetEntityIDs()
    {
        return EntityIDStorage;
    }
private:
    const ArchSignature Signature;
    std::unordered_map<EntityID, int> EntityToIndex;
    std::unordered_map<ComponentID, int> CmpToStoreIndex;
    ComponentStorage* EntityIDStorage;
    std::vector<ComponentStorage*> CmpStorage;
};

class System
{
public:
    System(const ArchSignature& signature, const std::function<void(class World*, EntityID&)>& handler)
        : Signature(signature),
          Handler(handler)
    {
    }

    ArchSignature GetSignature() const
    {
        return Signature;
    }

    std::function<void(World*, EntityID&)> GetHandler() const
    {
        return Handler;
    }

    void TryAddMatch(Archetype* Arch)
    {
        const ArchSignature& ArchSig = *Arch->GetSignature();
        for(auto CmpID : Signature.Value)
        {
            if(ArchSig.Value.find(CmpID) == ArchSig.Value.end())
            {
                return;
            }
        }
        MatchedArchetypes.emplace_back(Arch);
    }

    std::vector<Archetype*>* GetMatchedArchetypes()
    {
        return &MatchedArchetypes;
    }

private:
    const ArchSignature Signature;
    std::function<void(World*, EntityID&)> Handler;
    std::vector<Archetype*> MatchedArchetypes;
};

class SetQueue
{
public:
    SetQueue(const Component CmpType)
    {
        EntityIDs = new ComponentStorage(GetComponent<EntityID>());
        ComponentBuffer = new ComponentStorage(CmpType);
    }
    ~SetQueue()
    {
        delete EntityIDs;
        delete ComponentBuffer;
    }
    SetQueue(const SetQueue& obj) = delete;

    void Enqueue(EntityID Entity, const void* Data) const
    {
        auto index = EntityIDs->GetCount();
        EntityIDs->AddValue();
        EntityIDs->SetValue(index, &Entity);
        ComponentBuffer->AddValue();
        ComponentBuffer->SetValue(index, Data);
    }

    void ForEach(std::function<void(EntityID&, char*)> Handler)
    {
        for(int i = 0; i < EntityIDs->GetCount(); i++)
        {
            EntityID* Id = reinterpret_cast<EntityID*>(EntityIDs->GetValue(i));
            char* Data = ComponentBuffer->GetValue(i);
            Handler(*Id, Data);
        }
    }

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
    World()
    {
        Archetype* Empty = new Archetype(MakeSig({}));
        Graveyard = new ComponentStorage(GetComponent<EntityID>());
        
        Archetypes.emplace_back(Empty);
        ArchSignature Sig = *Empty->GetSignature();
        ArchetypeLookup.emplace(Sig, 0);
    }

    ~World()
    {
        for(auto Archetype : Archetypes)
        {
            delete Archetype;
        }
        delete Graveyard;
        for(auto Kvp : SetQueues)
        {
            delete Kvp.second;
        }
        for(auto Kvp : RemoveQueues)
        {
            delete Kvp.second;
        }
    }
    World(const World& obj) = delete;
    
    EntityID NewEntity()
    {
        EntityID E = NextEntityID++;
        EntityArchetypeLookup.emplace(E, 0);
        Archetypes[0]->AddEntity(E);
        return E;
    }

    template<typename T>
    void Set(const EntityID& Entity, const T& Data)
    {
        Component Type = GetComponent<T>();
        Set(Entity, Type, &Data);
    }
    
    void Set(const EntityID& Entity, Component Type, const void* Data)
    {
        if(WorldLock)
        {
            auto Queue = SetQueues[Type.ID];
            if(Queue == nullptr)
            {
                Queue = SetQueues[Type.ID] = new SetQueue(Type);
            }
            Queue->Enqueue(Entity, &Data);
            return;
        }
        Archetype* CurrentArchetype = Archetypes[EntityArchetypeLookup[Entity]];
        auto ContainsType = CurrentArchetype->GetSignature()->Value.find(Type.ID);
        
        //Not in current archetype. Move entity to new table.
        if(ContainsType == CurrentArchetype->GetSignature()->Value.end())
        {
            ArchSignature NewSig(CurrentArchetype->GetSignature()->Value);
            NewSig.Value.emplace(Type.ID);
            Archetype* NewArchetype = FindOrAddArchetype(NewSig);
            MoveEntity(Entity, CurrentArchetype, NewArchetype);
            CurrentArchetype = NewArchetype;
        }
        
        CurrentArchetype->SetValue(Entity,Type.ID, Data);
    }

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
        Component Type = GetComponent<T>();
        Remove(Entity, Type);
    }
    
    void Remove(const EntityID& Entity, Component Type)
    {
        if(WorldLock)
        {
            auto Queue = RemoveQueues[Type.ID];
            if(Queue == nullptr)
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

    void Delete(const EntityID& Entity)
    {
        if(WorldLock)
        {
            Graveyard->AddValue();
            Graveyard->SetValue(Graveyard->GetCount() - 1, &Entity);
            return;
        }
        Archetype* CurrentArchetype = Archetypes[EntityArchetypeLookup[Entity]];
        CurrentArchetype->FastDelete(Entity);
        EntityArchetypeLookup.erase(Entity);
    }

private:
    Archetype* FindOrAddArchetype(const ArchSignature& Signature)
    {
        auto SigExists = ArchetypeLookup.find(Signature);
        size_t ArchIndex;
        if(SigExists == ArchetypeLookup.end())
        {
            if(WorldLock)
            {
                Error("Cannot make new archetypes while world is locked");
            }
            ArchIndex = Archetypes.size();
            Archetype* NewArch = new Archetype(Signature);
            Archetypes.emplace_back(NewArch);
            ArchetypeLookup.emplace(Signature, ArchIndex);
            for(auto& System : Systems)
            {
                System.TryAddMatch(NewArch);
            }
        }else
        {
            ArchIndex = SigExists->second;
        }
        return Archetypes[ArchIndex];
    }
    
    void MoveEntity(const EntityID& Entity, Archetype* Src, Archetype* Dest)
    {
        if(WorldLock)
        {
            Error("Move requested for %d while world is locked", Entity);
        }
        
        if(Src == Dest) return;
        
        Dest->AddEntity(Entity);
        for(auto CmpID : Src->GetSignature()->Value)
        {
            if(Dest->GetSignature()->Value.find(CmpID) == Dest->GetSignature()->Value.end()) continue;
            
            const char* CmpData = Src->GetValue(Entity, CmpID);
            Dest->SetValue(Entity, CmpID, CmpData);
        }
        Src->FastDelete(Entity);
        EntityArchetypeLookup[Entity] = ArchetypeLookup[*Dest->GetSignature()];
    }
public:
    void Tick()
    {
        for(System& System : Systems)
        {
            WorldLock = true;
            for(auto Archetype : *System.GetMatchedArchetypes())
            {
                auto Entities = Archetype->GetEntityIDs();
                for(int i = Entities->GetCount() - 1; i >= 0; i--)
                {
                    EntityID* Entity = reinterpret_cast<EntityID*>(Entities->GetValue(i));
                    System.GetHandler()(this, *Entity);
                }
            }
            WorldLock = false;
            
            for(auto Kvp : SetQueues)
            {
                Component Cmp = GetComponentByID(Kvp.first);
                Kvp.second->ForEach([&](EntityID& Entity, char* Data)
                {
                   this->Set(Entity, Cmp, Data); 
                });
                Kvp.second->Empty();
            }

            for(auto Kvp : RemoveQueues)
            {
                Component Cmp = GetComponentByID(Kvp.first);
                for(int i = 0; i < Kvp.second->GetCount(); i++)
                {
                    EntityID* E = reinterpret_cast<EntityID*>(Kvp.second->GetValue(i));
                    Remove(*E, Cmp);
                }
                Kvp.second->Empty();
            }
        }
    }

    void AddSystem(System System)
    {
        for(auto Archetype : Archetypes)
        {
            System.TryAddMatch(Archetype);
        }
        Systems.emplace_back(System);
    }

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

struct TestData
{
    float Abc;
    int Def;
};

struct OtherData
{
    int First;
    float Second;
};

int main()
{
    World Wld;

    auto E = Wld.NewEntity();
    auto E2 = Wld.NewEntity();
    Wld.Set<TestData>(E, {5.5f, 10});
    Wld.Set<TestData>(E2, {7.5f, 14});

    System IncData(MakeSig({GetComponent<TestData>().ID}), [](World* w, EntityID e)
    {
        auto Data = w->Get<TestData>(e);
        Data->Abc += 0.1f;
        Data->Def += 2;
    });
    Wld.AddSystem(IncData);
    System PrintData(MakeSig({GetComponent<TestData>().ID}), [](World* w, EntityID e)
    {
        auto Data = w->Get<TestData>(e);
        printf("Data for %d: %f, %d\n", e, Data->Abc, Data->Def);
    });
    Wld.AddSystem(PrintData);

    Wld.Tick();
    Wld.Tick();
    Wld.Remove<TestData>(E2);
    Wld.Tick();
    
    return 0;
}
