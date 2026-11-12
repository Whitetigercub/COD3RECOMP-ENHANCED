#include <Windows.h>
#include <cassert>
#include <cstdio>

#include "../CoD3Host/enemy_wounds.cpp"

namespace
{
    constexpr uint32_t Entity = 0x40010000, Model = 0x40020000, Actor = 0x40030000;
    constexpr uint32_t Stack = 0x40040000, Trace = 0x40050000, ScratchMemory = 0x40060000;
    constexpr uint32_t ClientArray = 0x40080000, Player = 0x40090000;
    constexpr uint32_t EnemySentient = 0x400A0000, PlayerSentient = 0x400B0000;
    constexpr uint32_t Handle = (7u << 14) | 12;
    constexpr uint32_t HandleEntry = 0x82A2DCD0 + 12 * 8;
    bool enabled = false;
    Coop::Mode mode = Coop::Mode::Off;
    bool rotated = false, poseValid = true;

    void Page(uint64_t address)
    {
        auto* page = Guest::Base + (address & ~0xFFFull);
        assert(VirtualAlloc(page, 0x1000, MEM_COMMIT, PAGE_READWRITE) == page);
    }

    void Word(uint32_t address, uint32_t value) { Guest::Write32(Guest::Base, address, value); }
    void Float(uint32_t address, float value) { Word(address, std::bit_cast<uint32_t>(value)); }
    void Vector(uint32_t address, BloodMath::V v)
    {
        Float(address, v.x);
        Float(address + 4, v.y);
        Float(address + 8, v.z);
    }

    PPC_FUNC(FakeIntern) { ctx.r3.u32 = 5; }
    PPC_FUNC(FakePose)
    {
        if (!poseValid) return; // Simulate native success without an output matrix.
        Vector(ctx.r5.u32, rotated ? BloodMath::V{0, 1, 0} : BloodMath::V{1, 0, 0});
        Vector(ctx.r5.u32 + 16, rotated ? BloodMath::V{-1, 0, 0} : BloodMath::V{0, 1, 0});
        Vector(ctx.r5.u32 + 32, {0, 0, 1});
        Vector(ctx.r5.u32 + 48, rotated ? BloodMath::V{10, 20, 30} : BloodMath::V{0, 0, 0});
        ctx.r3.u32 = 1;
    }

    void Bind(uint32_t guest, PPCFunc* fn)
    {
        const uint64_t address = Guest::FuncTableBase + uint64_t(guest - PPC_CODE_BASE) * 2;
        Page(address);
        *reinterpret_cast<PPCFunc**>(Guest::Base + address) = fn;
    }

    void CheckCenter(const Blood::WorldVertex* verts, BloodMath::V expected)
    {
        BloodMath::V center;
        for (int i = 0; i < 6; ++i) center = center + BloodMath::V{verts[i].x, verts[i].y, verts[i].z};
        center = center * (1.0f / 6.0f);
        const auto delta = center - expected;
        assert(BloodMath::Dot(delta, delta) < 0.0001f);
    }
}

bool Settings::EnemyWoundsEnabled() { return enabled; }
Coop::Mode Coop::GetMode() { return mode; }
bool Overlay::IsOpen() { return false; }
namespace Guest
{
    uint8_t* Base = nullptr;
    PPCFunc* LookupModule(uint32_t) { return nullptr; }
    uint32_t AllocatePhysical(uint32_t)
    {
        Page(ScratchMemory + 0xE000);
        Page(ScratchMemory + 0xF000);
        return ScratchMemory;
    }
}

int main()
{
    using namespace EnemyWounds;
    Guest::Base = static_cast<uint8_t*>(VirtualAlloc(nullptr, 0xC0000000ull, MEM_RESERVE, PAGE_NOACCESS));
    assert(Guest::Base);
    for (uint32_t page : {Entity, Stack, Trace, ClientArray, Player, EnemySentient,
                          PlayerSentient, HandleEntry, 0x82A2AB40u}) Page(page);
    Bind(0x820CEBB8, FakeIntern);
    Bind(0x8254EC98, FakePose);
    Word(HandleEntry, Entity);
    Word(HandleEntry + 4, 7);
    Word(Entity + 500, Handle);
    Word(Entity + 508, Model);
    Word(Entity + 532, Actor);
    Word(Entity + 536, EnemySentient);
    Word(Entity + 776, 120);
    Word(EnemySentient + 4, 1);
    Word(0x82A2AB40, ClientArray);
    Word(ClientArray + 4, Player);
    Word(Player + 536, PlayerSentient);
    Word(PlayerSentient + 4, 2);
    Word(Stack + 84, 4);
    Vector(Trace, {5, 2, 0});
    Vector(Trace + 16, {1, 0, 0});

    PPCContext ctx{};
    ctx.lr = 0x82565D14;
    ctx.r1.u32 = Stack;
    ctx.r3.u32 = Entity;
    ctx.r7.u32 = Trace;
    ctx.r8.s32 = 20;
    ctx.r10.u32 = 1;
    auto* base = Guest::Base;
    assert(!BeforeDamage(ctx, base).handle);
    enabled = true;
    mode = Coop::Mode::Host;
    assert(!BeforeDamage(ctx, base).handle);
    mode = Coop::Mode::Off;
    const auto first = BeforeDamage(ctx, base);
    assert(first.handle == Handle);
    AfterDamage(base, first);
    Update(ctx, base);
    Blood::WorldVertex verts[12]{};
    assert(Append(verts, 12) == 0); // No confirmed health loss.

    Word(Entity + 776, 100);
    AfterDamage(base, first);
    Update(ctx, base);
    assert(Append(verts, 5) == 0);
    assert(Append(verts, 12) == 6);
    CheckCenter(verts, {5.24f, 2, 0});
    rotated = true;
    Update(ctx, base);
    assert(Append(verts, 12) == 6);
    CheckCenter(verts, {8, 25.24f, 30});

    Vector(Trace, {7, 26, 30});
    const auto second = BeforeDamage(ctx, base);
    Word(Entity + 776, 80);
    AfterDamage(base, second);
    Update(ctx, base);
    assert(Append(verts, 12) == 6); // Repeated hit replaces the same owner's wound.
    CheckCenter(verts, {7.24f, 26, 30});
    Word(HandleEntry + 4, 8);
    Update(ctx, base);
    assert(Append(verts, 12) == 0); // Recycled handle slot.
    Word(HandleEntry + 4, 7);
    AfterDamage(base, second);
    Word(Entity + 776, 0);
    Update(ctx, base);
    assert(Append(verts, 12) == 0);
    Word(Entity + 776, 80);
    poseValid = false;
    assert(!BeforeDamage(ctx, base).handle);
    assert(VirtualFree(base, 0, MEM_RELEASE));
    std::puts("PASS: wound gates, hit confirmation, animated attachment, replacement and cleanup");
}
