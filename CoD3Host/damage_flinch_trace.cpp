#include "kernel.h"
#include "coop.h"
#include "settings.h"
#include <bit>
#include <array>
#include <cstdio>
#include <cstdlib>

namespace {
bool Enabled()
{
    static const bool enabled = std::getenv("COD3_DAMAGE_FLINCH_TRACE") != nullptr;
    return enabled;
}

uint32_t Clock(const uint8_t* base) { return Guest::Read32(base, 0x82A95244); }
int32_t Int(const uint8_t* base, uint32_t address)
{
    return std::bit_cast<int32_t>(Guest::Read32(base, address));
}
float Float(const uint8_t* base, uint32_t address)
{
    return std::bit_cast<float>(Guest::Read32(base, address));
}

uint32_t View(const uint8_t* base)
{
    const uint32_t index = Guest::Read32(base, 0x82A2A1C8);
    return index < 2 ? 0x82A98D90 + index * 5840 : 0;
}

struct Feedback
{
    uint32_t sequence = 0;
    std::array<uint32_t, 3> values{};
};

// Restrict attribution to the local solo player. Track exact guest feedback
// records on the title thread and discard any scripted or unknown contribution.
thread_local uint32_t playerHandle = 0;
thread_local uint32_t playerRecord = 0;
thread_local int32_t trackedDamage = 0;
thread_local std::array<uint32_t, 4> trackedDirection;
thread_local Feedback pendingKick;

std::array<uint32_t, 4> Direction(const uint8_t* base, uint32_t ps)
{
    return { Guest::Read32(base, ps + 1528), Guest::Read32(base, ps + 1532),
             Guest::Read32(base, ps + 1536), Guest::Read32(base, ps + 1540) };
}

uint32_t LocalPlayer(const uint8_t* base)
{
    const uint32_t clients = Guest::Read32(base, 0x82A2AB40);
    return clients ? Guest::Read32(base, clients + 4) : 0;
}

bool SamePlayer(const uint8_t* base, uint32_t entity, uint32_t ps)
{
    return ps && ps == playerRecord && entity && entity == LocalPlayer(base) &&
           Guest::Read32(base, entity + 500) == playerHandle && Coop::FromRef(base, playerHandle) == entity;
}

#ifdef COD3_DAMAGE_FLINCH_REPLAY
// Controlled attribution replay: exercise the real script producer once,
// either before the first hit or after an actual hit but before consumption.
void ExerciseScriptProducer(PPCContext& ctx, uint8_t* base, uint32_t entity, uint32_t ps, int acc);
#endif
}

extern "C" PPC_FUNC(__imp__sub_820A0968);
PPC_FUNC(sub_820A0968)
{
    if (ctx.lr == 0x82562EFC && ctx.r30.u32) {
        const uint32_t ps = ctx.r30.u32;
        const uint32_t entity = ctx.r31.u32;
        const int32_t acc = Int(base, ps + 1524), damage = ctx.r27.s32;
        if (entity == LocalPlayer(base) && Coop::GetMode() == Coop::Mode::Off) {
            const bool continued = SamePlayer(base, entity, ps) && int64_t(acc) - damage == trackedDamage;
            playerHandle = Guest::Read32(base, entity + 500);
            playerRecord = ps;
            trackedDamage = damage > 0 && (acc == damage || continued) ? acc : 0;
            trackedDirection = Direction(base, ps);
        }
        if (Enabled()) std::printf("FLINCH damage t=%u ps=%08x entity=%08x applied=%d health_after_store=%d "
                    "acc=%d type=%u dir=%.5f,%.5f,%.5f\n",
                    Clock(base), ps, entity, damage,
                    Int(base, entity + 776), acc, ctx.r24.u32,
                    Float(base, ps + 1528), Float(base, ps + 1532), Float(base, ps + 1536));
    }
    __imp__sub_820A0968(ctx, base);
}

extern "C" PPC_FUNC(__imp__sub_8249E8D0);
PPC_FUNC(sub_8249E8D0)
{
    const uint32_t handle = ctx.r3.u32;
    if (handle == playerHandle) trackedDamage = 0;
    if (Enabled()) {
        std::printf("FLINCH script t=%u handle=%08x magnitude_arg=%d\n",
                    Clock(base), handle, ctx.r4.s32);
    }
    __imp__sub_8249E8D0(ctx, base);
}

#ifdef COD3_DAMAGE_FLINCH_REPLAY
namespace {
void ExerciseScriptProducer(PPCContext& ctx, uint8_t* base, uint32_t entity, uint32_t ps, int acc)
{
    static const char* const scenario = std::getenv("COD3_FLINCH_SCRIPT_TEST");
    static bool fired = false;
    if (!scenario || fired || !ps || Int(base, entity + 776) <= 0 || Clock(base) < 5000) return;
    const bool overlap = scenario[0] == 'm';
    if (overlap != (acc > 0)) return;
    const uint32_t handle = Guest::Read32(base, entity + 500);
    const uint32_t slot = handle & 0x3fff;
    if (!handle || slot >= 4096) return;
    const uint32_t entry = 0x82A2DCD0 + slot * 8;
    if (Guest::Read32(base, entry) != entity || Guest::Read32(base, entry + 4) != handle >> 14) return;
    fired = true;
    PPCContext script = Coop::CallCtx(ctx);
    script.r3.u64 = handle;
    script.r4.u64 = 10;
    script.r5.u64 = entity + 336;
    sub_8249E8D0(script, base);
    std::printf("FLINCH replay t=%u mode=%s ps=%08x acc=%d->%d health=%d\n",
                Clock(base), scenario, ps, acc, Int(base, ps + 1524), Int(base, entity + 776));
}
}
#endif

extern "C" PPC_FUNC(__imp__sub_8254A2A0);
PPC_FUNC(sub_8254A2A0)
{
    const uint32_t entity = ctx.r3.u32;
    const uint32_t ps = entity ? Guest::Read32(base, entity + 528) : 0;
#ifdef COD3_DAMAGE_FLINCH_REPLAY
    if (Enabled()) ExerciseScriptProducer(ctx, base, entity, ps, ps ? Int(base, ps + 1524) : 0);
#endif
    const int32_t acc = ps ? Int(base, ps + 1524) : 0;
    const uint32_t seq = ps ? Guest::Read32(base, ps + 252) : 0;
    const bool local = entity && entity == LocalPlayer(base);
    const bool attributable = local && SamePlayer(base, entity, ps) && acc > 0 && acc == trackedDamage &&
                              Direction(base, ps) == trackedDirection;
    if (local) trackedDamage = 0;
    __imp__sub_8254A2A0(ctx, base);
    if (acc > 0 && Guest::Read32(base, ps + 252) != seq) {
        const Feedback event{Guest::Read32(base, ps + 252),
                             {Guest::Read32(base, ps + 256), Guest::Read32(base, ps + 260),
                              Guest::Read32(base, ps + 264)}};
        if (local) pendingKick = attributable ? event : Feedback{};
        if (Enabled()) std::printf("FLINCH consume t=%u ps=%08x acc=%d->%d seq=%u->%u "
                    "event=%u,%u,%u health=%d attributed=%d\n",
                    Clock(base), ps, acc, Int(base, ps + 1524), seq, event.sequence,
                    event.values[0], event.values[1], event.values[2], Int(base, entity + 776), attributable);
    }
}

extern "C" PPC_FUNC(__imp__sub_82571678);
PPC_FUNC(sub_82571678)
{
    playerHandle = playerRecord = 0;
    trackedDamage = 0;
    pendingKick = {};
    __imp__sub_82571678(ctx, base);
}

extern "C" PPC_FUNC(__imp__sub_8243C390);
PPC_FUNC(sub_8243C390)
{
    const uint32_t pitch = ctx.r3.u32, yaw = ctx.r4.u32, magnitude = ctx.r5.u32;
    const uint32_t caller = uint32_t(ctx.lr);
    const uint32_t view = View(base);
    const uint32_t seq = view && caller == 0x8244B064 ? Guest::Read32(base, view + 3068) : 0;
    __imp__sub_8243C390(ctx, base);
    if (!view) return;
    const float originalPitch = Float(base, view + 5420), originalRoll = Float(base, view + 5424);
    float gain = 1.0f;
    if (caller == 0x8244B064 && pendingKick.sequence && seq == pendingKick.sequence &&
        Guest::Read32(base, 0x82A2A1C8) == 0 && Coop::GetMode() == Coop::Mode::Off &&
        pendingKick.values == std::array{pitch, yaw, magnitude} && SamePlayer(base, LocalPlayer(base), playerRecord)) {
        pendingKick = {};
        const int mode = Settings::DamageFlinch();
        gain = mode == 1 ? 0.5f : mode == 2 ? 0.25f : 1.0f;
        if (gain != 1.0f) {
            Guest::Write32(base, view + 5420, std::bit_cast<uint32_t>(originalPitch * gain));
            Guest::Write32(base, view + 5424, std::bit_cast<uint32_t>(originalRoll * gain));
        }
    }
    if (Enabled()) {
        std::printf("FLINCH kick t=%u caller=%08x seq=%u event=%u,%u,%u start=%u gain=%.2f "
                    "original=%.6f,%.6f amp=%.6f,%.6f\n", Clock(base), caller, seq, pitch, yaw,
                    magnitude, Guest::Read32(base, view + 5388), gain, originalPitch, originalRoll,
                    Float(base, view + 5420), Float(base, view + 5424));
    }
}

extern "C" PPC_FUNC(__imp__sub_8243EDB8);
PPC_FUNC(sub_8243EDB8)
{
    if (!Enabled()) { __imp__sub_8243EDB8(ctx, base); return; }
    const uint32_t view = View(base);
    const uint32_t start = view ? Guest::Read32(base, view + 5388) : 0;
    const uint32_t now = Clock(base);
    const bool active = start && uint32_t(now - start) < 500;
    const float pitch = active ? Float(base, view + 4544) : 0.0f;
    const float roll = active ? Float(base, view + 4552) : 0.0f;
    __imp__sub_8243EDB8(ctx, base);
    if (active) {
        static thread_local uint32_t previous = 0;
        if (previous == now) return;
        previous = now;
        std::printf("FLINCH camera t=%u start=%u pitch=%.6f->%.6f roll=%.6f->%.6f\n",
                    now, start, pitch, Float(base, view + 4544), roll, Float(base, view + 4552));
    }
}

extern "C" PPC_FUNC(__imp__sub_8254D930);
PPC_FUNC(sub_8254D930)
{
    const uint32_t entity = ctx.r3.u32, output = ctx.r4.u32;
    const bool shot = Enabled() && ctx.lr == 0x82569CBC;
    __imp__sub_8254D930(ctx, base);
    if (shot) {
        const uint32_t ps = Guest::Read32(base, entity + 528);
        if (!ps) return;
        std::printf("FLINCH shot t=%u health=%d ads=%.6f aim=%.6f,%.6f forward=%.6f,%.6f,%.6f\n",
                    Clock(base), Int(base, entity + 776), Float(base, ps + 140),
                    Float(base, ps + 1504), Float(base, ps + 1508),
                    Float(base, output), Float(base, output + 4), Float(base, output + 8));
    }
}
