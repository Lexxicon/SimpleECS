#include "System.h"

System::System(const ArchSignature& signature, const std::function<void(World*, Entity&)>& handler):
    Signature(signature),
    Handler(handler)
{
}

ArchSignature System::GetSignature() const
{
    return Signature;
}

std::function<void(World*, Entity&)> System::GetHandler() const
{
    return Handler;
}

void System::TryAddMatch(Archetype* Arch)
{
    const ArchSignature& ArchSig = *Arch->GetSignature();
    for (auto CmpID : Signature.Value)
    {
        if (ArchSig.Value.find(CmpID) == ArchSig.Value.end())
        {
            return;
        }
    }
    MatchedArchetypes.emplace_back(Arch);
}

std::vector<Archetype*>* System::GetMatchedArchetypes()
{
    return &MatchedArchetypes;
}
