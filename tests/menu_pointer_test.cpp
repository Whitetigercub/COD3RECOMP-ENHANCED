#include "../CoD3Host/menu_pointer.h"
#include <cassert>
#include <limits>
int main() {
    using namespace MenuPointer;
    assert(!ModalActive(0xFFFFFFFFu));assert(ModalActive(0));assert(ModalActive(1));
    assert(RowAt(11,8,0,true,true)==8&&RowAt(11,8,3,true,true)==0);
    assert(RowAt(11,8,3,true,false)==-1&&RowAt(3,0,3,false,false)==-1);
    assert(ModalSlot(1,0,true)==2);
    assert(ModalSlot(2,0,true)==1&&ModalSlot(2,1,true)==2);
    assert(ModalSlot(2,0,false)==0&&ModalSlot(2,1,false)==1);
    assert(ModalSlot(3,2,true)==3&&ModalSlot(4,3,true)==3);
    assert(ModalSlot(0,0,true)==-1&&ModalSlot(4,4,true)==-1);
    assert(SliderValue(100,100,100,50)==0);
    assert(SliderValue(100,100,150,50)==25);
    assert(SliderValue(100,100,200,37.5f)==37);
    assert(SliderValue(100,100,-100,50)==0&&SliderValue(100,100,500,50)==50);
    assert(SliderValue(100,0,150,50)==-1);
    Point p;
    assert(ToCanvas(1280,720,640,360,1280,720,p)&&p.x==640&&p.y==360);
    assert(ToCanvas(1920,1080,960,540,1280,720,p)&&p.x==640&&p.y==360);
    assert(!ToCanvas(1280,1024,100,100,1280,720,p));
    assert(ToCanvas(1280,1024,640,512,1280,720,p)&&p.x==640&&p.y==360);
    assert(!ToCanvas(2560,1080,100,540,1280,720,p));
    assert(ToCanvas(2560,1080,1280,540,1280,720,p)&&p.x==640&&p.y==360);
    assert(ToCanvas(1280,720,1500,360,640,480,p,true)&&p.x==640);
    assert(!ToCanvas(0,720,0,0,1280,720,p));
    assert(Contains({10,20,30,40},{10,20}));
    assert(!Contains({10,20,30,40},{40,20}));
    assert(!Contains({10,20,30,40},{10,60}));
    assert(!Contains({10,20,-30,40},{10,20}));
    assert(!Contains({10,20,std::numeric_limits<float>::quiet_NaN(),40},{10,20}));
    Press press;
    assert(!press.Up(1));
    press.Down(1);assert(!press.Up(2));
    press.Down(-1);assert(!press.Up(1));
    press.Down(1);assert(press.Up(1));assert(!press.Up(1));
    press.Down(1);press.Cancel();assert(!press.Up(1));
}
