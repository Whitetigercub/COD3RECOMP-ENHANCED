#ifndef COD3_MP
#include "coop.h"
#include "kernel.h"
#include "settings.h"

namespace
{
    constexpr uint32_t GroundWalkReturn = 0x824923A4;
    constexpr uint32_t CurrentPmove = 0x829F1040;
    constexpr uint32_t PmoveLocals = 0x82A922B0;

    // Standing ground walk was observed with the backwards bit and these
    // transient high bits. Let any other movement state use its native speed.
    constexpr uint32_t GroundWalkFlags = 0x00600040;
    constexpr uint32_t AimCommand = 0x8;
    constexpr uint32_t NoGroundFriction = 0x2;
    constexpr uint32_t StandingHeight = 66;

    bool IsNormalWalk(uint8_t* base)
    {
        const uint32_t move = Guest::Read32(base, CurrentPmove);
        const uint32_t state = Guest::Read32(base, move);
        return Guest::Read32(base, state + 36) == 0 &&
               (Guest::Read32(base, state + 44) & ~GroundWalkFlags) == 0 &&
               Guest::Read32(base, state + 184) == StandingHeight &&
               Guest::Read32(base, state + 192) == 0 &&
               (Guest::Read32(base, move + 8) & AimCommand) == 0 &&
               Guest::Read8(base, move + 305) == 0 &&
               (Guest::Read32(base, PmoveLocals + 100) & NoGroundFriction) == 0;
    }
}

extern "C" PPC_FUNC(__imp__sub_82478358);
PPC_FUNC(sub_82478358)
{
    // The ground-walk caller has finished computing the desired speed in f1.
    // Other callers accelerate in air, on ladders and in special movement modes.
    if (ctx.lr == GroundWalkReturn && ctx.f1.f64 > 0.0)
    {
        const int bonus = Settings::WalkSpeedBonus();
        if (bonus > 0 && Coop::GetMode() == Coop::Mode::Off && IsNormalWalk(base))
            ctx.f1.f64 = static_cast<float>(ctx.f1.f64 * (1.0 + bonus / 100.0));
    }

    __imp__sub_82478358(ctx, base);
}
#endif
