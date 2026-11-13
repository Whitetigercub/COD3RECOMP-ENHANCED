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
    constexpr uint32_t SecondEntity = Entity + 0x1000, SecondHandle = (3u << 14) | 13;
    bool enabled = false;
    Coop::Mode mode = Coop::Mode::Off;
    bool rotated = false, poseValid = true, skewed = false;
    int poseReads = 0;

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

    PPC_FUNC(FakeIntern) { ctx.r3.u32 = ctx.r3.u32 == 0x8206921C ? 5 : 6; }
    PPC_FUNC(FakePose)
    {
        ++poseReads;
        if (!poseValid) return; // Simulate native success without an output matrix.
        Vector(ctx.r5.u32, rotated ? BloodMath::V{0, 1, 0} : BloodMath::V{skewed ? 1.03f : 1.0f, 0, 0});
        Vector(ctx.r5.u32 + 16, rotated ? BloodMath::V{-1, 0, 0} : BloodMath::V{skewed ? .05f : 0.0f, 1, 0});
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
    for (uint32_t page : {Entity, SecondEntity, Stack, Trace, ClientArray, Player, EnemySentient,
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
    Blood::WorldVertex verts[MaxVertices]{};
    assert(Append(verts, 12) == 0); // No confirmed health loss.

    Word(Entity + 776, 100);
    AfterDamage(base, first);
    Update(ctx, base);
    assert(Append(verts, 5) == 0);
    assert(Append(verts, 12) == 6);
    CheckCenter(verts, {5.20f, 2, 0});
    skewed = true;
    Update(ctx, base);
    assert(Append(verts, 12) == 6);
    CheckCenter(verts, {5.20f, 2, 0});
    rotated = true;
    Update(ctx, base);
    assert(Append(verts, 12) == 6);
    CheckCenter(verts, {8, 25.20f, 30});

    auto record = [&](uint32_t entity, BloodMath::V position, int health, uint32_t location = 4) {
        ctx.r3.u32 = entity;
        Word(Stack + 84, location);
        Vector(Trace, position);
        const auto hit = BeforeDamage(ctx, base);
        assert(hit.handle);
        Word(entity + 776, health);
        AfterDamage(base, hit);
    };
    record(Entity, {7, 26, 30}, 80);
    record(Entity, {7, 26, 34}, 60, 5);
    poseReads = 0;
    Update(ctx, base);
    assert(poseReads == 2);
    assert(Append(verts, MaxVertices) == 18);
    assert(Append(verts, 16) == 12);
    CheckCenter(verts, {8, 25.20f, 30});
    CheckCenter(verts + 6, {7.20f, 26, 30});
    CheckCenter(verts + 12, {7.20f, 26, 34});
    assert((verts[0].mask >> 16) == BloodMask::WoundLayer + 1);

    record(Entity, {7, 26, 36}, 40, 5);
    Update(ctx, base);
    assert(Append(verts, MaxVertices) == 18);
    CheckCenter(verts, {7.20f, 26, 30});
    CheckCenter(verts + 6, {7.20f, 26, 34});
    CheckCenter(verts + 12, {7.20f, 26, 36});

    Word(HandleEntry + 8, SecondEntity);
    Word(HandleEntry + 12, 3);
    Word(SecondEntity + 500, SecondHandle);
    Word(SecondEntity + 508, Model);
    Word(SecondEntity + 532, Actor);
    Word(SecondEntity + 536, EnemySentient);
    Word(SecondEntity + 776, 100);
    record(SecondEntity, {9, 27, 35}, 85);
    Update(ctx, base);
    assert(Append(verts, MaxVertices) == 24);
    CheckCenter(verts + 18, {9.20f, 27, 35});

    Word(HandleEntry + 4, 8);
    Word(Entity + 500, (8u << 14) | 12);
    record(Entity, {7, 24, 32}, 20);
    Update(ctx, base);
    assert(Append(verts, MaxVertices) == 12); // Recycled slot starts empty.
    CheckCenter(verts, {7.20f, 24, 32});
    Word(Entity + 776, 0);
    Update(ctx, base);
    assert(Append(verts, MaxVertices) == 6);

    poseValid = false;
    Update(ctx, base);
    assert(Append(verts, MaxVertices) == 0); // Never draw a stale pose.
    ctx.r3.u32 = SecondEntity;
    assert(!BeforeDamage(ctx, base).handle);
    poseValid = true;
    Update(ctx, base);
    assert(Append(verts, MaxVertices) == 6);
    Word(SecondEntity + 508, Model + 0x1000);
    Update(ctx, base);
    assert(Append(verts, MaxVertices) == 0);
    record(SecondEntity, {9, 27, 35}, 70);
    Clear(); // The campaign reload and level transition hooks call this.
    Update(ctx, base);
    assert(Append(verts, MaxVertices) == 0);
    assert(VirtualFree(base, 0, MEM_RELEASE));
    std::puts("PASS: wound gates, three-hit limit, pose sharing, attachment, eviction, reuse and cleanup");
}
