#include <Windows.h>
#include <cassert>
#include <cmath>
#include <cstdio>

#include "../CoD3Host/campaign_movement.cpp"

namespace
{
    constexpr uint32_t Move = 0x1000;
    constexpr uint32_t State = 0x2000;
    int multiplier = 1;
    Coop::Mode coop = Coop::Mode::Off;
    unsigned nativeCalls = 0;
    double nativeSpeed = 0;

    void CommitPage(uint8_t* base, uint32_t address)
    {
        auto* page = base + (address & ~0xFFFu);
        assert(VirtualAlloc(page, 0x1000, MEM_COMMIT, PAGE_READWRITE) == page);
    }

    void CheckSpeed(uint8_t* base, uint32_t caller, double expected, double wishSpeed = 190.0)
    {
        PPCContext ctx{};
        ctx.lr = caller;
        ctx.f1.f64 = wishSpeed;
        nativeCalls = 0;
        sub_82478358(ctx, base);
        assert(nativeCalls == 1);
        assert(std::abs(nativeSpeed - expected) < 0.001);
    }
}

int Settings::WalkSpeedMultiplier() { return multiplier; }
Coop::Mode Coop::GetMode() { return coop; }
PPC_FUNC_IMPL(__imp__sub_82478358)
{
    ++nativeCalls;
    nativeSpeed = ctx.f1.f64;
}

int main()
{
    auto* base = static_cast<uint8_t*>(VirtualAlloc(nullptr, 0x90000000ull, MEM_RESERVE, PAGE_NOACCESS));
    assert(base);
    CommitPage(base, CurrentPmove);
    CommitPage(base, PmoveLocals + 100);
    CommitPage(base, Move);
    CommitPage(base, State);
    Guest::Write32(base, CurrentPmove, Move);
    Guest::Write32(base, Move, State);
    Guest::Write32(base, State + 184, 66);

    CheckSpeed(base, GroundWalkReturn, 190.0);
    multiplier = 2;
    CheckSpeed(base, GroundWalkReturn, 380.0);
    multiplier = 3;
    CheckSpeed(base, GroundWalkReturn, 570.0);
    multiplier = 4;
    CheckSpeed(base, GroundWalkReturn, 760.0);
    Guest::Write32(base, State + 44, 0x40);  // Backwards.
    CheckSpeed(base, GroundWalkReturn, 760.0);
    Guest::Write32(base, State + 44, 0);
    coop = Coop::Mode::Host;
    CheckSpeed(base, GroundWalkReturn, 190.0);
    coop = Coop::Mode::Off;

    CheckSpeed(base, 0x82490CDC, 190.0);  // Air.
    CheckSpeed(base, 0x82491044, 190.0);  // Ladder.
    CheckSpeed(base, GroundWalkReturn, 0.0, 0.0);

    for (uint32_t flags : {0x1u, 0x2u, 0x80u, 0x2000u, 0x4000u, 0x10000u})
    {
        Guest::Write32(base, State + 44, flags);
        CheckSpeed(base, GroundWalkReturn, 190.0);
    }
    Guest::Write32(base, State + 44, 0);

    Guest::Write32(base, State + 184, 11);
    CheckSpeed(base, GroundWalkReturn, 190.0);
    Guest::Write32(base, State + 184, 66);
    Guest::Write32(base, State + 192, 1);
    CheckSpeed(base, GroundWalkReturn, 190.0);
    Guest::Write32(base, State + 192, 0);

    Guest::Write32(base, State + 36, 1);
    CheckSpeed(base, GroundWalkReturn, 190.0);
    Guest::Write32(base, State + 36, 0);
    Guest::Write32(base, Move + 8, 8);
    CheckSpeed(base, GroundWalkReturn, 190.0);
    Guest::Write32(base, Move + 8, 0);
    Guest::Write8(base, Move + 305, 100);
    CheckSpeed(base, GroundWalkReturn, 190.0);
    Guest::Write8(base, Move + 305, 0);
    Guest::Write32(base, PmoveLocals + 100, 2);
    CheckSpeed(base, GroundWalkReturn, 190.0);

    assert(VirtualFree(base, 0, MEM_RELEASE));
    std::puts("PASS: campaign acceleration call and walk-state exclusions");
}
