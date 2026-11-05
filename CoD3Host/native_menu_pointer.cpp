#include "native_menu_pointer.h"
#include "menu_pointer.h"
#include "kernel.h"
#include "window.h"
#include "overlay.h"
#include <atomic>
#include <mutex>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <Windows.h>

#ifdef COD3_MP
bool NativeMenuPointer::Active() { return false; }
bool NativeMenuPointer::InMission() { return false; }
bool NativeMenuPointer::MenuVisible() { return false; }
void NativeMenuPointer::Button(bool,bool,int,int) {}
void NativeMenuPointer::Cancel() {}
#else
namespace {
constexpr uint32_t Manager=0x82A8DF2C, SinglePlayerVtable=0x82071EC4;
std::atomic<uint32_t> active{0};
struct Event { uint32_t menu; bool right,down; int x,y; };
std::mutex mutex;
std::vector<Event> events;
MenuPointer::Press left,right;
uint32_t previousMenu=0;
POINT previousPoint{};
bool havePoint=false;
bool Trace() { static bool on=getenv("COD3_MENUMOUSELOG")!=nullptr;return on; }
std::atomic<bool> cancelled{false};
bool Valid(uint32_t p) { return p>=0x10000&&p<0xFFFF0000u; }
float Float(uint8_t* base,uint32_t p) {
    uint32_t word=Guest::Read32(base,p);float value;memcpy(&value,&word,4);return value;
}
void Call(PPCContext& ctx,uint8_t* base,uint32_t fn,uint32_t object,uint32_t argument) {
    if(auto* target=Guest::Lookup(fn)) {
        PPCContext call=ctx;call.r1.u32=(ctx.r1.u32-0x2000)&~15u;
        call.r3.u64=object;call.r4.u64=argument;call.r5.u64=0;call.lr=0;target(call,base);
    }
}
bool Owns(uint8_t* base) {
    if(!base) return false;
    uint32_t system=Guest::Read32(base,Manager+28);
    if(!Valid(system)||!Guest::Read8(base,system+28)||Guest::Read32(base,system+12)!=2||Guest::Read32(base,system+20)<=2) return false;
    uint32_t array=Guest::Read32(base,system+4);
    if(!Valid(array)) return false;
    uint32_t menu=Guest::Read32(base,array+8);
    return Valid(menu)&&Guest::Read32(base,menu)==SinglePlayerVtable;
}
uint32_t Current(uint8_t* base) {
    if(!Guest::Read8(base,Manager+48)) return 0;
    uint32_t modal=Guest::Read32(base,Manager+200);
    if(Valid(modal)) {
        uint32_t index=Guest::Read32(base,modal+60);
        if(index>3||MenuPointer::ModalActive(Guest::Read32(base,modal+12+4*index))) return 0;
    }
    uint32_t system=Guest::Read32(base,Manager+28);
    if(!Valid(system)||!Guest::Read8(base,system+28)||Guest::Read32(base,system+12)!=2||Guest::Read32(base,system+20)<=2) return 0;
    uint32_t array=Guest::Read32(base,system+4);
    if(!Valid(array)) return 0;
    uint32_t menu=Guest::Read32(base,array+8);
    if(!Valid(menu)||Guest::Read32(base,menu)!=SinglePlayerVtable||Guest::Read8(base,menu+48)||Guest::Read8(base,menu+76)) return 0;
    return menu;
}
float Measure(PPCContext& ctx,uint8_t* base,uint32_t function,uint32_t text,uint32_t scale) {
    auto* target=Guest::Lookup(function);if(!target) return 0;
    PPCContext call=ctx;call.r1.u32=(ctx.r1.u32-0x2000)&~15u;
    call.r3.u64=text;call.r4.u64=scale;call.lr=0;target(call,base);return float(call.f1.f64);
}
int Hit(PPCContext& ctx,uint8_t* base,uint32_t menu,int x,int y) {
    RECT client{};
    if(!GetClientRect(static_cast<HWND>(Window::Handle()),&client)) return -1;
    MenuPointer::Point point;
    if(!MenuPointer::ToCanvas(client.right,client.bottom,x,y,640,480,point)) return -1;
    uint32_t array=Guest::Read32(base,menu+4);
    if(!Valid(array)||Guest::Read16(base,menu+38)!=3) return -1;
    for(int i=0;i<3;i++) {
        uint32_t child=Guest::Read32(base,array+4*i);
        if(!Valid(child)||Guest::Read32(base,child)!=0x82071034||Guest::Read8(base,child+22)) continue;
        uint32_t text=Guest::Read32(base,child+16);
        if(!Valid(text)||Guest::Read32(base,text)!=0x82070B54||!(Guest::Read8(base,text+16)&4)) continue;
        // Match plain-text draw 824CFD58 and alignment 824BF4A8. The
        // selected and unselected font scales may differ; use the drawn one.
        uint32_t scale=text+(Guest::Read8(base,child+21)?56:80);
        float w=Measure(ctx,base,0x824CFBE8,text,scale),h=Measure(ctx,base,0x824CFB48,text,scale);
        float left=Float(base,text+44),top=Float(base,text+48);
        uint16_t flags=Guest::Read16(base,text+108);
        left-=(flags&0x20)?w:((flags&0x10)?0:w*.5f);
        top-=(flags&0x80)?h:((flags&0x40)?0:h*.5f);
        if(MenuPointer::Contains({left,top,w,h},point)) return i;
    }
    return -1;
}
void Update(PPCContext& ctx,uint8_t* base) {
    uint32_t menu=Current(base);
    static const bool ignored=[] {
        const char* value=getenv("COD3_NOKEYBOARD");if(!value) value=getenv("COD3_BACKGROUND");
        return value&&*value&&*value!='0';
    }();
    bool enabled=menu&&Window::HasFocus()&&!Overlay::IsOpen()&&!ignored;
    active.store(enabled?menu:0);
    std::vector<Event> pending;
    { std::lock_guard lock(mutex);pending.swap(events); }
    if(cancelled.exchange(false)||!enabled||menu!=previousMenu) { left.Cancel();right.Cancel();havePoint=false;pending.clear(); }
    previousMenu=enabled?menu:0;
    if(!enabled) return;
    POINT point{};
    if(GetCursorPos(&point)&&ScreenToClient(static_cast<HWND>(Window::Handle()),&point)) {
        if(!havePoint||point.x!=previousPoint.x||point.y!=previousPoint.y) {
            int row=Hit(ctx,base,menu,point.x,point.y);
            if(row>=0&&int16_t(Guest::Read16(base,menu+34))!=row) {
                Call(ctx,base,0x824CE558,menu,row);
                if(Trace()) { printf("menu pointer: hover row %d at client %ld,%ld\n",row,point.x,point.y);fflush(stdout); }
            }
        }
        previousPoint=point;havePoint=true;
    }
    for(const auto& e:pending) {
        if(e.menu!=menu||Current(base)!=menu) { left.Cancel();right.Cancel();break; }
        int row=Hit(ctx,base,menu,e.x,e.y);
        if(e.right) {
            RECT client{};MenuPointer::Point canvas;
            bool inside=GetClientRect(static_cast<HWND>(Window::Handle()),&client)&&
                MenuPointer::ToCanvas(client.right,client.bottom,e.x,e.y,640,480,canvas);
            if(e.down) right.Down(inside?0:-1);
            else if(right.Up(inside?0:-1)) {
                if(Trace()) { printf("menu pointer: back\n");fflush(stdout); }
                Call(ctx,base,0x824C48B8,Guest::Read32(base,Manager+28),128);
            }
        } else if(e.down) left.Down(row);
        else if(left.Up(row)) {
            if(Trace()) { printf("menu pointer: activate row %d\n",row);fflush(stdout); }
            Call(ctx,base,0x824CE558,menu,row);
            Call(ctx,base,0x824C48B8,Guest::Read32(base,Manager+28),64);
        }
    }
}
}
bool NativeMenuPointer::Active() { return Owns(Guest::Base); }
bool NativeMenuPointer::InMission() {
    auto* base=Guest::Base;if(!base) return false;
    // Native usercmd production requires connection state 2. Confirm that
    // the local soldier and its playerstate exist, as the co-op bridge does.
    uint32_t entity=Guest::Read32(base,0x82A2DCD8);
    return Guest::Read32(base,0x829C3F08)==2&&entity>=0x40000000u&&entity<0xC0000000u&&Valid(Guest::Read32(base,entity+528));
}
bool NativeMenuPointer::MenuVisible() {
    auto* base=Guest::Base;if(!base) return true;
    uint32_t frontend=Guest::Read32(base,Manager+28),pause=Guest::Read32(base,Manager+32);
    uint32_t local=Guest::Read32(base,0x82A2A1C8);
    if(Valid(frontend)&&Guest::Read8(base,frontend+28)) return true;
    if(local>3) return true;
    if(Valid(pause)&&Guest::Read8(base,pause+28+local)) return true;
    uint32_t modal=Guest::Read32(base,Manager+200+4*local);
    if(Valid(modal)) { uint32_t index=Guest::Read32(base,modal+60);if(index>3||MenuPointer::ModalActive(Guest::Read32(base,modal+12+4*index))) return true; }
    return false;
}
void NativeMenuPointer::Button(bool right,bool down,int x,int y) {
    uint32_t menu=active.load();if(!menu) return;
    std::lock_guard lock(mutex);
    if(events.size()<64) events.push_back({menu,right,down,x,y});
}
void NativeMenuPointer::Cancel() {
    active.store(0);cancelled.store(true);std::lock_guard lock(mutex);events.clear();
}
// Keep queued presses from surviving frontend closure (the frontend update
// itself is no longer called once gameplay takes over).
extern "C" PPC_FUNC(__imp__sub_82511168);
PPC_FUNC(sub_82511168) {
    __imp__sub_82511168(ctx,base);
    if(!Current(base)) {
        NativeMenuPointer::Cancel();left.Cancel();right.Cancel();previousMenu=0;havePoint=false;
    }
}
extern "C" PPC_FUNC(__imp__sub_824CF060);
PPC_FUNC(sub_824CF060) {
    __imp__sub_824CF060(ctx,base);
    Update(ctx,base);
}
#endif
