#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>
namespace MenuPointer {
// Native modal systems use a signed current index: -1 idle, 0 first dialog.
inline bool ModalActive(uint32_t index) { return int32_t(index)>=0; }
inline int ModalSlot(int count,int row,bool paired) {
    if(count<1||count>4||row<0||row>=count) return -1;
    return count==1?2:row+((count==2||count==3)&&paired?1:0);
}
inline int RowAt(int count,int first,int offset,bool scrolling,bool wrap) {
    if(count<=0||offset<0||offset>=count) return -1;
    int row=(scrolling?first:0)+offset;
    if(row<0||(!wrap&&row>=count)) return -1;
    return row%count;
}
inline int SliderValue(float left,float width,float x,float maximum) {
    if(!std::isfinite(left)||!std::isfinite(width)||!std::isfinite(x)||!std::isfinite(maximum)||width<=0||maximum<=0) return -1;
    return std::clamp(int(std::lround(std::clamp((x-left)/width,0.0f,1.0f)*maximum)),0,int(maximum));
}
struct Point { float x=0,y=0; };
struct Rect { float x,y,width,height; };
inline bool Contains(Rect r,Point p) {
    return std::isfinite(r.x)&&std::isfinite(r.y)&&std::isfinite(r.width)&&std::isfinite(r.height)&&
        r.width>0&&r.height>0&&p.x>=r.x&&p.y>=r.y&&p.x<r.x+r.width&&p.y<r.y+r.height;
}
// Match the floor-rounded 16:9 viewport in Render::Present.
inline bool ToCanvas(int width,int height,int x,int y,float canvasWidth,float canvasHeight,Point& p,bool clamp=false) {
    if(width<=0||height<=0) return false;
    float w=float(width),h=w/(16.0f/9.0f);
    if(h>height) { h=float(height);w=h*(16.0f/9.0f); }
    float left=std::floor((width-w)*.5f),top=std::floor((height-h)*.5f);
    w=std::floor(w);h=std::floor(h);
    if(w<=0||h<=0||(!clamp&&!Contains({left,top,w,h},{float(x),float(y)}))) return false;
    p={(x-left)*canvasWidth/w,(y-top)*canvasHeight/h};
    if(clamp) { p.x=std::clamp(p.x,0.0f,canvasWidth);p.y=std::clamp(p.y,0.0f,canvasHeight); }
    return true;
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
