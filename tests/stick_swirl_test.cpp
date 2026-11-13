#include "stick_swirl.h"
#include <map>
#include <stdexcept>
#include <cstdio>
int main() {
    std::map<uint32_t,uint32_t> m;
    int checks=0;
    auto active=[&] { return StickSwirl::Active(0x1000,[&](uint32_t a) { return m.at(a); }); };
    auto expect=[&](bool value) { ++checks; if(active()!=value) throw std::runtime_error("wrong interaction ownership"); };
    m[0x1004]=0; expect(false); // receiver cache existence must not activate routing
    m[0x1004]=0x2000; m[0x2020]=0; m[0x201c]=0x4000; expect(false);
    m[0x2020]=0x3000; m[0x3000]=0x8206C9F0; m[0x3004]=2; m[0x3010]=0x4000; m[0x4444]=0; expect(true);
    m[0x3000]=0x8206C9D4; expect(false); // another input receiver
    m[0x3000]=0x8206C9F0; m[0x3004]=1; expect(false);
    m[0x3004]=2; m[0x4444]=1; expect(false); // left-stick interaction
    m[0x4444]=0; m[0x3010]=0x5000; expect(false); // cached definition from another state
    m[0x3010]=0x4000; expect(true);
    m[0x1004]=0; expect(false); // teardown, no stale receiver dereference
    m[0x1004]=0x2000; expect(true); // consecutive restart
    m[0x201c]=0; expect(false);
    printf("PASS: %d StickSwirl ownership and lifecycle snapshot checks\n",checks);
}
