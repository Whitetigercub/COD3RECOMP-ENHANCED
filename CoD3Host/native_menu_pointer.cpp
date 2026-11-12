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
#include <algorithm>
#include <cmath>
#include <Windows.h>

#ifdef COD3_MP
bool NativeMenuPointer::Active() { return false; }
bool NativeMenuPointer::RequestQuit() { return false; }
bool NativeMenuPointer::InMission() { return false; }
bool NativeMenuPointer::MenuVisible() { return false; }
void NativeMenuPointer::Button(bool,bool,int,int) {}
void NativeMenuPointer::Cancel() {}
#else
namespace {
constexpr uint32_t Manager=0x82A8DF2C;
std::atomic<uint64_t> active{0};
std::atomic<uint64_t> modalRevision{0};
std::atomic<bool> quitRequested{false};
uint32_t quitMenu=0;
uint32_t quitStrings=0;
bool configuringQuit=false;
struct Event { uint64_t token; bool right,down; int x,y; };
std::mutex mutex;
std::vector<Event> events;
MenuPointer::Press left,right;
struct Target {
    uint32_t system=0,menu=0,local=0,presentation=0;
    int rows=0,busy=0;
    bool modal=false,owned=false,ready=false;
    uint64_t revision=0;
    bool operator==(const Target&) const = default;
};
Target previousTarget;
uint64_t token=0;
POINT previousPoint{};
bool havePoint=false;
int dragging=-1;
bool Trace() { static bool on=getenv("COD3_MENUMOUSELOG")!=nullptr;return on; }
std::atomic<bool> cancelled{false};
bool Valid(uint32_t p) { return p>=0x10000&&p<0xFFFF0000u; }
float Float(uint8_t* base,uint32_t p) {
    uint32_t word=Guest::Read32(base,p);float value;memcpy(&value,&word,4);return value;
}
uint32_t Call(PPCContext& ctx,uint8_t* base,uint32_t fn,uint32_t object,uint32_t argument,uint32_t argument2=0) {
    if(auto* target=Guest::Lookup(fn)) {
        PPCContext call=ctx;call.r1.u32=(ctx.r1.u32-0x2000)&~15u;
        call.r3.u64=object;call.r4.u64=argument;call.r5.u64=argument2;call.lr=0;target(call,base);return call.r3.u32;
    }
    return 0;
}
uint32_t Method(uint8_t* base,uint32_t object,uint32_t slot) {
    if(!Valid(object)) return 0;
    uint32_t vtable=Guest::Read32(base,object);
    return vtable>=0x82000000&&vtable<0x82C30000?Guest::Read32(base,vtable+slot):0;
}
void CallFloat(PPCContext& ctx,uint8_t* base,uint32_t fn,uint32_t object,uint32_t argument,uint32_t argument2,float value) {
    if(auto* target=Guest::Lookup(fn)) {
        PPCContext call=ctx;call.r1.u32=(ctx.r1.u32-0x2000)&~15u;
        call.r3.u64=object;call.r4.u64=argument;call.r5.u64=argument2;call.f1.f64=value;call.lr=0;target(call,base);
    }
}
struct Spec { int index;uint32_t vtable;int rows,busy;bool pause; };
constexpr Spec Specs[]={
    {1,0x82071DDC,5,72,false}, {2,0x82071EC4,3,76,false},
    {9,0x820728BC,4,0,false}, {10,0x8207251C,6,0,false},
    {14,0x820729A4,4,0,false},
    {3,0x82071FAC,4,0,false}, {4,0x82072094,8,0,false},
    {5,0x8207217C,1,0,false},
    {0,0x82071A3C,7,0,true}, {1,0x82071B24,11,0,true}
};
Target Resolve(uint8_t* base) {
    Target t;if(!base) return t;
    uint32_t local=Guest::Read32(base,0x82A2A1C8);if(local>3) return t;
    t.local=local;
    uint32_t frontend=Guest::Read32(base,Manager+28),pause=Guest::Read32(base,Manager+32);
    bool frontVisible=Valid(frontend)&&Guest::Read8(base,frontend+28);
    uint32_t modal=Guest::Read32(base,Manager+200+4*(frontVisible?0:local));
    if(Valid(modal)) {
        uint32_t slot=Guest::Read32(base,modal+60);
        if(slot>3) { t.owned=true;return t; }
        int index=int32_t(Guest::Read32(base,modal+12+4*slot));
        if(index>=0) {
            // Modal ownership always blocks the underlying menu, even if an
            // unknown dialog cannot be safely interacted with yet.
            t.owned=true;t.modal=true;t.system=modal;t.local=slot;t.revision=modalRevision.load();
            if(Guest::Read32(base,modal)!=0x820719D4||index>1) return t;
            uint32_t array=Guest::Read32(base,modal+4);if(!Valid(array)) return t;
            t.menu=Guest::Read32(base,array+4*index);
            t.presentation=Guest::Read32(base,modal+52);
            if(!Valid(t.menu)||!Valid(t.presentation)||Guest::Read32(base,t.menu)!=0x820718EC) return t;
            t.rows=int(Guest::Read32(base,t.menu+56));
            if(t.rows<1||t.rows>4||Guest::Read32(base,t.presentation+56)!=uint32_t(t.rows)) return t;
            t.ready=Guest::Read8(base,Manager+48)&&!Guest::Read8(base,t.menu+48)&&!Guest::Read8(base,t.presentation+64);
            return t;
        }
    }
    bool paused=false;
    if(Valid(frontend)&&Guest::Read8(base,frontend+28)) t.system=frontend;
    else if(Valid(pause)&&Guest::Read8(base,pause+28+local)) { t.system=pause;paused=true; }
    else return t;
    int index=int32_t(Guest::Read32(base,t.system+12));
    int count=int32_t(Guest::Read32(base,t.system+20));
    uint32_t array=Guest::Read32(base,t.system+4);
    if(index<0||index>=count||count>64||!Valid(array)) return t;
    t.menu=Guest::Read32(base,array+4*index);if(!Valid(t.menu)) return t;
    for(const auto& spec:Specs) {
        if(spec.pause==paused&&spec.index==index&&Guest::Read32(base,t.menu)==spec.vtable) {
            t.owned=true;t.rows=spec.rows;t.busy=spec.busy;
            t.ready=Guest::Read8(base,Manager+48)&&!Guest::Read8(base,t.menu+48)&&
                (!spec.busy||!Guest::Read8(base,t.menu+spec.busy));
            return t;
        }
    }
    return t;
}
bool MainReady(uint8_t* base) {
    const Target t=Resolve(base);
    const uint32_t frontend=Guest::Read32(base,Manager+28);
    return t.owned&&t.ready&&!t.modal&&Valid(frontend)&&t.system==frontend&&
        Guest::Read32(base,frontend+12)==1&&Guest::Read32(base,t.menu)==0x82071DDC;
}
void Invoke(PPCContext& ctx,uint8_t* base,uint32_t fn,uint32_t a,uint32_t b=0,uint32_t c=0,uint32_t d=0,uint32_t e=0,uint32_t f=0) {
    if(auto* target=Guest::Lookup(fn)) {
        PPCContext call=ctx;call.r1.u32=(ctx.r1.u32-0x2000)&~15u;
        call.r3.u64=a;call.r4.u64=b;call.r5.u64=c;call.r6.u64=d;call.r7.u64=e;call.r8.u64=f;call.lr=0;
        target(call,base);
    }
}
void OpenQuit(PPCContext& ctx,uint8_t* base) {
    if(!MainReady(base)) return;
    const uint32_t modal=Guest::Read32(base,Manager+200);
    if(!Valid(modal)||Guest::Read32(base,modal)!=0x820719D4||Guest::Read32(base,modal+60)!=0||
       int32_t(Guest::Read32(base,modal+12))>=0) return;
    const uint32_t array=Guest::Read32(base,modal+4),presentation=Guest::Read32(base,modal+52);
    if(!Valid(array)||!Valid(presentation)) return;
    for(int i=0;i<2;i++) { const uint32_t item=Guest::Read32(base,array+4*i);
        if(!Valid(item)||Guest::Read32(base,item)!=0x820718EC) return; }
    for(uint32_t fn:{0x824D31E0u,0x824C4428u,0x824BDDE8u,0x824CDB28u,0x824CDD78u,0x823F86D0u}) if(!Guest::Lookup(fn)) return;
    if(!quitStrings) {
        quitStrings=Guest::AllocatePhysical(256);
        if(!quitStrings) return;
        memcpy(base+quitStrings,"Quit Game?",11);
        memcpy(base+quitStrings+32,"Quit",5);
        memcpy(base+quitStrings+64,"Cancel",7);
    }
    // Mirrors the game's two-button dialog setup (0x823FC2C8).
    // The callbacks return true so the native dispatcher closes the dialog.
    Invoke(ctx,base,0x824D31E0,modal,quitStrings,0,0,0x82065B28,1);
    const int index=int32_t(Guest::Read32(base,modal+12));
    if(index<0||index>1) return;
    const uint32_t menu=Guest::Read32(base,array+(index==0?0:4));
    Invoke(ctx,base,0x824C4428,menu,quitStrings+32,0x823F86D0);
    Invoke(ctx,base,0x824C4428,menu,quitStrings+64,0x823F86D0);
    Guest::Write16(base,menu+34,1);
    Invoke(ctx,base,0x824BDDE8,presentation,1);
    quitMenu=menu;configuringQuit=true;
    Invoke(ctx,base,0x824CDB28,menu,1,0);
    configuringQuit=false;
}
float Measure(PPCContext& ctx,uint8_t* base,uint32_t function,uint32_t text,uint32_t scale) {
    auto* target=Guest::Lookup(function);if(!target) return 0;
    PPCContext call=ctx;call.r1.u32=(ctx.r1.u32-0x2000)&~15u;
    call.r3.u64=text;call.r4.u64=scale;call.lr=0;target(call,base);return float(call.f1.f64);
}
bool LabelHit(PPCContext& ctx,uint8_t* base,uint32_t text,bool selected,MenuPointer::Point point) {
    if(!Valid(text)||Guest::Read32(base,text)!=0x82070B54||!(Guest::Read8(base,text+16)&4)) return false;
    // Shared by plain menu labels and actual rendered modal button captions.
    uint32_t scale=text+(selected?56:80);
    float w=Measure(ctx,base,0x824CFBE8,text,scale),h=Measure(ctx,base,0x824CFB48,text,scale);
    float left=Float(base,text+44),top=Float(base,text+48);
    uint16_t flags=Guest::Read16(base,text+108);
    left-=(flags&0x20)?w:((flags&0x10)?0:w*.5f);
    top-=(flags&0x80)?h:((flags&0x40)?0:h*.5f);
    return MenuPointer::Contains({left,top,w,h},point);
}
struct Control { int kind=0;MenuPointer::Rect bounds{};uint32_t value=0;float maximum=0; };
bool ImageBounds(uint8_t* base,uint32_t image,bool track,MenuPointer::Rect& bounds) {
    if(!Valid(image)||Guest::Read32(base,image)!=0x82070E04||!(Guest::Read8(base,image+16)&4)||Guest::Read32(base,image+40)!=1) return false;
    uint32_t array=Guest::Read32(base,image+32);if(!Valid(array)) return false;
    uint32_t primitive=Guest::Read32(base,array);if(!Valid(primitive)) return false;
    float left=Float(base,primitive+16),right=left,top=Float(base,primitive+20),bottom=top;
    for(int i=1;i<4;i++) {
        float x=Float(base,primitive+16+20*i),y=Float(base,primitive+20+20*i);
        left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);
    }
    if(track) {
        left=Float(base,primitive+16);
        right=left+(int16_t(Guest::Read16(base,primitive+6))-int16_t(Guest::Read16(base,primitive)))*Float(base,image+52);
    }
    bounds={left,top,right-left,bottom-top};
    return std::isfinite(left)&&std::isfinite(top)&&std::isfinite(bounds.width)&&std::isfinite(bounds.height)&&bounds.width>0&&bounds.height>0;
}
bool Slider(uint8_t* base,const Target& t,int row,Control& control) {
    if(t.modal) return false;
    uint32_t vtable=Guest::Read32(base,t.menu),bar=0,value=0;
    float maximum=50;
    if(vtable==0x82072094&&(row==2||row==3)) { bar=180+4*(row-2);value=196+4*(row-2);if(row==2) maximum=37.5f; }
    if(vtable==0x8207217C&&row==0) { bar=80;value=84; }
    if(vtable==0x82071B24&&(row==2||row==3||row==6)) { int index=row==6?2:row-2;bar=136+4*index;value=152+4*index;if(row==2) maximum=37.5f; }
    if(!bar||!ImageBounds(base,Guest::Read32(base,t.menu+bar),true,control.bounds)) return false;
    control.kind=1;control.value=t.menu+value;control.maximum=maximum;return true;
}
uint32_t ArrowOffset(uint8_t* base,const Target& t,int row) {
    if(t.modal) return 0;
    switch(Guest::Read32(base,t.menu)) {
    case 0x82071FAC:case 0x82072094:return 116+8*row;
    case 0x82071B24: {
        int index=row==0?0:row==1?1:row==4?2:row==5?3:row==7?4:row==8?5:-1;
        return index>=0?88+8*index:0;
    }
    }
    return 0;
}
bool Toggle(uint8_t* base,const Target& t,int row) {
    if(t.modal) return false;
    uint32_t vtable=Guest::Read32(base,t.menu);
    return (vtable==0x82071FAC&&row>=0&&row<4)||
        (vtable==0x82072094&&row>=4&&row<=7)||
        (vtable==0x82071B24&&(row==0||row==1||row==4||row==5||row==7||row==8));
}
uint32_t ValueText(uint8_t* base,const Target& t,int row) {
    if(t.modal) return 0;
    switch(Guest::Read32(base,t.menu)) {
    case 0x82071FAC:return Guest::Read32(base,t.menu+100+4*row);
    case 0x82072094:return Guest::Read32(base,t.menu+84+4*row);
    case 0x82071B24: {
        int index=row==0?0:row==1?1:row==4?2:row==5?3:row==7?4:row==8?5:-1;
        return index>=0?Guest::Read32(base,t.menu+64+4*index):0;
    }
    }
    return 0;
}
int Hit(PPCContext& ctx,uint8_t* base,const Target& t,int x,int y,Control* picked=nullptr) {
    if(picked) *picked={};
    RECT client{};
    if(!GetClientRect(static_cast<HWND>(Window::Handle()),&client)) return -1;
    MenuPointer::Point point;
    if(!MenuPointer::ToCanvas(client.right,client.bottom,x,y,640,480,point)) return -1;
    uint32_t array=Guest::Read32(base,t.menu+4);
    int count=Guest::Read16(base,t.menu+38);
    if(!Valid(array)||count!=(t.modal?4:t.rows)) return -1;
    uint16_t flags=Guest::Read16(base,t.menu+42);
    int first=int16_t(Guest::Read16(base,t.menu+32));
    int slots=int16_t(Guest::Read16(base,t.menu+40));
    int shown=0;
    bool scrolling=!t.modal&&(flags&1);
    for(int offset=0;offset<t.rows;offset++) {
        int i=MenuPointer::RowAt(t.rows,first,offset,scrolling,(flags&2)!=0);
        if(i<0) break;
        uint32_t child=Guest::Read32(base,array+4*i);
        if(!Valid(child)||Guest::Read32(base,child)!=0x82071034) continue;
        bool disabled=Guest::Read8(base,child+22)!=0;
        if(scrolling) {
            if((flags&0x20)&&disabled) continue;
            if(shown++>=slots) break;
        }
        if(disabled) continue;
        Control slider;
        if(Slider(base,t,i,slider)&&MenuPointer::Contains(slider.bounds,point)) { if(picked) *picked=slider;return i; }
        uint32_t arrows=ArrowOffset(base,t,i);
        if(arrows) for(int side=0;side<2;side++) {
            MenuPointer::Rect bounds;
            if(ImageBounds(base,Guest::Read32(base,t.menu+arrows+4*side),false,bounds)&&MenuPointer::Contains(bounds,point)) {
                if(picked) { picked->kind=2+side;picked->bounds=bounds; }return i;
            }
        }
        uint32_t text=Guest::Read32(base,child+16);
        bool selected=Guest::Read8(base,child+21)!=0;
        if(t.modal) {
            bool paired=Guest::Read32(base,t.presentation+28)&&Guest::Read32(base,t.presentation+32);
            // Reject incomplete alternative layouts where the presenter's
            // selection mapping would differ from its caption layout.
            if(paired&&t.rows==2&&!Guest::Read32(base,t.presentation+16)) continue;
            if(t.rows==2&&!paired&&(!Guest::Read32(base,t.presentation+24)||!Guest::Read32(base,t.presentation+28)||!Guest::Read32(base,t.presentation+12))) continue;
            if(t.rows==3&&(!paired||!Guest::Read32(base,t.presentation+16)||!Guest::Read32(base,t.presentation+20)||!Guest::Read32(base,t.presentation+36))) continue;
            if(t.rows==4&&(!Guest::Read32(base,t.presentation+24)||!Guest::Read32(base,t.presentation+28)||!Guest::Read32(base,t.presentation+32)||!Guest::Read32(base,t.presentation+36)||!Guest::Read32(base,t.presentation+16)||!Guest::Read32(base,t.presentation+20))) continue;
            int slot=MenuPointer::ModalSlot(t.rows,i,paired);
            text=Guest::Read32(base,t.presentation+24+4*slot);
            selected=int32_t(Guest::Read32(base,t.presentation+60))==slot;
        }
        if(LabelHit(ctx,base,text,selected,point)) return i;
        uint32_t value=ValueText(base,t,i);
        if(value&&LabelHit(ctx,base,value,selected,point)) { if(picked) picked->kind=3;return i; }
    }
    return -1;
}
void RefreshDetails(PPCContext& ctx,uint8_t* base,const Target& t,int row) {
    uint32_t vt=Guest::Read32(base,t.menu),descriptionTable=0,imageTable=0,imageOffset=0;
    // The refresh portions of 824C3C08, 824C3FE0 and 824C3248, without
    // invoking their directional navigation. Keep help text/art on the row
    // selected by the cursor as well as by the keyboard/controller.
    if(vt==0x82072094) { descriptionTable=0x82064810;imageTable=0x82064858;imageOffset=76; }
    if(vt==0x82071FAC) { descriptionTable=0x82064744;imageTable=0x82064774;imageOffset=92; }
    if(vt==0x820729A4) { descriptionTable=0x82064BE0;imageTable=0x82064BF0;imageOffset=80; }
    if(!descriptionTable) return;
    uint32_t description=Guest::Read32(base,t.menu+72),descriptionMethod=Method(base,description,132);
    if(descriptionMethod) Call(ctx,base,descriptionMethod,description,Guest::Read32(base,descriptionTable+4*row));
    uint32_t image=Guest::Read32(base,t.menu+imageOffset),stateMethod=Method(base,image,304),imageMethod=Method(base,image,360);
    if(stateMethod&&imageMethod) {
        uint32_t oldState=Call(ctx,base,stateMethod,image,0);
        CallFloat(ctx,base,imageMethod,image,Guest::Read32(base,imageTable+4*row),oldState,Float(base,0x825D121C));
    }
    if(vt==0x82072094) {
        for(int axis=0;axis<2;axis++) {
            uint32_t bar=Guest::Read32(base,t.menu+180+4*axis),method=Method(base,bar,136);
            if(method) CallFloat(ctx,base,method,bar,0,0,Float(base,row==axis+2?0x82064200:0x82062720));
        }
        uint32_t help=Guest::Read32(base,t.menu+60),method=Method(base,help,132);
        if(method) Call(ctx,base,method,help,row<2?0x8206FC3C:0x8206FFE4);
    }
}
void Select(PPCContext& ctx,uint8_t* base,const Target& t,int row) {
    Call(ctx,base,0x824CE558,t.menu,row);
    if(t.modal) Call(ctx,base,0x824BDDE8,t.presentation,row);
    else {
        uint32_t vtable=Guest::Read32(base,t.menu);
        uint32_t secondaryOffset=vtable==0x82072094?80:vtable==0x82071FAC?96:0;
        if(secondaryOffset) {
            uint32_t secondary=Guest::Read32(base,t.menu+secondaryOffset);
            if(Valid(secondary)&&Guest::Read16(base,secondary+38)>row) {
                uint32_t secondaryVtable=Guest::Read32(base,secondary);
                if(Valid(secondaryVtable)) Call(ctx,base,Guest::Read32(base,secondaryVtable+164),secondary,row,1);
            }
        }
        RefreshDetails(ctx,base,t,row);
        switch(vtable) {
        case 0x82071EC4: Call(ctx,base,0x824E5310,t.menu,0);break;
        case 0x820728BC: Call(ctx,base,0x824D2640,t.menu,0);break;
        case 0x8207251C: Call(ctx,base,0x824BC8C8,t.menu,0);break;
        case 0x82071B24: Call(ctx,base,0x824BD3F8,t.menu,0);break;
        }
    }
}
void Dispatch(PPCContext& ctx,uint8_t* base,const Target& t,uint32_t action) {
    constexpr uint32_t RestartCheckpointYes=0x824EE5F0;
    const bool checkpointYes=action==64&&t.modal&&t.ready&&
        Guest::Read16(base,t.menu+34)==0&&Guest::Read32(base,t.menu+60)==RestartCheckpointYes;
    Call(ctx,base,0x824C48B8,t.system,action,t.local);
    if(!checkpointYes) return;

    // Mouse dispatch runs after the native UI tick. Restart Checkpoint
    // queues its reload before that tick can finish the dialog close.
    // Complete the pending close for this same dialog with the routine
    // its native presentation update normally calls.
    const Target after=Resolve(base);
    if(after.modal&&after.system==t.system&&after.menu==t.menu&&
       after.presentation==t.presentation&&after.local==t.local&&
       after.revision==t.revision&&Guest::Read8(base,t.presentation+64)==1)
        Call(ctx,base,0x824CE230,t.system,0);
}
void Adjust(PPCContext& ctx,uint8_t* base,const Target& t,int row,int x,int y) {
    Control control;RECT client{};MenuPointer::Point point;
    if(!Slider(base,t,row,control)||!GetClientRect(static_cast<HWND>(Window::Handle()),&client)||
        !MenuPointer::ToCanvas(client.right,client.bottom,x,y,640,480,point,true)) return;
    int wanted=MenuPointer::SliderValue(control.bounds.x,control.bounds.width,point.x,control.maximum);
    if(wanted<0) return;
    Select(ctx,base,t,row);
    // Let the original adjustment callback update preferences, fill geometry,
    // audio and animations. Never write slider values behind its back.
    for(int step=0;step<50;step++) {
        int before=int(Guest::Read32(base,control.value));
        if(before==wanted||!(Resolve(base)==t)) break;
        Dispatch(ctx,base,t,wanted>before?32:16);
        int after=int(Guest::Read32(base,control.value));
        if(after==before) break;
    }
}
void Update(PPCContext& ctx,uint8_t* base) {
    if(quitRequested.exchange(false)) OpenQuit(ctx,base);
    Target target=Resolve(base);
    if(quitMenu && (!target.modal || target.menu!=quitMenu)) quitMenu=0;
    static const bool ignored=[] {
        const char* value=getenv("COD3_NOKEYBOARD");if(!value) value=getenv("COD3_BACKGROUND");
        return value&&*value&&*value!='0';
    }();
    bool enabled=target.ready&&Window::HasFocus()&&!Overlay::IsOpen()&&!ignored;
    bool changed=cancelled.exchange(false)||!(target==previousTarget);
    if(changed) {
        ++token;
        if(Trace()&&target.owned) { printf("menu pointer: context system %08X menu %08X modal %d rows %d ready %d\n",target.system,target.menu,int(target.modal),target.rows,int(target.ready));fflush(stdout); }
    }
    active.store(enabled?token:0);
    std::vector<Event> pending;
    { std::lock_guard lock(mutex);pending.swap(events); }
    if(changed||!enabled) { left.Cancel();right.Cancel();dragging=-1;havePoint=false;pending.clear(); }
    previousTarget=target;
    if(!enabled) return;
    POINT point{};
    if(GetCursorPos(&point)&&ScreenToClient(static_cast<HWND>(Window::Handle()),&point)) {
        if(dragging<0&&(!havePoint||point.x!=previousPoint.x||point.y!=previousPoint.y)) {
            int row=Hit(ctx,base,target,point.x,point.y);
            if(row>=0&&int16_t(Guest::Read16(base,target.menu+34))!=row) {
                Select(ctx,base,target,row);
                if(Trace()) { printf("menu pointer: hover row %d at client %ld,%ld\n",row,point.x,point.y);fflush(stdout); }
            }
        }
        previousPoint=point;havePoint=true;
    }
    for(const auto& e:pending) {
        if(e.token!=token||!(Resolve(base)==target)) { left.Cancel();right.Cancel();dragging=-1;break; }
        Control control;
        int row=Hit(ctx,base,target,e.x,e.y,&control);
        if(e.right) {
            RECT client{};MenuPointer::Point canvas;
            bool inside=GetClientRect(static_cast<HWND>(Window::Handle()),&client)&&
                MenuPointer::ToCanvas(client.right,client.bottom,e.x,e.y,640,480,canvas);
            if(e.down) right.Down(inside?0:-1);
            else if(right.Up(inside?0:-1)) {
                if(Trace()) { printf("menu pointer: back\n");fflush(stdout); }
                Dispatch(ctx,base,target,128);
            }
        } else if(e.down) {
            left.Down(row<0?-1:row*4+control.kind);
            if(row>=0) Select(ctx,base,target,row);
            if(row>=0&&control.kind==1) { dragging=row;Adjust(ctx,base,target,row,e.x,e.y); }
        } else if(dragging>=0) {
            Adjust(ctx,base,target,dragging,e.x,e.y);dragging=-1;left.Cancel();
        } else if(left.Up(row<0?-1:row*4+control.kind)) {
            if(Trace()) { printf("menu pointer: activate row %d kind %d\n",row,control.kind);fflush(stdout); }
            Select(ctx,base,target,row);
            Dispatch(ctx,base,target,control.kind==2?16:(control.kind==3||Toggle(base,target,row)?32:64));
        }
    }
    if(dragging>=0&&!(GetAsyncKeyState(VK_LBUTTON)&0x8000)) { dragging=-1;left.Cancel(); }
    if(dragging>=0&&Resolve(base)==target&&GetCursorPos(&point)&&ScreenToClient(static_cast<HWND>(Window::Handle()),&point))
        Adjust(ctx,base,target,dragging,point.x,point.y);
}
}
bool NativeMenuPointer::Active() { return Resolve(Guest::Base).owned; }
bool NativeMenuPointer::RequestQuit() {
    auto* base=Guest::Base;
    if(!base||!Window::HasFocus()||Overlay::IsOpen()||!MainReady(base)) return false;
    quitRequested.store(true);return true;
}
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
    uint64_t route=active.load();if(!route) return;
    std::lock_guard lock(mutex);
    if(events.size()<64) events.push_back({route,right,down,x,y});
    else { events.clear();active.store(0);cancelled.store(true); }
}
void NativeMenuPointer::Cancel() {
    active.store(0);cancelled.store(true);std::lock_guard lock(mutex);events.clear();
}
// Service whichever native UI context is in front once per game frame.
extern "C" PPC_FUNC(__imp__sub_82511168);
PPC_FUNC(sub_82511168) {
    __imp__sub_82511168(ctx,base);
    Update(ctx,base);
}
// Route Back on this confirmation to Cancel even when Quit is highlighted.
extern "C" PPC_FUNC(__imp__sub_824C48B8);
PPC_FUNC(sub_824C48B8) {
    const Target t=Resolve(base);
    if(quitMenu && t.modal && t.menu==quitMenu && ctx.r3.u32==t.system && ctx.r4.u32==128) {
        Invoke(ctx,base,0x824CDD78,quitMenu,1);
        return;
    }
    __imp__sub_824C48B8(ctx,base);
}
// Native confirmation dispatcher: row 0 quits; row 1 uses the normal
// callback and modal close. Other dialogs always retain their guest behavior.
extern "C" PPC_FUNC(__imp__sub_824CDD78);
PPC_FUNC(sub_824CDD78) {
    if(quitMenu && ctx.r3.u32==quitMenu && ctx.r4.u32==0) {
        PostMessageW(static_cast<HWND>(Window::Handle()),WM_CLOSE,0,0);
        ctx.r4.u64=1;
    }
    __imp__sub_824CDD78(ctx,base);
}
// A pooled dialog can be configured again at the same address. Invalidate
// all old pointer presses when its callbacks/captions are replaced.
extern "C" PPC_FUNC(__imp__sub_824CDB28);
PPC_FUNC(sub_824CDB28) {
    if(!configuringQuit && ctx.r3.u32==quitMenu) quitMenu=0;
    modalRevision.fetch_add(1);
    NativeMenuPointer::Cancel();
    __imp__sub_824CDB28(ctx,base);
}
#endif
