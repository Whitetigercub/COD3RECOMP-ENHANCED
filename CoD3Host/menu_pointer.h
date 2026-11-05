#pragma once
#include <cmath>
#include <cstdint>
namespace MenuPointer {
// Native modal systems use a signed current index: -1 idle, 0 first dialog.
inline bool ModalActive(uint32_t index) { return int32_t(index)>=0; }
struct Point { float x=0,y=0; };
struct Rect { float x,y,width,height; };
inline bool Contains(Rect r,Point p) {
    return std::isfinite(r.x)&&std::isfinite(r.y)&&std::isfinite(r.width)&&std::isfinite(r.height)&&
        r.width>0&&r.height>0&&p.x>=r.x&&p.y>=r.y&&p.x<r.x+r.width&&p.y<r.y+r.height;
}
// Match the floor-rounded 16:9 viewport in Render::Present.
inline bool ToCanvas(int width,int height,int x,int y,float canvasWidth,float canvasHeight,Point& p) {
    if(width<=0||height<=0) return false;
    float w=float(width),h=w/(16.0f/9.0f);
    if(h>height) { h=float(height);w=h*(16.0f/9.0f); }
    float left=std::floor((width-w)*.5f),top=std::floor((height-h)*.5f);
    w=std::floor(w);h=std::floor(h);
    if(!Contains({left,top,w,h},{float(x),float(y)})) return false;
    p={(x-left)*canvasWidth/w,(y-top)*canvasHeight/h}; return true;
}
// A release activates only the row on which this press began. Changing menus,
// losing focus or entering an overlay cancels the press.
struct Press {
    int row=-1;
    void Down(int hit) { row=hit; }
    bool Up(int hit) { bool activate=row>=0&&row==hit;row=-1;return activate; }
    void Cancel() { row=-1; }
};
}
