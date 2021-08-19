#include "Component.h"

#include "ErrorHandling.h"

ComponentID GetNextID()
{
    static ComponentID NextComponentID = 0;
    return NextComponentID++;
}

std::unordered_map<ComponentID, Component>* GetRegisteredComponents()
{
    static std::unordered_map<ComponentID, Component> RegisteredComponents;
    return &RegisteredComponents;
}

Component GetComponentByID(ComponentID ID)
{
    const auto Found = GetRegisteredComponents()->find(ID);
    if (Found == GetRegisteredComponents()->end())
    {
        Error("Unrecognized Component for %d\n", ID);
    }
    return Found->second;
}
