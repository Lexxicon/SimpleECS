#pragma once

#include "Component.h"

class ComponentStorage
{

public:
    ComponentStorage(const Component& type);

    ~ComponentStorage();
    ComponentStorage(const ComponentStorage& obj) = delete;

private:
    void Grow();

public:
    char* GetValue(int Index) const;
    void SetValue(int Index, const void* Value) const;
    void AddValue();
    void Swap(int IndexOne, int IndexTwo) const;
    void FastDelete(int Index);
    void Empty();
    int GetCount() const;
    const Component* GetType() const;

private:
    const Component Type;
    int MaxCount;
    int Count;
    char* Data;
    char* SwapBuffer;
};
