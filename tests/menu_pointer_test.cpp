#include "../CoD3Host/menu_pointer.h"
#include <cassert>
#include <limits>
int main() {
    using namespace MenuPointer;
    assert(!ModalActive(0xFFFFFFFFu));assert(ModalActive(0));assert(ModalActive(1));
    Point p;
    assert(ToCanvas(1280,720,640,360,1280,720,p)&&p.x==640&&p.y==360);
    assert(ToCanvas(1920,1080,960,540,1280,720,p)&&p.x==640&&p.y==360);
    assert(!ToCanvas(1280,1024,100,100,1280,720,p));
    assert(ToCanvas(1280,1024,640,512,1280,720,p)&&p.x==640&&p.y==360);
    assert(!ToCanvas(2560,1080,100,540,1280,720,p));
    assert(ToCanvas(2560,1080,1280,540,1280,720,p)&&p.x==640&&p.y==360);
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
