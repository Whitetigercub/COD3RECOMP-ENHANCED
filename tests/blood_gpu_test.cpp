#include "blood.h"
#include "blood_mask.h"
void BloodDiagnosticEye(float* eye){eye[0]=eye[1]=eye[2]=0;}
#include "blood_render.h"
#include "blood_projection.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <chrono>
using Microsoft::WRL::ComPtr;
static uint32_t color=0xff0000ff;static float z=.5f;static bool show=true;
static unsigned maskMode=0;
static uint32_t UV(unsigned u,unsigned v,unsigned layer){return u|(v<<8)|(layer<<16);}
int Blood::BuildWorld(WorldVertex* out,int){
 if(!show)return 0;
 if(maskMode){
  auto stamp=[&](float x,float y,float radius,unsigned layer){
   WorldVertex a{x-radius,y-radius,z,color,UV(0,0,layer)},b{x+radius,y-radius,z,color,UV(255,0,layer)};
   WorldVertex c{x+radius,y+radius,z,color,UV(255,255,layer)},d{x-radius,y+radius,z,color,UV(0,255,layer)};
   *out++=a;*out++=b;*out++=c;*out++=a;*out++=c;*out++=d;
  };
  if(maskMode==3){
   uint32_t seed=0x93a71b2du;
   WorldVertex edge[12];
   for(int k=0;k<12;k++){
    seed=seed*1664525u+1013904223u;
    const float r=.44f*1.41421356f*(.65f+(seed&255)/255.0f*.35f);
    const float a=k*6.2831853f/12;
    edge[k]={-.45f+cosf(a)*r,.44f+sinf(a)*r,z,color};
   }
   for(int k=0;k<12;k++){*out++={-.45f,.44f,z,color};*out++=edge[k];*out++=edge[(k+1)%12];}
   stamp(.5f,.48f,.44f,1);stamp(.5f,-.48f,.44f,2);return 48;
  }
  if(maskMode>=4){
   constexpr unsigned edge[12][2]={{255,0},{255,85},{255,170},{255,255},{170,255},{85,255},{0,255},{0,170},{0,85},{0,0},{85,0},{170,0}};
   for(int j=0;j<64;j++)for(int i=0;i<64;i++){
    const float x=(i+.5f)/32-1,y=(j+.5f)/32-1;
    const unsigned layer=maskMode==4?0:1u+unsigned((i+j)%2);
    auto corner=[&](int k){
     const unsigned u=edge[k][0],v=edge[k][1];
     return WorldVertex{x+(u/255.f-.5f)/32,y+(v/255.f-.5f)/32,z,color,UV(u,v,layer)};
    };
    for(int k=0;k<12;k++){*out++={x,y,z,color,UV(128,128,layer)};*out++=corner(k);*out++=corner((k+1)%12);}
   }
   return Blood::MaxDecals*Blood::VerticesPerSplat;
  }
  stamp(0,0,.8f,maskMode);return 6;
 }
 out[0]={-.8f,-.8f,z,color};out[1]={.8f,-.8f,z,color};out[2]={0,.8f,z,color};return 3;
}
static int checks=0;
#define CHECK(x) do{++checks;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
int main(){
 ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
 CHECK(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)));
 D3D11_TEXTURE2D_DESC t{};t.Width=t.Height=512;t.MipLevels=t.ArraySize=1;t.Format=DXGI_FORMAT_R8G8B8A8_UNORM;t.SampleDesc.Count=1;t.BindFlags=D3D11_BIND_RENDER_TARGET;
 ComPtr<ID3D11Texture2D> target,depth,staging;ComPtr<ID3D11RenderTargetView> rtv;ComPtr<ID3D11DepthStencilView> dsv;
 CHECK(SUCCEEDED(device->CreateTexture2D(&t,nullptr,&target)));CHECK(SUCCEEDED(device->CreateRenderTargetView(target.Get(),nullptr,&rtv)));
 t.Format=DXGI_FORMAT_D32_FLOAT;t.BindFlags=D3D11_BIND_DEPTH_STENCIL;CHECK(SUCCEEDED(device->CreateTexture2D(&t,nullptr,&depth)));CHECK(SUCCEEDED(device->CreateDepthStencilView(depth.Get(),nullptr,&dsv)));
 t.Format=DXGI_FORMAT_R8G8B8A8_UNORM;t.BindFlags=0;t.Usage=D3D11_USAGE_STAGING;t.CPUAccessFlags=D3D11_CPU_ACCESS_READ;CHECK(SUCCEEDED(device->CreateTexture2D(&t,nullptr,&staging)));
 ID3D11RenderTargetView* view=rtv.Get();context->OMSetRenderTargets(1,&view,dsv.Get());D3D11_VIEWPORT viewport{0,0,512,512,0,1};context->RSSetViewports(1,&viewport);
 BloodRender::Projection p{};for(int i=0;i<4;i++)p.matrix[i*4+i]=1;p.scale[0]=1;p.scale[1]=-1;p.scale[2]=1;p.offset[0]=p.offset[1]=.5f;p.target[0]=p.target[1]=2;p.flags[0]=63;
 auto clear=[&](float d){float black[4]={0,0,0,1};context->ClearRenderTargetView(rtv.Get(),black);context->ClearDepthStencilView(dsv.Get(),D3D11_CLEAR_DEPTH,d,0);};
 auto pixel=[&](int x=256,int y=256){context->CopyResource(staging.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE m{};if(FAILED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&m)))return 0u;uint32_t c;memcpy(&c,(char*)m.pData+y*m.RowPitch+x*4,4);context->Unmap(staging.Get(),0);return c;};
 clear(.75f);CHECK(BloodRender::Draw(device.Get(),context.Get(),p));CHECK((pixel()&0xffffff)==0); // wall in front
 clear(.25f);CHECK(BloodRender::Draw(device.Get(),context.Get(),p));CHECK((pixel()&0xffffff)==0xff); // visible in front of wall
 color=0xff00ff00;z=.4f;CHECK(BloodRender::Draw(device.Get(),context.Get(),p));CHECK((pixel()&0xffffff)==0xff00); // blood did not write depth
 color=0xffff0000;z=.9f;CHECK(BloodRender::Draw(device.Get(),context.Get(),p));CHECK((pixel()&0xffffff)==0xff0000); // later foreground/viewmodel
 clear(.25f);color=0xff0000ff;z=.5f;CHECK(BloodRender::Draw(device.Get(),context.Get(),p));color=0x80ffffff;z=.8f;CHECK(BloodRender::Draw(device.Get(),context.Get(),p));CHECK((pixel()&0x00ffff00)!=0); // later translucent layer attenuates
 clear(.25f);color=0xff0000ff;p.matrix[3]=1.8f;CHECK(BloodRender::Draw(device.Get(),context.Get(),p));CHECK((pixel()&0xffffff)==0); // current frame projection moves geometry
 // Reproduce a world point in front of the camera clipped by a short-range matrix.
 p={};p.matrix[0]=p.matrix[5]=1;p.matrix[14]=1;
 p.matrix[10]=.05f/(.05f-.4f);p.matrix[11]=.05f*.4f/(.4f-.05f);
 p.scale[0]=1;p.scale[1]=-1;p.scale[2]=1;p.offset[0]=p.offset[1]=.5f;p.target[0]=p.target[1]=2;p.flags[0]=63;
 z=.5f;color=0xff0000ff;clear(.01f);CHECK(BloodRender::Draw(device.Get(),context.Get(),p));CHECK((pixel()&0xffffff)==0);
 CHECK(BloodRender::NearPlane(p.matrix)<1);
 p.matrix[10]=6.f/(6.f-8192.f);p.matrix[11]=6.f*8192.f/(8192.f-6.f);
 // Scale the same scene to game units: triangle at 350 units, six-unit near plane.
 z=350; p.matrix[0]=p.matrix[5]=350;
 CHECK(BloodRender::NearPlane(p.matrix)>=1);clear(.01f);CHECK(BloodRender::Draw(device.Get(),context.Get(),p));CHECK((pixel()&0xffffff)==0xff);
 clear(.25f);show=false;p.matrix[3]=0;CHECK(!BloodRender::Draw(device.Get(),context.Get(),p));CHECK((pixel()&0xffffff)==0);
 show=true;z=.5f;color=0xff0000ff;
 p={};for(int i=0;i<4;i++)p.matrix[i*4+i]=1;
 p.scale[0]=1;p.scale[1]=-1;p.scale[2]=1;p.offset[0]=p.offset[1]=.5f;p.target[0]=p.target[1]=2;p.flags[0]=63;
 maskMode=1;clear(.25f);CHECK(BloodRender::Draw(device.Get(),context.Get(),p));
 const auto splatter=pixel()&255u;
 CHECK(splatter>50&&splatter<250);
 CHECK((pixel(75,75)&0xffffff)==0); // Transparent corner inside the submitted square.
 clear(.75f);CHECK(BloodRender::Draw(device.Get(),context.Get(),p));CHECK((pixel()&0xffffff)==0);
 maskMode=2;clear(.25f);CHECK(BloodRender::Draw(device.Get(),context.Get(),p));
 CHECK((pixel()&255u)>0);
 CHECK((pixel(256,330)&255u)<(pixel()&255u)); // Narrow across the streak.
 for(int layer=0;layer<BloodMask::Layers;layer++){
  int clearTexels=0,partialTexels=0;
  for(int y=0;y<BloodMask::Side;y++)for(int x=0;x<BloodMask::Side;x++){
   const auto a=BloodMask::Texel(layer,x,y);
   clearTexels+=a==0;partialTexels+=a>0&&a<240;
  }
  CHECK(clearTexels>1500&&partialTexels>400);
 }
 if(const char* preview=std::getenv("COD3_BLOOD_MASK_PREVIEW")){
  // Optional screenshot of the production shader output. Top left is an
  // Classic outline, top right splatter, bottom right streak.
  float gray[4]={.36f,.34f,.31f,1};
  context->ClearRenderTargetView(rtv.Get(),gray);
  context->ClearDepthStencilView(dsv.Get(),D3D11_CLEAR_DEPTH,.25f,0);
  color=0xb803054c;maskMode=3;
  CHECK(BloodRender::Draw(device.Get(),context.Get(),p));
  context->CopyResource(staging.Get(),target.Get());
  D3D11_MAPPED_SUBRESOURCE m{};
  CHECK(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&m)));
  FILE* f=fopen(preview,"wb");CHECK(f);
  fprintf(f,"P6\n512 512\n255\n");
  for(int y=0;y<512;y++)for(int x=0;x<512;x++)fwrite(static_cast<const char*>(m.pData)+y*m.RowPitch+x*4,1,3,f);
  fclose(f);context->Unmap(staging.Get(),0);
 }
 if(std::getenv("COD3_BLOOD_MASK_PREVIEW")){
  // Tiled, no-overdraw WARP comparison. Includes vertex staging, draw and
  // synchronized readback, so this is a stress check, not a gameplay FPS test.
  for(unsigned mode:{4u,5u}){
   maskMode=mode;clear(.25f);
   CHECK(BloodRender::Draw(device.Get(),context.Get(),p));pixel();
   const auto start=std::chrono::steady_clock::now();
   for(int i=0;i<12;i++){clear(.25f);CHECK(BloodRender::Draw(device.Get(),context.Get(),p));pixel();}
   printf("WARP full-pool %s: %.2f ms/frame (draw + synchronized readback)\n",mode==4?"unmasked":"masked",
     std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/12);
  }
 }
 printf("PASS %d GPU checks: real shader, reversed-depth occlusion, no depth writes, later foreground/transparency, frame matrix and hidden batch.\n",checks);
}



