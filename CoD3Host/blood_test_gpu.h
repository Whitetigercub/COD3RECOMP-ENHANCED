#pragma once
#include "blood_render.h"
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct ID3D11ShaderResourceView;
struct D3D11_VIEWPORT;
namespace BloodTestGpu {
struct Frame {
  ID3D11ShaderResourceView *resource = nullptr;
  unsigned width = 0, height = 0;
  float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
};
struct Comparison {
  Frame normal, noDepth;
  uint64_t normalSamples = 0, noDepthSamples = 0;
  bool drawn = false, queriesReady = false;
};
// Diagnostic replay on private color/depth copies. Results remain valid until
// the next Compare call. Caller restores bindings/viewport after use.
Comparison Compare(ID3D11Device *, ID3D11DeviceContext *,
                   ID3D11RenderTargetView *, ID3D11DepthStencilView *,
                   const BloodRender::Projection &, const Blood::WorldVertex *,
                   int, const D3D11_VIEWPORT &);
void Observe(ID3D11Device *, ID3D11DeviceContext *, ID3D11RenderTargetView *,
             ID3D11DepthStencilView *, const BloodRender::Projection &,
             unsigned, unsigned);
Frame FrozenFrame(ID3D11Device *, ID3D11DeviceContext *);
void Reset();
} // namespace BloodTestGpu
