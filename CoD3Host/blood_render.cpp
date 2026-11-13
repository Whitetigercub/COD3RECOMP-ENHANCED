#include "blood_diag.h"
extern void BloodDiagnosticEye(float* eye);
#include "blood_render.h"
#include "blood.h"
#include "blood_mask.h"
#ifndef COD3_MP
#include "enemy_wounds.h"
#endif
#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <array>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
using Microsoft::WRL::ComPtr;
namespace {
ComPtr<ID3D11VertexShader> vs;ComPtr<ID3D11PixelShader> ps;
ComPtr<ID3D11Buffer> vertices,constants;ComPtr<ID3D11InputLayout> layout;
ComPtr<ID3D11BlendState> blend;ComPtr<ID3D11DepthStencilState> depth;ComPtr<ID3D11RasterizerState> raster;
ComPtr<ID3D11ShaderResourceView> masks;
ComPtr<ID3D11SamplerState> maskSampler;
#ifdef COD3_MP
constexpr int Capacity=Blood::MaxVertices;
#else
constexpr int Capacity=Blood::MaxVertices+EnemyWounds::MaxVertices;
#endif

bool CreateMasks(ID3D11Device* device){
 using namespace BloodMask;
 // Immutable R8 masks with box-filtered mips for distant stains.
 std::array<std::array<uint8_t,Side*Side*4/3+1>,Layers> pixels{};
 std::array<D3D11_SUBRESOURCE_DATA,Layers*Mips> data{};
 for(int layer=0;layer<Layers;layer++){
  auto& image=pixels[layer];
  for(int y=0;y<Side;y++)for(int x=0;x<Side;x++)image[y*Side+x]=Texel(layer,x,y);
  int previous=0,offset=0;
  for(int mip=0,side=Side;mip<Mips;mip++,side/=2){
   data[layer*Mips+mip]={image.data()+offset,UINT(side),0};
   if(mip)for(int y=0;y<side;y++)for(int x=0;x<side;x++){
    const int i=previous+4*y*side+2*x;
    image[offset+y*side+x]=uint8_t((image[i]+image[i+1]+image[i+2*side]+image[i+2*side+1]+2)/4);
   }
   previous=offset;offset+=side*side;
  }
 }
 D3D11_TEXTURE2D_DESC desc{};
 desc.Width=desc.Height=Side;desc.MipLevels=Mips;desc.ArraySize=Layers;
 desc.Format=DXGI_FORMAT_R8_UNORM;desc.SampleDesc.Count=1;
 desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
 ComPtr<ID3D11Texture2D> texture;
 if(FAILED(device->CreateTexture2D(&desc,data.data(),&texture))||
    FAILED(device->CreateShaderResourceView(texture.Get(),nullptr,&masks)))return false;
 D3D11_SAMPLER_DESC sampler{};
 sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
 sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
 sampler.MaxAnisotropy=1;sampler.ComparisonFunc=D3D11_COMPARISON_NEVER;
 sampler.MaxLOD=D3D11_FLOAT32_MAX;
 return SUCCEEDED(device->CreateSamplerState(&sampler,&maskSampler));
}
bool Initialize(ID3D11Device* device){
 if(vs)return true;
 const char* source=R"(
 cbuffer Projection:register(b0){float4 rows[4];float4 scale;float4 offset;float4 target;uint4 flags;};
 Texture2DArray<float> masks:register(t15);
 SamplerState maskSampler:register(s15);
 struct V{float3 p:POSITION;float4 c:COLOR;uint mask:TEXCOORD0;};
 struct O{float4 p:SV_Position;float4 c:COLOR;float2 uv:TEXCOORD0;nointerpolation uint layer:TEXCOORD1;};
 O VS(V v){O o;float4 w=float4(v.p,1);float4 p=float4(dot(rows[0],w),dot(rows[1],w),dot(rows[2],w),dot(rows[3],w));
 uint f=flags.x;float ww=(f&256)?1:p.w;float xs=(f&1)?scale.x:1;float xo=(f&2)?offset.x:0;
 float ys=(f&4)?scale.y:1;float yo=(f&8)?offset.y:0;float zs=(f&16)?scale.z:1;float zo=(f&32)?offset.z:0;
 float centre=(f&65536)?0:.5;
 float z=(f&512)?(p.z*zs+zo)*ww:(p.z*zs+zo*ww);
 o.p=float4(p.x*(2*xs/target.x)+ww*(2*(xo+centre)/target.x-1),-(p.y*(2*ys/target.y)+ww*(2*(yo+centre)/target.y-1)),z,ww);o.c=v.c;
 uint2 uv=uint2(v.mask&255,(v.mask>>8)&255);
 o.uv=all(uv==128)?float2(.5,.5):float2(uv)/255;
 o.layer=v.mask>>16;return o;}
 float4 PS(O v):SV_Target{
  [branch] if(v.layer!=0) {
   float coverage=masks.Sample(maskSampler,float3(v.uv,v.layer-1));
   // The dense center of an enemy wound reads darker than its thin fringe.
   if(v.layer==3) v.c.rgb*=lerp(1.0,.44,smoothstep(.42,.88,coverage));
   v.c.a*=coverage;
  }
  return v.c;
 }
 )";
 ComPtr<ID3DBlob> vb,pb,error;
 if(FAILED(D3DCompile(source,strlen(source),"blood world pass",nullptr,nullptr,"VS","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vb,&error))||
 FAILED(D3DCompile(source,strlen(source),"blood world pass",nullptr,nullptr,"PS","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&pb,&error))){printf("blood: world shader compilation failed\n");return false;}
 D3D11_INPUT_ELEMENT_DESC elements[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"COLOR",0,DXGI_FORMAT_R8G8B8A8_UNORM,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32_UINT,0,16,D3D11_INPUT_PER_VERTEX_DATA,0}};
 ComPtr<ID3D11VertexShader> newVs;
 if(FAILED(device->CreateVertexShader(vb->GetBufferPointer(),vb->GetBufferSize(),nullptr,&newVs))||FAILED(device->CreatePixelShader(pb->GetBufferPointer(),pb->GetBufferSize(),nullptr,&ps))||FAILED(device->CreateInputLayout(elements,3,vb->GetBufferPointer(),vb->GetBufferSize(),&layout)))return false;
 D3D11_BUFFER_DESC b{};b.ByteWidth=Capacity*sizeof(Blood::WorldVertex);b.Usage=D3D11_USAGE_DYNAMIC;b.BindFlags=D3D11_BIND_VERTEX_BUFFER;b.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
 if(FAILED(device->CreateBuffer(&b,nullptr,&vertices)))return false;
 b.ByteWidth=sizeof(BloodRender::Projection);b.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
 if(FAILED(device->CreateBuffer(&b,nullptr,&constants)))return false;
 D3D11_BLEND_DESC blendDesc{};auto& rt=blendDesc.RenderTarget[0];rt.BlendEnable=true;rt.SrcBlend=D3D11_BLEND_SRC_ALPHA;rt.DestBlend=D3D11_BLEND_INV_SRC_ALPHA;rt.BlendOp=D3D11_BLEND_OP_ADD;rt.SrcBlendAlpha=D3D11_BLEND_ONE;rt.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;rt.BlendOpAlpha=D3D11_BLEND_OP_ADD;rt.RenderTargetWriteMask=15;
 D3D11_DEPTH_STENCIL_DESC ds{};ds.DepthEnable=true;ds.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;ds.DepthFunc=D3D11_COMPARISON_GREATER_EQUAL;
 D3D11_RASTERIZER_DESC rs{};rs.FillMode=D3D11_FILL_SOLID;rs.CullMode=D3D11_CULL_NONE;rs.DepthClipEnable=true;rs.MultisampleEnable=true;rs.DepthBias=32;rs.SlopeScaledDepthBias=1.0f;rs.DepthBiasClamp=.00002f;
 if(FAILED(device->CreateBlendState(&blendDesc,&blend))||FAILED(device->CreateDepthStencilState(&ds,&depth))||FAILED(device->CreateRasterizerState(&rs,&raster)))return false;
 if(!CreateMasks(device))return false;
 vs=newVs;return true;
}
}
bool BloodRender::Draw(ID3D11Device* device,ID3D11DeviceContext* context,const Projection& projection){
 static thread_local std::array<Blood::WorldVertex,Capacity> data;
 int count=Blood::BuildWorld(data.data(),Blood::MaxVertices);
#ifndef COD3_MP
 count+=EnemyWounds::Append(data.data()+count,Capacity-count);
#endif
 if(!count||!Initialize(device))return false;
 D3D11_MAPPED_SUBRESOURCE map{};
 if(FAILED(context->Map(vertices.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&map)))return false;
 memcpy(map.pData,data.data(),count*sizeof(data[0]));context->Unmap(vertices.Get(),0);
 if(FAILED(context->Map(constants.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&map)))return false;
 memcpy(map.pData,&projection,sizeof(projection));context->Unmap(constants.Get(),0);
 ID3D11Buffer* cb=constants.Get();context->VSSetConstantBuffers(0,1,&cb);
 UINT stride=sizeof(Blood::WorldVertex),offset=0;ID3D11Buffer* buffer=vertices.Get();context->IASetVertexBuffers(0,1,&buffer,&stride,&offset);
 context->IASetInputLayout(layout.Get());context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
 context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);context->GSSetShader(nullptr,nullptr,0);
 ComPtr<ID3D11ShaderResourceView> previousMask;
 ComPtr<ID3D11SamplerState> previousSampler;
 context->PSGetShaderResources(15,1,&previousMask);
 context->PSGetSamplers(15,1,&previousSampler);
 ID3D11ShaderResourceView* maskView=masks.Get();
 ID3D11SamplerState* sampler=maskSampler.Get();
 context->PSSetShaderResources(15,1,&maskView);
 context->PSSetSamplers(15,1,&sampler);
 context->RSSetState(raster.Get());context->OMSetBlendState(blend.Get(),nullptr,0xffffffff);context->OMSetDepthStencilState(depth.Get(),0);
 static std::array<ComPtr<ID3D11Query>,4> diagQueries;static bool pending[4]{};static unsigned nextQuery=0;int query=-1;
 static std::array<ComPtr<ID3D11Query>,8> decalQueries;static bool decalPending[8]{};static uint64_t lastDecalProbe=0;
 if(BloodDiag::On()){
  for(int i=0;i<4;i++)if(pending[i]){UINT64 samples=0;if(context->GetData(diagQueries[i].Get(),&samples,sizeof(samples),D3D11_ASYNC_GETDATA_DONOTFLUSH)==S_OK){static int reports=0;if(reports++%30==0)BloodDiag::Log("GPU depth/clipping visible_samples=%llu",samples);pending[i]=false;}}
  for(int i=0;i<8;i++)if(decalPending[i]){UINT64 samples=0;if(context->GetData(decalQueries[i].Get(),&samples,sizeof(samples),D3D11_ASYNC_GETDATA_DONOTFLUSH)==S_OK){BloodDiag::Log("decal_probe slot=%d visible_samples=%llu",i,samples);decalPending[i]=false;}}
  unsigned i=nextQuery++%4;if(!pending[i]){if(!diagQueries[i]){D3D11_QUERY_DESC q{D3D11_QUERY_OCCLUSION,0};device->CreateQuery(&q,&diagQueries[i]);}if(diagQueries[i]){query=int(i);context->Begin(diagQueries[i].Get());}}
 }
 bool probed=false;
 if(BloodDiag::On()&&GetTickCount64()-lastDecalProbe>=500&&count>=36){
  lastDecalProbe=GetTickCount64();probed=true;int probeCount=std::min(8,count/36);
  for(int i=0;i<probeCount;i++){
   BloodDiag::Log("decal_probe_position slot=%d world=%.3f,%.3f,%.3f",i,data[i*36].x,data[i*36].y,data[i*36].z);
   if(!decalPending[i]){if(!decalQueries[i]){D3D11_QUERY_DESC q{D3D11_QUERY_OCCLUSION,0};device->CreateQuery(&q,&decalQueries[i]);}if(decalQueries[i]){context->Begin(decalQueries[i].Get());context->Draw(36,i*36);context->End(decalQueries[i].Get());decalPending[i]=true;continue;}}
   context->Draw(36,i*36);
  }
  if(count>probeCount*36)context->Draw(count-probeCount*36,probeCount*36);
 }
 if(!probed)context->Draw(count,0);
 if(query>=0){context->End(diagQueries[query].Get());pending[query]=true;}
 if(BloodDiag::On()){
 static int report=0;if(report++%30==0){float c[4]{};for(int r=0;r<4;r++)c[r]=projection.matrix[r*4]*data[0].x+projection.matrix[r*4+1]*data[0].y+projection.matrix[r*4+2]*data[0].z+projection.matrix[r*4+3];float eye[3]{};BloodDiagnosticEye(eye);float eyeW=projection.matrix[12]*eye[0]+projection.matrix[13]*eye[1]+projection.matrix[14]*eye[2]+projection.matrix[15];BloodDiag::Log("GPU submission eye_w=%.4f vertices=%d first_world=%.4f,%.4f,%.4f clip=%.4f,%.4f,%.4f,%.4f",eyeW,count,data[0].x,data[0].y,data[0].z,c[0],c[1],c[2],c[3]);}
 }
 static const bool trace=getenv("COD3_BLOOD_TRACE")!=nullptr;static int reports=0;
 if(trace && reports++<12){float clip[4]{};for(int row=0;row<4;row++)clip[row]=projection.matrix[row*4]*data[0].x+projection.matrix[row*4+1]*data[0].y+projection.matrix[row*4+2]*data[0].z+projection.matrix[row*4+3];printf("blood: world batch %d point %.1f %.1f %.1f clip %.2f %.2f %.2f %.2f\n",count,data[0].x,data[0].y,data[0].z,clip[0],clip[1],clip[2],clip[3]);}
// Diagnostic-only draws: zero color/depth writes, same geometry and viewport.
if(BloodDiag::On()){
 static ComPtr<ID3D11BlendState> mute;
 static ComPtr<ID3D11DepthStencilState> noDepth;
 static std::array<ComPtr<ID3D11Query>,4> q;
 static bool waiting[4]{};static uint64_t probeFrame[4]{};static uint64_t last=0;
 if(!mute){D3D11_BLEND_DESC b{};device->CreateBlendState(&b,&mute);D3D11_DEPTH_STENCIL_DESC d{};d.DepthEnable=false;d.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;d.DepthFunc=D3D11_COMPARISON_ALWAYS;device->CreateDepthStencilState(&d,&noDepth);}
 for(int i=0;i<4;i++)if(waiting[i]){UINT64 samples=0;if(context->GetData(q[i].Get(),&samples,sizeof(samples),D3D11_ASYNC_GETDATA_DONOTFLUSH)==S_OK){BloodDiag::Log("isolate frame=%llu probe=%d samples=%llu (0=no_depth,1=original,2=depth_plus_0.0001,3=depth_plus_0.001)",probeFrame[i],i,samples);waiting[i]=false;}}
 uint64_t now=GetTickCount64();
 if(now-last>=1000 && mute && noDepth){last=now;
  BloodDiag::Log("isolate viewport flags=%X scale=%.6f,%.6f,%.6f offset=%.6f,%.6f,%.6f native_bias=%.8f",projection.flags[0],projection.scale[0],projection.scale[1],projection.scale[2],projection.offset[0],projection.offset[1],projection.offset[2],projection.offset[3]);
  for(int k=0;k<std::min(8,count/36);k++){
   auto v=data[k*36];float c[4]{};for(int j=0;j<4;j++)c[j]=projection.matrix[4*j]*v.x+projection.matrix[4*j+1]*v.y+projection.matrix[4*j+2]*v.z+projection.matrix[4*j+3];
   auto& p=projection;auto f=p.flags[0];float w=(f&256)?1:c[3];float z=(f&512)?(c[2]*((f&16)?p.scale[2]:1)+((f&32)?p.offset[2]:0))*w:c[2]*((f&16)?p.scale[2]:1)+((f&32)?p.offset[2]:0)*w;
   float centre=(f&65536)?0:.5f;
   float x=c[0]*(2*((f&1)?p.scale[0]:1)/p.target[0])+w*(2*(((f&2)?p.offset[0]:0)+centre)/p.target[0]-1);
   float y=-(c[1]*(2*((f&4)?p.scale[1]:1)/p.target[1])+w*(2*(((f&8)?p.offset[1]:0)+centre)/p.target[1]-1));
   const char* reason=!std::isfinite(z)||!std::isfinite(w)?"nonfinite":w<=0?"behind_camera":z<0?"depth_below_zero":z>w?"depth_above_one":fabsf(x)>w||fabsf(y)>w?"outside_xy":"inside_clip";
   BloodDiag::Log("projected frame=%llu decal_slot=%d vertices=%d world=%.4f,%.4f,%.4f clip=%.6f,%.6f,%.6f,%.6f ndc_depth=%.8f reason=%s",p.diagFrame,k,count,v.x,v.y,v.z,x,y,z,w,w!=0?z/w:0,reason);
  }
  context->OMSetBlendState(mute.Get(),nullptr,0xffffffff);
  for(int i=0;i<4;i++)if(!waiting[i]){
   if(!q[i]){D3D11_QUERY_DESC d{D3D11_QUERY_OCCLUSION,0};device->CreateQuery(&d,&q[i]);}if(!q[i])continue;
   auto p=projection;
   if(i>=2 && (p.flags[0]&16) && !(p.flags[0]&512) && p.scale[2]!=0){float bias=i==2?.0001f:.001f;for(int k=0;k<4;k++)p.matrix[8+k]+=p.matrix[12+k]*(bias/p.scale[2]);}
   D3D11_MAPPED_SUBRESOURCE m{};if(FAILED(context->Map(constants.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&m)))continue;
   memcpy(m.pData,&p,sizeof(p));context->Unmap(constants.Get(),0);
   context->OMSetDepthStencilState(i==0?noDepth.Get():depth.Get(),0);
   context->Begin(q[i].Get());context->Draw(count,0);context->End(q[i].Get());waiting[i]=true;probeFrame[i]=projection.diagFrame;
  }
  context->OMSetBlendState(blend.Get(),nullptr,0xffffffff);context->OMSetDepthStencilState(depth.Get(),0);
 }
}

 maskView=previousMask.Get();sampler=previousSampler.Get();
 context->PSSetShaderResources(15,1,&maskView);
 context->PSSetSamplers(15,1,&sampler);
 // Native shaders fetch vertices through SRVs and use no input layout.
 context->IASetInputLayout(nullptr);buffer=nullptr;context->IASetVertexBuffers(0,1,&buffer,&stride,&offset);
 return true;
}

