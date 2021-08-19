#pragma once
#include <functional>

#include "Archetype.h"

class Entity;

class System
{
public:
    System(const ArchSignature& signature, const std::function<void(class World*, Entity&)>& handler);

    ArchSignature GetSignature() const;
    std::function<void(World*, Entity&)> GetHandler() const;
    void TryAddMatch(Archetype* Arch);
    std::vector<Archetype*>* GetMatchedArchetypes();

private:
    const ArchSignature Signature;
    std::function<void(World*, Entity&)> Handler;
    std::vector<Archetype*> MatchedArchetypes;
};
