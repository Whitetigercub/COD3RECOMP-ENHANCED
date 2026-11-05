#include "blood_test_gpu.h"
#include "blood_test_mode.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <d3d11.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
namespace BloodTestGpu {
namespace {
ComPtr<ID3D11Texture2D> sourceColor, sourceDepth, baseline, color, depth,
    resolved;
ComPtr<ID3D11RenderTargetView> sourceRtv, rtv;
ComPtr<ID3D11DepthStencilView> sourceDsv, dsv;
ComPtr<ID3D11ShaderResourceView> srv;
BloodRender::Projection observed{}, projection{};
unsigned observedW = 0, observedH = 0, width = 0, height = 0;
uint64_t revision = ~uint64_t(0);
bool ready = false, pending = false;
ComPtr<ID3D11Query> validation[4];
std::array<Blood::WorldVertex, 36> vertices;
bool Texture(ID3D11Device *device, ID3D11Texture2D *src, UINT bind,
             ComPtr<ID3D11Texture2D> &dst, bool single = false) {
  D3D11_TEXTURE2D_DESC d{};
  src->GetDesc(&d);
  d.Usage = D3D11_USAGE_DEFAULT;
  d.BindFlags = bind;
  d.CPUAccessFlags = 0;
  d.MiscFlags = 0;
  if (single) {
    d.SampleDesc.Count = 1;
    d.SampleDesc.Quality = 0;
  }
  dst.Reset();
  HRESULT result = device->CreateTexture2D(&d, nullptr, &dst);
  if (FAILED(result)) {
    char message[128];
    snprintf(
        message, sizeof(message),
        "Snapshot allocation failed %08X fmt=%u bind=%u MSAA=%u. Use LIVE.",
        unsigned(result), unsigned(d.Format), bind, d.SampleDesc.Count);
    BloodTest::Status(message);
  }
  return SUCCEEDED(result);
}
bool Capture(ID3D11Device *device, ID3D11DeviceContext *ctx) {
  if (!sourceColor || !sourceDepth || !sourceDsv || !observedW || !observedH) {
    BloodTest::Status("Waiting for a native scene with depth and projection.");
    return false;
  }
  bool finite = true;
  for (float v : observed.matrix)
    finite &= std::isfinite(v);
  for (float v : observed.scale)
    finite &= std::isfinite(v);
  if (!finite || observed.target[0] <= 0 || observed.target[1] <= 0 ||
      (observed.flags[0] & 256)) {
    BloodTest::Status(
        "Snapshot rejected: invalid perspective/viewport constants. Use LIVE.");
    return false;
  }
  D3D11_TEXTURE2D_DESC c{}, z{};
  sourceColor->GetDesc(&c);
  sourceDepth->GetDesc(&z);
  if (c.Width != z.Width || c.Height != z.Height ||
      c.SampleDesc.Count != z.SampleDesc.Count) {
    BloodTest::Status("Snapshot rejected: native color/depth dimensions or "
                      "MSAA differ. Use LIVE.");
    return false;
  }
  if (!Texture(device, sourceColor.Get(), D3D11_BIND_RENDER_TARGET, baseline) ||
      !Texture(device, sourceColor.Get(), D3D11_BIND_RENDER_TARGET, color) ||
      !Texture(device, sourceDepth.Get(), D3D11_BIND_DEPTH_STENCIL, depth) ||
      !Texture(device, sourceColor.Get(), D3D11_BIND_SHADER_RESOURCE, resolved,
               true))
    return false;
  rtv.Reset();
  dsv.Reset();
  srv.Reset();
  D3D11_DEPTH_STENCIL_VIEW_DESC dv{};
  sourceDsv->GetDesc(&dv);
  HRESULT hr = device->CreateRenderTargetView(color.Get(), nullptr, &rtv);
  if (SUCCEEDED(hr))
    hr = device->CreateDepthStencilView(depth.Get(), &dv, &dsv);
  if (SUCCEEDED(hr))
    hr = device->CreateShaderResourceView(resolved.Get(), nullptr, &srv);
  if (FAILED(hr)) {
    char message[128];
    snprintf(message, sizeof(message),
             "Snapshot view creation failed %08X; depth format %u. Use LIVE.",
             unsigned(hr), unsigned(dv.Format));
    BloodTest::Status(message);
    return false;
  }
  ctx->CopyResource(baseline.Get(), sourceColor.Get());
  ctx->CopyResource(depth.Get(), sourceDepth.Get());
  projection = observed;
  width = observedW;
  height = observedH;
  int count = BloodTest::Build(vertices.data(), int(vertices.size()));
  if (count) {
    for (int i = 0; i < 4; i++) {
      validation[i].Reset();
      D3D11_QUERY_DESC q{D3D11_QUERY_OCCLUSION, 0};
      if (FAILED(device->CreateQuery(&q, &validation[i])))
        return false;
    }
    D3D11_VIEWPORT vp{0, 0, float(width), float(height), 0, 1};
    ctx->RSSetViewports(1, &vp);
    for (int i = 0; i < 4; i++) {
      ID3D11RenderTargetView *target = i < 2 ? sourceRtv.Get() : rtv.Get();
      ctx->OMSetRenderTargets(1, &target, i < 2 ? sourceDsv.Get() : dsv.Get());
      ctx->Begin(validation[i].Get());
      BloodRender::Draw(device, ctx, projection, vertices.data(), count,
                        (i & 1) != 0, false);
      ctx->End(validation[i].Get());
    }
    pending = true;
  }
  BloodTest::SnapshotReady(true, width, height, projection.diagNear,
                           projection.diagFar, projection.diagFrame);
  BloodTest::Status(
      "Frozen native color/depth/projection copied; checking GPU parity.");
  return true;
}
} // namespace
Comparison Compare(ID3D11Device *device, ID3D11DeviceContext *ctx,
                   ID3D11RenderTargetView *target, ID3D11DepthStencilView *z,
                   const BloodRender::Projection &p,
                   const Blood::WorldVertex *fixture, int count,
                   const D3D11_VIEWPORT &viewport) {
  Comparison result;
  if (!BloodTest::Active() || !target || !z || !fixture || count <= 0)
    return result;
  // These resources are never presented or substituted for native targets.
  static ComPtr<ID3D11Texture2D> outputs[2];
  static ComPtr<ID3D11ShaderResourceView> views[2];
  ComPtr<ID3D11Resource> resource;
  ComPtr<ID3D11Texture2D> nativeColor, nativeDepth, scratch, scratchDepth;
  target->GetResource(&resource);
  resource.As(&nativeColor);
  z->GetResource(resource.ReleaseAndGetAddressOf());
  resource.As(&nativeDepth);
  D3D11_TEXTURE2D_DESC c{}, d{};
  if (!nativeColor || !nativeDepth)
    return result;
  nativeColor->GetDesc(&c);
  nativeDepth->GetDesc(&d);
  if (c.Width != d.Width || c.Height != d.Height ||
      c.SampleDesc.Count != d.SampleDesc.Count ||
      !Texture(device, nativeColor.Get(), D3D11_BIND_RENDER_TARGET, scratch) ||
      !Texture(device, nativeDepth.Get(), D3D11_BIND_DEPTH_STENCIL, scratchDepth))
    return result;
  ComPtr<ID3D11RenderTargetView> scratchRtv;
  ComPtr<ID3D11DepthStencilView> scratchDsv;
  D3D11_RENDER_TARGET_VIEW_DESC cv{};
  D3D11_DEPTH_STENCIL_VIEW_DESC dv{};
  target->GetDesc(&cv);
  z->GetDesc(&dv);
  if (FAILED(device->CreateRenderTargetView(scratch.Get(), &cv, &scratchRtv)) ||
      FAILED(device->CreateDepthStencilView(scratchDepth.Get(), &dv, &scratchDsv)))
    return result;
  ctx->CopyResource(scratchDepth.Get(), nativeDepth.Get());
  ComPtr<ID3D11Query> queries[2];
  bool ok = true;
  for (int i = 0; i < 2; ++i) {
    outputs[i].Reset();
    views[i].Reset();
    D3D11_QUERY_DESC q{D3D11_QUERY_OCCLUSION, 0};
    if (!Texture(device, nativeColor.Get(), D3D11_BIND_SHADER_RESOURCE,
                 outputs[i], true) ||
        FAILED(device->CreateShaderResourceView(outputs[i].Get(), nullptr, &views[i])) ||
        FAILED(device->CreateQuery(&q, &queries[i])))
      return result;
    ID3D11RenderTargetView *none = nullptr;
    ctx->OMSetRenderTargets(1, &none, nullptr);
    ctx->CopyResource(scratch.Get(), nativeColor.Get());
    ID3D11RenderTargetView *rt = scratchRtv.Get();
    ctx->OMSetRenderTargets(1, &rt, scratchDsv.Get());
    ctx->RSSetViewports(1, &viewport);
    ctx->Begin(queries[i].Get());
    ok &= BloodRender::Draw(device, ctx, p, fixture, count, i != 0, true, false);
    ctx->End(queries[i].Get());
    ctx->OMSetRenderTargets(1, &none, nullptr);
    if (c.SampleDesc.Count > 1)
      ctx->ResolveSubresource(outputs[i].Get(), 0, scratch.Get(), 0, cv.Format);
    else
      ctx->CopyResource(outputs[i].Get(), scratch.Get());
  }
  // Rare, explicitly armed diagnostic frames may wait for their own two
  // queries. Never pair results with a later frame's geometry or camera.
  ctx->Flush();
  UINT64 samples[2]{};
  const uint64_t until = GetTickCount64() + 500;
  bool complete = false;
  do {
    complete = ctx->GetData(queries[0].Get(), &samples[0], sizeof(UINT64),
                            D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK;
    complete &= ctx->GetData(queries[1].Get(), &samples[1], sizeof(UINT64),
                             D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK;
    if (!complete) Sleep(0);
  } while (!complete && GetTickCount64() < until);
  result.normal = {views[0].Get(), c.Width, c.Height};
  result.noDepth = {views[1].Get(), c.Width, c.Height};
  result.normalSamples = samples[0];
  result.noDepthSamples = samples[1];
  result.drawn = ok;
  result.queriesReady = complete;
  return result;
}
void Reset() {
  sourceColor.Reset();
  sourceDepth.Reset();
  sourceRtv.Reset();
  sourceDsv.Reset();
  baseline.Reset();
  color.Reset();
  depth.Reset();
  resolved.Reset();
  rtv.Reset();
  dsv.Reset();
  srv.Reset();
  ready = pending = false;
  revision = ~uint64_t(0);
  BloodTest::SnapshotReady(false);
}
void Observe(ID3D11Device *device, ID3D11DeviceContext *ctx,
             ID3D11RenderTargetView *target, ID3D11DepthStencilView *z,
             const BloodRender::Projection &p, unsigned w, unsigned h) {
  if (!BloodTest::Active() || !target || !z)
    return;
  BloodTest::SceneSeen();
  ComPtr<ID3D11Resource> resource;
  target->GetResource(&resource);
  resource.As(&sourceColor);
  resource.Reset();
  z->GetResource(&resource);
  resource.As(&sourceDepth);
  sourceRtv = target;
  sourceDsv = z;
  observed = p;
  observedW = w;
  observedH = h;
}
Frame FrozenFrame(ID3D11Device *device, ID3D11DeviceContext *ctx) {
  auto state = BloodTest::Get();
  if (!BloodTest::Active() || !state.frozen || !state.anchor) {
    if (ready)
      Reset();
    return {};
  }
  if (!ready || revision != state.revision) {
    ready = false;
    pending = false;
    revision = state.revision;
    ready = Capture(device, ctx);
    if (!ready)
      return {};
  }
  if (pending) {
    UINT64 v[4]{};
    bool complete = true;
    for (int i = 0; i < 4; i++)
      complete &= ctx->GetData(validation[i].Get(), &v[i], sizeof(v[i]),
                               D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK;
    if (complete) {
      pending = false;
      BloodTest::Validation(v[0], v[1], v[2], v[3]);
    }
  }
  ctx->CopyResource(color.Get(), baseline.Get());
  int count = BloodTest::Build(vertices.data(), int(vertices.size()));
  ID3D11RenderTargetView *target = rtv.Get();
  ctx->OMSetRenderTargets(1, &target, dsv.Get());
  D3D11_VIEWPORT vp{0, 0, float(width), float(height), 0, 1};
  ctx->RSSetViewports(1, &vp);
  if (count)
    BloodRender::Draw(device, ctx, projection, vertices.data(), count,
                      state.depthOff);
  else
    BloodTest::Submitted(projection.diagFrame, 0, false);
  ID3D11RenderTargetView *none = nullptr;
  ctx->OMSetRenderTargets(1, &none, nullptr);
  D3D11_TEXTURE2D_DESC desc{};
  color->GetDesc(&desc);
  if (desc.SampleDesc.Count > 1)
    ctx->ResolveSubresource(resolved.Get(), 0, color.Get(), 0, desc.Format);
  else
    ctx->CopyResource(resolved.Get(), color.Get());
  Frame out;
  out.resource = srv.Get();
  out.width = desc.Width;
  out.height = desc.Height;
  // Retain the native viewport's active rectangle, not unused EDRAM padding.
  float sx = float(width) / projection.target[0],
        sy = float(height) / projection.target[1];
  out.x0 = std::clamp((projection.offset[0] - fabsf(projection.scale[0])) * sx,
                      0.f, float(desc.Width));
  out.x1 = std::clamp((projection.offset[0] + fabsf(projection.scale[0])) * sx,
                      out.x0, float(desc.Width));
  out.y0 = std::clamp((projection.offset[1] - fabsf(projection.scale[1])) * sy,
                      0.f, float(desc.Height));
  out.y1 = std::clamp((projection.offset[1] + fabsf(projection.scale[1])) * sy,
                      out.y0, float(desc.Height));
  return out;
}
} // namespace BloodTestGpu
