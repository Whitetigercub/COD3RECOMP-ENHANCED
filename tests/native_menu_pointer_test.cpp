// Exercises the actual host pointer module with guest memory and callbacks
// supplied by this harness. No game code, UI input or window is run.
#include <Windows.h>
#include "../CoD3Host/kernel.h"
#include <cassert>
#include <cstdio>
static POINT cursor{};
static bool focused=true,overlay=false,held=false;
static BOOL TestClient(HWND,LPRECT r) { *r={0,0,1280,720};return TRUE; }
static BOOL TestCursor(LPPOINT p) { *p=cursor;return TRUE; }
static BOOL TestScreen(HWND,LPPOINT) { return TRUE; }
static SHORT TestKey(int) { return held?SHORT(0x8000):0; }
#define GetClientRect TestClient
#define GetCursorPos TestCursor
#define ScreenToClient TestScreen
#define GetAsyncKeyState TestKey
#include "../CoD3Host/native_menu_pointer.cpp"
#undef GetClientRect
#undef GetCursorPos
#undef ScreenToClient
#undef GetAsyncKeyState
namespace Guest { uint8_t* Base=nullptr; PPCFunc* LookupModule(uint32_t) { return nullptr; } }
namespace Window { bool HasFocus() { return focused; } void* Handle() { return reinterpret_cast<void*>(1); } }
namespace Overlay { bool IsOpen() { return overlay; } }
PPC_FUNC_IMPL(__imp__sub_82511168) {}
PPC_FUNC_IMPL(__imp__sub_824CDB28) {}
static uint32_t next=0x40010000;
static int confirms=0,backs=0,steps=0;
static uint32_t lastAction=0;
static uint32_t lastSystem=0,lastLocal=0,sliderValueAddress=0;
static void Commit(uint64_t offset,size_t size) {
    uint64_t start=offset&~uint64_t(4095),end=(offset+size+4095)&~uint64_t(4095);
    assert(VirtualAlloc(Guest::Base+start,size_t(end-start),MEM_COMMIT,PAGE_READWRITE));
}
static uint32_t Alloc() { uint32_t address=next;next+=0x1000;Commit(address,0x1000);return address; }
static void W(uint32_t p,uint32_t value) { Commit(p,4);Guest::Write32(Guest::Base,p,value); }
static void H(uint32_t p,uint16_t value) { Commit(p,2);Guest::Write16(Guest::Base,p,value); }
static void B(uint32_t p,uint8_t value) { Commit(p,1);Guest::Write8(Guest::Base,p,value); }
static void F(uint32_t p,float value) { uint32_t word;memcpy(&word,&value,4);W(p,word); }
static void Bind(uint32_t address,PPCFunc* fn) {
    uint64_t p=Guest::FuncTableBase+uint64_t(address-PPC_CODE_BASE)*2;Commit(p,8);
    *reinterpret_cast<PPCFunc**>(Guest::Base+p)=fn;
}
PPC_FUNC_IMPL(FakeSelect) { H(ctx.r3.u32+34,uint16_t(ctx.r4.u32)); }
PPC_FUNC_IMPL(FakeModalSelect) {
    auto p=ctx.r3.u32;W(p+60,MenuPointer::ModalSlot(Guest::Read32(base,p+56),ctx.r4.u32,Guest::Read32(base,p+28)&&Guest::Read32(base,p+32)));
}
PPC_FUNC_IMPL(FakeWidth) { ctx.f1.f64=Guest::Read32(base,ctx.r3.u32+0x200); }
PPC_FUNC_IMPL(FakeHeight) { ctx.f1.f64=12; }
PPC_FUNC_IMPL(FakeRefresh) {}
PPC_FUNC_IMPL(FakeDispatch) {
    lastSystem=ctx.r3.u32;lastLocal=ctx.r5.u32;lastAction=ctx.r4.u32;
    if(ctx.r4.u32==64) ++confirms;
    else if(ctx.r4.u32==128) ++backs;
    else { assert(ctx.r4.u32==16||ctx.r4.u32==32);++steps;
        if(sliderValueAddress) W(sliderValueAddress,uint32_t(int(Guest::Read32(base,sliderValueAddress))+(ctx.r4.u32==32?1:-1)));
    }
}
static uint32_t Label(float x,float y) {
    auto p=Alloc();W(p,0x82070B54);B(p+16,4);F(p+44,x);F(p+48,y);H(p+108,0x50);W(p+0x200,80);return p;
}
static uint32_t MakeMenu(uint32_t vt,int count,uint32_t* outArray=nullptr) {
    auto menu=Alloc(),array=Alloc();W(menu,vt);W(menu+4,array);H(menu+38,count);H(menu+40,count);H(menu+34,0);
    for(int i=0;i<count;i++) { auto child=Alloc();W(array+4*i,child);W(child,0x82071034);W(child+16,Label(50,40+20.0f*i)); }
    if(outArray) *outArray=array;return menu;
}
static uint32_t MakeSystem(uint32_t menu,int index) {
    auto system=Alloc(),array=Alloc();W(system+4,array);W(system+12,index);W(system+20,16);B(system+28,1);W(array+4*index,menu);return system;
}
static PPCContext context{};
static void ResetPointer() {
    NativeMenuPointer::Cancel();previousTarget={};cursor={0,0};focused=true;overlay=false;held=false;confirms=backs=steps=0;sliderValueAddress=0;
    Update(context,Guest::Base);
}
static void Click(int x,int y,bool right=false) {
    cursor={x,y};NativeMenuPointer::Button(right,true,x,y);NativeMenuPointer::Button(right,false,x,y);Update(context,Guest::Base);
}
int main() {
    Guest::Base=static_cast<uint8_t*>(VirtualAlloc(nullptr,Guest::TotalSize,MEM_RESERVE,PAGE_NOACCESS));assert(Guest::Base);
    Commit(0x82070000,0x3000);Commit(Manager,0x1000);Commit(0x82A2A1C8,4);W(0x82A2A1C8,0);B(Manager+48,1);
    context.r1.u32=0x70010000;
    Bind(0x824CE558,FakeSelect);Bind(0x824BDDE8,FakeModalSelect);Bind(0x824CFBE8,FakeWidth);Bind(0x824CFB48,FakeHeight);Bind(0x824C48B8,FakeDispatch);
    for(auto address:{0x824E5310u,0x824D2640u,0x824BC8C8u,0x824BD3F8u}) Bind(address,FakeRefresh);
    auto menu=MakeMenu(0x82071EC4,3),front=MakeSystem(menu,2);W(Manager+28,front);ResetPointer();
    assert(NativeMenuPointer::Active());Click(110,67);assert(confirms==1&&lastSystem==front);
    Click(800,600);assert(confirms==1);
    cursor={110,97};Update(context,Guest::Base);assert(Guest::Read16(Guest::Base,menu+34)==1);
    H(menu+34,0);Update(context,Guest::Base);assert(Guest::Read16(Guest::Base,menu+34)==0); // stationary pointer preserves keyboard selection
    NativeMenuPointer::Button(false,true,110,67);NativeMenuPointer::Button(false,false,110,97);Update(context,Guest::Base);assert(confirms==1);
    Click(800,600,true);assert(backs==1);
    auto modalMenu=MakeMenu(0x820718EC,4),modal=MakeSystem(modalMenu,0),pres=Alloc();W(modal,0x820719D4);W(modal+52,pres);W(modal+60,0);W(modalMenu+56,2);W(pres+56,2);W(pres+16,Alloc());
    W(pres+28,Label(200,200));W(pres+32,Label(300,200));W(pres+60,1);W(Manager+200,modal);B(pres+65,0xFF); // padding must not become transition state
    ResetPointer();assert(Resolve(Guest::Base).modal&&Resolve(Guest::Base).ready);
    Click(110,67);assert(confirms==0); // underlying menu cannot receive the click
    Click(420,310);assert(confirms==1&&lastSystem==modal&&Guest::Read32(Guest::Base,pres+60)==1);
    auto dummy=Guest::Read32(Guest::Base,Guest::Read32(Guest::Base,modalMenu+4)+4);B(dummy+22,1);Click(620,310);assert(confirms==1);B(dummy+22,0);
    NativeMenuPointer::Button(false,true,420,310);Update(context,Guest::Base);
    sub_824CDB28(context,Guest::Base);Update(context,Guest::Base);NativeMenuPointer::Button(false,false,420,310);Update(context,Guest::Base);assert(confirms==1);
    W(modal+12,0xFFFFFFFF);assert(!Resolve(Guest::Base).modal);
    // Pause uses its own system and routes native action arguments correctly.
    B(front+28,0);auto pauseMenu=MakeMenu(0x82071A3C,7),pause=MakeSystem(pauseMenu,0);W(Manager+32,pause);ResetPointer();Click(110,67);assert(confirms==1&&lastSystem==pause&&lastLocal==0);
    B(pause+28,0);B(front+28,1);auto sound=MakeMenu(0x8207217C,1),soundArray=Guest::Read32(Guest::Base,front+4);W(front+12,5);W(soundArray+20,sound);
    auto image=Alloc(),prims=Alloc(),primitive=Alloc();W(sound+80,image);W(image,0x82070E04);B(image+16,4);F(image+52,1);W(image+32,prims);W(image+40,1);W(prims,primitive);H(primitive,0);H(primitive+6,100);
    for(int i=0;i<4;i++) { F(primitive+16+20*i,i<2?100:125);F(primitive+20+20*i,(i==0||i==3)?100:110); }
    ResetPointer();sliderValueAddress=sound+84;Click(300,157);assert(Guest::Read32(Guest::Base,sound+84)==25&&steps==25&&confirms==0);
    held=true;cursor={210,157};NativeMenuPointer::Button(false,true,210,157);Update(context,Guest::Base);assert(dragging==0);
    cursor={400,157};Update(context,Guest::Base);assert(Guest::Read32(Guest::Base,sound+84)==50);
    held=false;Update(context,Guest::Base);assert(dragging==-1);
    NativeMenuPointer::Button(false,true,110,67);overlay=true;Update(context,Guest::Base);overlay=false;Update(context,Guest::Base);NativeMenuPointer::Button(false,false,110,67);Update(context,Guest::Base);assert(confirms==0);
    focused=false;Update(context,Guest::Base);assert(active.load()==0);
    // The same native track data must honor sensitivity's fractional render
    // denominator while retaining its original integer maximum of 37.
    auto controller=MakeMenu(0x82072094,8);W(front+12,4);W(soundArray+16,controller);W(controller+180,image);
    ResetPointer();sliderValueAddress=controller+196;Click(399,157);assert(Guest::Read32(Guest::Base,controller+196)==37&&steps==37&&confirms==0);
    auto gameplay=MakeMenu(0x82071FAC,4);W(front+12,3);W(soundArray+12,gameplay);W(gameplay+116,image);
    ResetPointer();Click(210,157);assert(steps==1&&lastAction==16&&confirms==0);
    W(gameplay+100,Label(250,40));Click(510,67);assert(steps==2&&lastAction==32&&confirms==0);
    auto mainMenu=MakeMenu(0x82071DDC,5);W(front+12,1);W(soundArray+4,mainMenu);ResetPointer();
    B(mainMenu+72,1);Update(context,Guest::Base);assert(NativeMenuPointer::Active()&&active.load()==0);
    Click(110,67);assert(confirms==0);B(mainMenu+72,0);Update(context,Guest::Base);Click(110,67);assert(confirms==1);
    VirtualFree(Guest::Base,0,MEM_RELEASE);puts("Actual pointer module: menu, modal, pause, slider and cancellation checks passed.");
}
