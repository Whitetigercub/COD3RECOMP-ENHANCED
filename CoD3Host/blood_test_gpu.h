#pragma once
#include "blood_render.h"
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct ID3D11ShaderResourceView;
namespace BloodTestGpu {
struct Frame {
  ID3D11ShaderResourceView *resource = nullptr;
  unsigned width = 0, height = 0;
  float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};
void Observe(ID3D11Device *, ID3D11DeviceContext *, ID3D11RenderTargetView *,
             ID3D11DepthStencilView *, const BloodRender::Projection &,
             unsigned, unsigned);
Frame FrozenFrame(ID3D11Device *, ID3D11DeviceContext *);
void Reset();
} // namespace BloodTestGpu
