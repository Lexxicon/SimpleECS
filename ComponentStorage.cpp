// #include "ComponentStorage.h"
//
// #include "ErrorHandling.h"
//
//
// #define IND_TO_D_PTR(Index) Data + (Index * Type.Size)
//
// ComponentStorage::ComponentStorage(const Component& type): Type(type),
//                                                            MaxCount(10),
//                                                            Count(0)
// {
//     Data = new char[MaxCount * Type.Size];
//     SwapBuffer = new char[Type.Size];
// }
//
// ComponentStorage::~ComponentStorage()
// {
//     delete [] Data;
//     delete [] SwapBuffer;
// }
//
// void ComponentStorage::Grow()
// {
//     const int Growth = static_cast<uint32_t>(std::min(0.5 * MaxCount, 1000.0));
//     if (MaxCount > INT_MAX - Growth)
//     {
//         Error("Grow would overflow %d+%d\n", MaxCount, Growth);
//     }
//     MaxCount += Growth;
//     char* NewData = new char[MaxCount * Type.Size];
//     memmove(NewData, Data, Count * Type.Size);
//     delete [] Data;
//     Data = NewData;
// }
//
// char* ComponentStorage::GetValue(const int Index) const
// {
//     if (Index >= Count || Index < 0)
//     {
//         Error("Request Get OOB [%d] %d\n", Index, Count);
//     }
//     return IND_TO_D_PTR(Index);
// }
//
// void ComponentStorage::SetValue(const int Index, const void* Value) const
// {
//     if (Index >= Count || Index < 0)
//     {
//         Error("Request Set OOB [%d] %d\n", Index, Count);
//     }
//     memcpy(IND_TO_D_PTR(Index), Value, Type.Size);
// }
//
// void ComponentStorage::AddValue()
// {
//     if (Count == MaxCount)
//     {
//         Grow();
//     }
//     memset(IND_TO_D_PTR(Count), 0, Type.Size);
//     Count++;
// }
//
// void ComponentStorage::Swap(int IndexOne, int IndexTwo) const
// {
//     if (IndexOne >= Count || IndexTwo >= Count || IndexOne < 0 || IndexTwo < 0)
//     {
//         Error("Swap is OOB [%d]: %d %d\n", Count, IndexOne, IndexTwo);
//     }
//     memcpy(SwapBuffer, IND_TO_D_PTR(IndexOne), Type.Size);
//     memcpy(IND_TO_D_PTR(IndexOne), IND_TO_D_PTR(IndexTwo), Type.Size);
//     memcpy(IND_TO_D_PTR(IndexTwo), SwapBuffer, Type.Size);
// }
//
// void ComponentStorage::FastDelete(int Index)
// {
//     if (Index >= Count || Index < 0)
//     {
//         Error("FastDelete is OOB [%d]: %d\n", Count, Index);
//     }
//     if (Index != Count - 1)
//     {
//         Swap(Index, Count - 1);
//     }
//     Count--;
// }
//
// void ComponentStorage::Empty()
// {
//     Count = 0;
// }
//
// int ComponentStorage::GetCount() const
// {
//     return Count;
// }
//
// const Component* ComponentStorage::GetType() const
// {
//     return &Type;
// }
//
// #undef IND_TO_D_PTR
