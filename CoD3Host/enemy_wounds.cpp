#ifndef COD3_MP
#include "enemy_wounds.h"
#include "blood_diag.h"
#include "blood_mask.h"
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
    constexpr float SurfaceOffset = 0.20f;

    struct Pose
    {
        V x, y, z, origin;
        V LocalVector(V v) const { return {Dot(v, x), Dot(v, y), Dot(v, z)}; }
        V WorldVector(V v) const { return x * v.x + y * v.y + z * v.z; }
    };

    struct Wound
    {
        uint32_t tag = 0;
        V point, localRight, localUp;
        V center, right, up;
        bool visible = false;
    };

    struct Enemy
    {
        uint32_t handle = 0, model = 0, actor = 0;
        std::array<Wound, WoundsPerEnemy> wounds{};
        unsigned next = 0, count = 0;
    };

    std::mutex mutex;
    std::array<Enemy, MaxEnemies> enemies;

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
    uint32_t ResolveOwner(const uint8_t* base, uint32_t handle, uint32_t model, uint32_t actor)
    {
        const uint32_t slot = handle & 0x3FFF;
        if (!handle || slot >= 4096) return 0;
        const uint32_t entry = EntityHandles + slot * 8;
        if (Guest::Read32(base, entry + 4) != handle >> 14) return 0;
        const uint32_t entity = Guest::Read32(base, entry);
        return Pointer(entity) && Guest::Read32(base, entity + 500) == handle &&
               Guest::Read32(base, entity + 508) == model &&
               Guest::Read32(base, entity + 532) == actor ? entity : 0;
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
        if (!call.r3.u32 || !Finite(pose.origin) || !validAxis(pose.x) ||
            !validAxis(pose.y) || !validAxis(pose.z) ||
            std::abs(Dot(pose.x, pose.y)) >= 0.1f ||
            std::abs(Dot(pose.x, pose.z)) >= 0.1f ||
            std::abs(Dot(pose.y, pose.z)) >= 0.1f)
            return false;
        // Use the same rigid basis for capture and playback. Its transpose is
        // the inverse even when the native animation matrix has slight skew.
        pose.x = Unit(pose.x);
        pose.y = Unit(pose.y - pose.x * Dot(pose.y, pose.x));
        const V z = Cross(pose.x, pose.y);
        pose.z = Dot(z, pose.z) < 0 ? z * -1.0f : z;
        return true;
    }

    Wound MakeWound(const Hit& hit)
    {
        const V n = hit.normal;
        const V tangent = Unit(Cross(n, std::abs(n.z) < 0.9f ? V{0, 0, 1} : V{0, 1, 0}));
        const V bitangent = Cross(n, tangent);
        uint32_t seed = hit.handle ^ std::bit_cast<uint32_t>(hit.point.x);
        seed = std::rotl(seed, 11) ^ std::bit_cast<uint32_t>(hit.point.y);
        seed = std::rotl(seed, 11) ^ std::bit_cast<uint32_t>(hit.point.z);
        seed = (seed ^ (seed >> 16)) * 0x7feb352du;
        const float angle = (float(seed & 255) / 255.0f - 0.5f) * 1.8f;
        const float scale = 1.5f * (0.87f + float((seed >> 8) & 255) / 255.0f * 0.26f);
        const float c = std::cos(angle), s = std::sin(angle);
        return {hit.tag, hit.point + n * SurfaceOffset,
                (tangent * c + bitangent * s) * (1.85f * scale),
                (bitangent * c - tangent * s) * (1.65f * scale)};
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
    if (!Pointer(hit.model) || !Pointer(hit.actor) || hit.health <= 0 ||
        ResolveOwner(base, hit.handle, hit.model, hit.actor) != entity)
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
    const uint32_t entity = ResolveOwner(base, hit.handle, hit.model, hit.actor);
    if (!entity) return;
    const int health = int32_t(Guest::Read32(base, entity + 776));
    if (health <= 0 || health >= hit.health) return;
    std::lock_guard lock(mutex);
    const auto existing = std::find_if(enemies.begin(), enemies.end(),
        [&hit](const Enemy& owner) { return (owner.handle & 0x3FFF) == (hit.handle & 0x3FFF) && owner.handle; });
    const auto slot = existing != enemies.end() ? existing : std::find_if(enemies.begin(), enemies.end(),
        [](const Enemy& owner) { return owner.handle == 0; });
    if (slot == enemies.end()) return;
    if (slot->handle != hit.handle || slot->model != hit.model || slot->actor != hit.actor)
        *slot = {hit.handle, hit.model, hit.actor};
    slot->wounds[slot->next] = MakeWound(hit);
    slot->next = (slot->next + 1) % WoundsPerEnemy;
    slot->count = std::min<unsigned>(slot->count + 1, WoundsPerEnemy);
    BloodDiag::Log("enemy wound attached handle=%08X health=%d->%d wounds=%u local=%.2f,%.2f,%.2f",
                   hit.handle, hit.health, health, slot->count, hit.point.x, hit.point.y, hit.point.z);
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
    for (auto& owner : enemies)
    {
        if (!owner.handle) continue;
        const uint32_t entity = ResolveOwner(base, owner.handle, owner.model, owner.actor);
        if (!entity || int32_t(Guest::Read32(base, entity + 776)) <= 0)
        {
            owner = {};
            continue;
        }

        // An enemy's wounds share at most two torso transforms. Keep local anchors
        // when a pose is temporarily unavailable, but draw no stale geometry.
        struct TagPose { uint32_t tag; Pose pose; bool valid; };
        std::array<TagPose, 2> poses{};
        for (unsigned i = 0; i < owner.count; ++i)
        {
            auto& wound = owner.wounds[i];
            auto& cached = poses[poses[0].tag && poses[0].tag != wound.tag ? 1 : 0];
            if (cached.tag != wound.tag)
            {
                cached.tag = wound.tag;
                cached.valid = ReadPose(ctx, base, entity, wound.tag, cached.pose);
            }
            wound.visible = cached.valid;
            if (!cached.valid) continue;
            const Pose& pose = cached.pose;
            wound.center = pose.origin + pose.WorldVector(wound.point);
            wound.right = pose.WorldVector(wound.localRight);
            wound.up = pose.WorldVector(wound.localUp);
        }
    }
}

void Clear()
{
    std::lock_guard lock(mutex);
    enemies = {};
}

int Append(Blood::WorldVertex* output, int capacity)
{
    if (!Settings::EnemyWoundsEnabled() || Overlay::IsOpen()) return 0;
    std::lock_guard lock(mutex);
    constexpr uint32_t color = 0xEB0C155Bu;
    constexpr unsigned mask = (BloodMask::WoundLayer + 1u) << 16;
    constexpr uint32_t uv[] = {mask, mask | 255, mask | 65535, mask | (255 << 8)};
    constexpr int indices[] = {0, 1, 2, 0, 2, 3};
    int count = 0;
    for (const auto& owner : enemies)
    {
        for (unsigned i = 0; i < owner.count; ++i)
        {
            if (capacity - count < 6) return count;
            const auto& wound = owner.wounds[(owner.next + WoundsPerEnemy - owner.count + i) % WoundsPerEnemy];
            if (!wound.visible) continue;
            const V p = wound.center, u = wound.right, v = wound.up;
            const V corners[] = {p - u - v, p + u - v, p + u + v, p - u + v};
            for (const int corner : indices)
                output[count++] = {corners[corner].x, corners[corner].y, corners[corner].z, color, uv[corner]};
        }
    }
    return count;
}
}
#endif
