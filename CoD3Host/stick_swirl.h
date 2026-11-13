#ifndef COD3_MP
#pragma once
#include <cstdint>
namespace StickSwirl {
// SP v0.5.851: RTTI + receiver factory identify this vtable/type pair.
// Call on the guest thread while the interaction controller is alive.
template<class Read> bool Active(uint32_t controller, Read read) {
    if (!controller) return false;
    const auto state = read(controller + 4);
    if (!state) return false;
    const auto receiver = read(state + 32), definition = read(state + 28);
    return receiver && definition && read(receiver) == 0x8206C9F0 &&
        read(receiver + 4) == 2 && read(receiver + 16) == definition &&
        read(definition + 0x444) == 0;
}
}

#endif

