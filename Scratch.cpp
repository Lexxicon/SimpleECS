#include <vector>

#include "Entity.h"
#include "World.h"
#include "Examples/MoveSystem.h"

struct MyData
{
    float X;
};

int main()
{
    // Movement();

    World Wrld;

    auto E1 = Wrld.NewEntity();
    MyData D = ;
    E1.Set<MyData>({5});
    printf("%f\n", E1.Get<MyData>()->X);
    
    return 0;
}
