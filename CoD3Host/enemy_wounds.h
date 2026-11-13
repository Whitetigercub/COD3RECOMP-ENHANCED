#pragma once

#include <cstdint>
#include "blood.h"
#include "blood_math.h"

namespace EnemyWounds
{
    constexpr int MaxEnemies = 12;
    constexpr int WoundsPerEnemy = 5;
    constexpr int MaxVertices = MaxEnemies * WoundsPerEnemy * 6;

    struct Hit
    {
        uint32_t handle = 0, model = 0, actor = 0, tag = 0;
        int health = 0;
        BloodMath::V point, normal;
    };

    Hit BeforeDamage(PPCContext& ctx, uint8_t* base);
    void AfterDamage(uint8_t* base, const Hit& hit);
    void Update(PPCContext& ctx, uint8_t* base);
    void Clear();
    int Append(Blood::WorldVertex* output, int capacity);
}
