#include "MoveSystem.h"
#include <vector>

#include "../World.h"
#include "../Entity.h"


struct Position
{
    float X;
    float Y;
};

struct Velocity
{
    float X;
    float Y;
};

int Movement()
{
    World Wld;
    
    auto E = Wld.NewEntity();
    auto E2 = Wld.NewEntity();
    E.Set<Position>({5.5f, 10})
        .Set<Velocity>({1.0, 0.1f});
    
    E2.Set<Position>({2, 0})
        .Set<Velocity>({-1.1f, -0.2f});
    
    Wld.AddSystem({
        MakeSig({
            GetComponent<Position>().ID,
            GetComponent<Velocity>().ID
        }),
        [](World* w, Entity e)
        {
            auto Pos = e.Get<Position>();
            auto Vel = e.Get<Velocity>();
            Pos->X += Vel->X;
            Pos->Y += Vel->Y;
        }});
    
    Wld.AddSystem({
        MakeSig({GetComponent<Position>().ID}),
        [](World* w, Entity e)
        {
            auto Pos = e.Get<Position>();
            printf("Data for %d: %f, %f\n", e.GetID(), Pos->X, Pos->Y);
        }});

    Wld.AddSystem({
        MakeSig({GetComponent<Position>().ID}),
        [](World* w, Entity e)
        {
            auto Pos = e.Get<Position>();
            if(Pos->X > 10 || Pos->X < -10
                ||Pos->Y > 10 || Pos->Y < -10)
            {
                printf("Killing %d\n", e.GetID());
                w->NewEntity()
                    .Set<Position>({0, 1})
                    .Set<Velocity>({1, -0.5});
                w->Delete(e.GetID());
            }
        }});

    for(int i = 0; i < 100; i++)
    {
        Wld.Tick();
    }
    
    return 0;
}
