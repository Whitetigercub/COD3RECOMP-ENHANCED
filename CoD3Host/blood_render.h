#pragma once
#include <cstdint>
#include "blood.h"
struct ID3D11Device;struct ID3D11DeviceContext;
namespace BloodRender {
struct Projection {float matrix[16],scale[4],offset[4],target[4];uint32_t flags[4]; uint64_t diagShader=0,diagFrame=0; uint32_t diagDepth=0,diagMask=0,diagPass=0,diagPad=0; float diagNear=0,diagFar=0,diagPad2[2]{};};
bool Draw(ID3D11Device* device,ID3D11DeviceContext* context,const Projection& projection,const Blood::WorldVertex* fixture=nullptr,int fixtureCount=0,bool disableDepth=false,bool colorWrites=true,bool reportFixture=true);
}
