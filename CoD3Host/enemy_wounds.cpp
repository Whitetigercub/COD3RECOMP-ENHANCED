#ifndef COD3_MP
#include "enemy_wounds.h"
#include "blood_diag.h"
#include "coop.h"
#include "kernel.h"
#include "overlay.h"
#include "settings.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <mutex>

namespace EnemyWounds
{
namespace
{
    using namespace BloodMath;
    constexpr uint32_t BulletDamageReturn = 0x82565D14;
    constexpr uint32_t EntityHandles = 0x82A2DCD0;
    constexpr uint32_t Clients = 0x82A2AB40;
    constexpr uint32_t UpperSpineName = 0x8206921C, LowerSpineName = 0x8206922C;
    constexpr uint32_t InternString = 0x820CEBB8, WorldTagMatrix = 0x8254EC98;
    constexpr uint32_t TraceNormal = 16;

    struct Pose
    {
        V x, y, z, origin;
        V LocalVector(V v) const { return {Dot(v, x), Dot(v, y), Dot(v, z)}; }
        V WorldVector(V v) const { return x * v.x + y * v.y + z * v.z; }
    };

    struct Wound
    {
        Hit hit;
        V center, right, up;
        bool visible = false;
    };

    std::mutex mutex;
    std::array<Wound, MaxVertices / 6> wounds;

    bool Pointer(uint32_t address) { return address >= 0x40000000u && address < 0xBFFFF800u; }
    V ReadVector(const uint8_t* base, uint32_t address)
    {
        return {
            std::bit_cast<float>(Guest::Read32(base, address)),
            std::bit_cast<float>(Guest::Read32(base, address + 4)),
            std::bit_cast<float>(Guest::Read32(base, address + 8))
        };
    }

    // Native handles carry the slot in the low 14 bits and a generation above it.
    uint32_t ResolveOwner(const uint8_t* base, const Hit& hit)
    {
        const uint32_t slot = hit.handle & 0x3FFF;
        if (!hit.handle || slot >= 4096) return 0;
        const uint32_t entry = EntityHandles + slot * 8;
        if (Guest::Read32(base, entry + 4) != hit.handle >> 14) return 0;
        const uint32_t entity = Guest::Read32(base, entry);
        return Pointer(entity) && Guest::Read32(base, entity + 500) == hit.handle &&
               Guest::Read32(base, entity + 508) == hit.model &&
               Guest::Read32(base, entity + 532) == hit.actor ? entity : 0;
    }

    uint32_t Team(const uint8_t* base, uint32_t entity)
    {
        if (!Pointer(entity)) return 0;
        const uint32_t sentient = Guest::Read32(base, entity + 536);
        return Pointer(sentient) ? Guest::Read32(base, sentient + 4) : 0;
    }

    uint32_t Scratch()
    {
        // Used on the game's thread, including the native damage callback.
        static uint32_t buffer = 0;
        if (!buffer) buffer = Guest::AllocatePhysical(0x10000);
        return buffer;
    }

    uint32_t TorsoTag(PPCContext& ctx, uint8_t* base, bool upper)
    {
        static uint32_t tags[2]{};
        uint32_t& tag = tags[upper ? 0 : 1];
        if (tag) return tag;
        const uint32_t memory = Scratch();
        const auto intern = Guest::Lookup(InternString);
        if (!memory || !intern) return 0;
        PPCContext call = ctx;
        call.r1.u32 = memory + 0xE000;
        call.lr = 0;
        call.r3.u32 = upper ? UpperSpineName : LowerSpineName;
        intern(call, base);
        return tag = call.r3.u32;
    }

    bool ReadPose(PPCContext& ctx, uint8_t* base, uint32_t owner, uint32_t tag, Pose& pose)
    {
        const uint32_t memory = Scratch();
        const auto transform = Guest::Lookup(WorldTagMatrix);
        if (!memory || !transform || !tag) return false;
        const uint32_t result = memory + 0xF000;
        // The native wrapper can return success after finding the tag even if
        // the pose was unavailable. Require every output vector to be valid.
        for (uint32_t offset = 0; offset < 64; offset += 4)
            Guest::Write32(base, result + offset, 0x7FC00000);
        PPCContext call = ctx;
        call.r1.u32 = memory + 0xE000;
        call.lr = 0;
        call.r3.u32 = owner;
        call.r4.u32 = tag;
        call.r5.u32 = result;
        transform(call, base);
        pose = {ReadVector(base, result), ReadVector(base, result + 16),
                ReadVector(base, result + 32), ReadVector(base, result + 48)};
        const auto validAxis = [](V v) { return Finite(v) && std::abs(Dot(v, v) - 1.0f) < 0.12f; };
        return call.r3.u32 && Finite(pose.origin) && validAxis(pose.x) &&
               validAxis(pose.y) && validAxis(pose.z) &&
               std::abs(Dot(pose.x, pose.y)) < 0.1f &&
               std::abs(Dot(pose.x, pose.z)) < 0.1f &&
               std::abs(Dot(pose.y, pose.z)) < 0.1f;
    }
}

Hit BeforeDamage(PPCContext& ctx, uint8_t* base)
{
    if (ctx.lr != BulletDamageReturn || !Settings::EnemyWoundsEnabled() ||
        Coop::GetMode() != Coop::Mode::Off || ctx.r8.s32 <= 0 ||
        (ctx.r10.u32 != 1 && ctx.r10.u32 != 2)) return {};

    const uint32_t entity = ctx.r3.u32, trace = ctx.r7.u32;
    const uint32_t location = Guest::Read32(base, ctx.r1.u32 + 84);
    if ((location != 4 && location != 5) || !Pointer(entity) || !Pointer(trace)) return {};

    const uint32_t clients = Guest::Read32(base, Clients);
    const uint32_t player = Pointer(clients) ? Guest::Read32(base, clients + 4) : 0;
    const uint32_t enemyTeam = Team(base, entity), playerTeam = Team(base, player);
    if (!enemyTeam || !playerTeam || enemyTeam == playerTeam) return {};

    Hit hit;
    hit.handle = Guest::Read32(base, entity + 500);
    hit.model = Guest::Read32(base, entity + 508);
    hit.actor = Guest::Read32(base, entity + 532);
    hit.health = int32_t(Guest::Read32(base, entity + 776));
    if (!Pointer(hit.model) || !Pointer(hit.actor) || hit.health <= 0 || ResolveOwner(base, hit) != entity)
        return {};

    const V position = ReadVector(base, trace), normal = ReadVector(base, trace + TraceNormal);
    const float length = Dot(normal, normal);
    if (!Finite(position) || !Finite(normal) || length < 0.8f || length > 1.2f) return {};

    hit.tag = TorsoTag(ctx, base, location == 4);
    Pose pose;
    if (!ReadPose(ctx, base, entity, hit.tag, pose)) return {};
    const V offset = position - pose.origin;
    if (Dot(offset, offset) > 22.0f * 22.0f) return {};

    hit.point = pose.LocalVector(offset);
    hit.normal = Unit(pose.LocalVector(normal));
    return hit;
}

void AfterDamage(uint8_t* base, const Hit& hit)
{
    if (!hit.handle) return;
    const uint32_t entity = ResolveOwner(base, hit);
    if (!entity) return;
    const int health = int32_t(Guest::Read32(base, entity + 776));
    if (health <= 0 || health >= hit.health) return;
    std::lock_guard lock(mutex);
    const auto existing = std::find_if(wounds.begin(), wounds.end(),
        [&hit](const Wound& wound) { return wound.hit.handle == hit.handle; });
    const auto slot = existing != wounds.end() ? existing : std::find_if(wounds.begin(), wounds.end(),
        [](const Wound& wound) { return wound.hit.handle == 0; });
    if (slot != wounds.end())
    {
        *slot = {hit};
        BloodDiag::Log("enemy wound attached handle=%08X health=%d->%d", hit.handle, hit.health, health);
    }
}

void Update(PPCContext& ctx, uint8_t* base)
{
    static bool wasEnabled = false;
    const bool enabled = Settings::EnemyWoundsEnabled();
    if (!enabled)
    {
        if (wasEnabled) Clear();
        wasEnabled = false;
        return;
    }
    wasEnabled = true;
    std::lock_guard lock(mutex);
    for (auto& wound : wounds)
    {
        const Hit& hit = wound.hit;
        if (!hit.handle) continue;
        const uint32_t entity = ResolveOwner(base, hit);
        Pose pose;
        if (!entity || int32_t(Guest::Read32(base, entity + 776)) <= 0 ||
            !ReadPose(ctx, base, entity, hit.tag, pose))
        {
            wound = {};
            continue;
        }
        const V localNormal = hit.normal;
        const V tangent = Unit(Cross(localNormal, std::abs(localNormal.z) < 0.9f ? V{0, 0, 1} : V{0, 1, 0}));
        wound.center = pose.origin + pose.WorldVector(hit.point + localNormal * 0.24f);
        wound.right = pose.WorldVector(tangent) * 1.9f;
        wound.up = pose.WorldVector(Cross(localNormal, tangent)) * 1.65f;
        wound.visible = true;
    }
}

void Clear()
{
    std::lock_guard lock(mutex);
    wounds = {};
}

int Append(Blood::WorldVertex* output, int capacity)
{
    if (!Settings::EnemyWoundsEnabled() || Overlay::IsOpen()) return 0;
    std::lock_guard lock(mutex);
    constexpr uint32_t color = 0xE0040758u;
    constexpr unsigned mask = 1u << 16;
    constexpr uint32_t uv[] = {mask, mask | 255, mask | 65535, mask | (255 << 8)};
    constexpr int indices[] = {0, 1, 2, 0, 2, 3};
    int count = 0;
    for (const auto& wound : wounds)
    {
        if (capacity - count < 6) break;
        if (!wound.visible) continue;
        const V p = wound.center, u = wound.right, v = wound.up;
        const V corners[] = {p - u - v, p + u - v, p + u + v, p - u + v};
        for (const int i : indices)
            output[count++] = {corners[i].x, corners[i].y, corners[i].z, color, uv[i]};
    }
    return count;
}
}
#endif
