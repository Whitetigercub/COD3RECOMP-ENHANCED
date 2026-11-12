#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iterator>

namespace BloodMask {
inline constexpr int Side=64, Layers=2, Mips=7;

struct Patch { float x,y,rx,ry,angle,opacity; };

inline float Coverage(float x,float y,const Patch& p){
 const float c=std::cos(p.angle),s=std::sin(p.angle);
 const float dx=x-p.x,dy=y-p.y;
 const float u=(dx*c+dy*s)/p.rx,v=(dy*c-dx*s)/p.ry;
 return p.opacity*std::clamp((1.0f-std::sqrt(u*u+v*v))*5.0f+.5f,0.0f,1.0f);
}

inline uint8_t Texel(int layer,int col,int row){
 constexpr Patch splatter[]={
  {-.17f,.06f,.42f,.29f,-.55f,.98f},{.14f,-.04f,.32f,.40f,.43f,.86f},
  {-.37f,-.22f,.23f,.115f,.58f,.84f},{.32f,.17f,.23f,.13f,.68f,.70f},
  {-.02f,.32f,.15f,.22f,.35f,.88f},{.21f,-.39f,.12f,.22f,-.6f,.72f},
  {-.62f,.27f,.10f,.065f,-.44f,.91f},{.63f,.33f,.09f,.054f,.8f,.86f},
  {-.53f,-.48f,.06f,.105f,-.6f,.78f},{.42f,-.67f,.055f,.07f,.1f,.83f},
  {-.23f,.73f,.06f,.048f,.0f,.72f},{.74f,-.22f,.07f,.045f,.4f,.64f}
 };
 constexpr Patch streak[]={
  {-.44f,-.04f,.35f,.215f,.12f,.96f},{-.13f,.04f,.41f,.154f,.07f,.89f},
  {.19f,-.015f,.36f,.087f,-.24f,.76f},{.35f,.12f,.29f,.059f,.03f,.67f},
  {.64f,-.078f,.15f,.039f,-.16f,.73f},{.80f,.12f,.085f,.032f,-.12f,.61f},
  {-.61f,.30f,.10f,.06f,.23f,.74f},{-.23f,-.28f,.07f,.047f,-.29f,.59f}
 };
 const float x=(col+.5f)*2.0f/Side-1.0f,y=(row+.5f)*2.0f/Side-1.0f;
 // Warp the patch edges before combining them so the individual ellipses
 // don't read as clean stamps. This runs only when creating the mask.
 const float wx=x+.040f*std::sin(15*y+5*x)+.015f*std::sin(37*y-19*x);
 const float wy=y+.034f*std::sin(17*x-4*y)+.016f*std::sin(33*x+21*y);
 float a=0;
 const Patch* patches=layer==0?splatter:streak;
 const size_t count=layer==0?std::size(splatter):std::size(streak);
 for(size_t i=0;i<count;i++)a=std::max(a,Coverage(wx,wy,patches[i]));
 constexpr Patch holes[]={{-.22f,.14f,.061f,.085f,-.4f,1},{.07f,-.13f,.062f,.042f,.8f,.87f}};
 constexpr Patch streakHoles[]={{-.49f,.03f,.105f,.048f,.2f,.9f},{.14f,.018f,.17f,.026f,-.13f,.92f}};
 const Patch* openings=layer==0?holes:streakHoles;
 for(size_t i=0;i<std::size(holes);i++)a*=1-Coverage(wx,wy,openings[i]);
 const float grain=.78f+.14f*std::sin(x*49+y*29)*std::sin(y*51-x*18)+.08f*std::sin(x*13-y*23);
 return uint8_t(std::clamp(a*grain,0.0f,1.0f)*255+.5f);
}
}
