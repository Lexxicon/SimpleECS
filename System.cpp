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
    for (auto CmpID : Signature)
    {
        const auto Found = ArchSig.find(CmpID);
        if (Found == ArchSig.end())
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
