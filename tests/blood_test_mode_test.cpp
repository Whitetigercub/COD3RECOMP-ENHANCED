#include "blood_test_gpu.h"
#include "blood_test_mode.h"
#include <Windows.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <d3d11.h>
#include <wrl/client.h>
struct PPCContext {};
namespace Blood {
int BuildWorld(WorldVertex *, int) { return 0; }
bool CaptureTestSurface(PPCContext &, uint8_t *, float *p, float *n, float *eye,
                        float *forward) {
  float a[3] = {0, 0, .5f}, b[3] = {0, 0, 1}, c[3] = {0, 0, -1};
  memcpy(p, a, 12);
  memcpy(n, b, 12);
  memcpy(eye, c, 12);
  memcpy(forward, b, 12);
  return true;
}
} // namespace Blood
void BloodDiagnosticEye(float *e) { e[0] = e[1] = e[2] = 0; }
using Microsoft::WRL::ComPtr;
static int checks = 0;
#define CHECK(x)                                                               \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(x)) {                                                                \
      printf("FAIL line%d: %s\n", __LINE__, #x);                               \
      return 1;                                                                \
    }                                                                          \
  } while (0)
int main(int argc, char **) {
  _putenv_s("COD3_BLOOD_TEST_MODE", argc > 1 ? "" : "1");
  if (argc > 1) {
    CHECK(!BloodTest::Available());
    CHECK(!BloodTest::Key(VK_F7));
    BloodTest::Enable(true);
    CHECK(!BloodTest::Active());
    puts("PASS: developer controls unavailable without opt-in");
    return 0;
  }
  PPCContext ctx{};
  BloodTest::Update(ctx, nullptr);
  CHECK(BloodTest::Active());
  BloodTest::SceneSeen();
  BloodTest::Request(BloodTest::Action::Capture);
  BloodTest::Update(ctx, nullptr);
  std::array<Blood::WorldVertex, 36> a{}, b{};
  CHECK(BloodTest::Build(a.data(), 36) == 36);
  CHECK(BloodTest::Build(a.data(), 35) == 0);
  BloodTest::Request(BloodTest::Action::Clear);
  BloodTest::Update(ctx, nullptr);
  CHECK(BloodTest::Build(b.data(), 36) == 0);
  BloodTest::Request(BloodTest::Action::Regenerate);
  BloodTest::Update(ctx, nullptr);
  CHECK(BloodTest::Build(b.data(), 36) == 36);
  CHECK(!memcmp(a.data(), b.data(), sizeof(a)));
  ComPtr<ID3D11Device> device;
  ComPtr<ID3D11DeviceContext> context;
  CHECK(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0,
                                    nullptr, 0, D3D11_SDK_VERSION, &device,
                                    nullptr, &context)));
  ComPtr<ID3D11Texture2D> color, depth;
  ComPtr<ID3D11RenderTargetView> rtv;
  ComPtr<ID3D11DepthStencilView> dsv;
  auto surfaces = [&](unsigned w, unsigned h, unsigned samples = 1) {
    color.Reset();
    depth.Reset();
    rtv.Reset();
    dsv.Reset();
    D3D11_TEXTURE2D_DESC t{};
    t.Width = w;
    t.Height = h;
    t.MipLevels = t.ArraySize = 1;
    t.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    t.SampleDesc.Count = samples;
    t.BindFlags = D3D11_BIND_RENDER_TARGET;
    device->CreateTexture2D(&t, nullptr, &color);
    device->CreateRenderTargetView(color.Get(), nullptr, &rtv);
    t.Format =
        samples > 1 ? DXGI_FORMAT_R32G8X24_TYPELESS : DXGI_FORMAT_D32_FLOAT;
    t.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    device->CreateTexture2D(&t, nullptr, &depth);
    D3D11_DEPTH_STENCIL_VIEW_DESC dv{};
    dv.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    dv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
    device->CreateDepthStencilView(depth.Get(), samples > 1 ? &dv : nullptr,
                                   &dsv);
  };
  surfaces(64, 64);
  CHECK(color && depth && rtv && dsv);
  BloodRender::Projection p{};
  for (int i = 0; i < 4; i++)
    p.matrix[i * 4 + i] = 1;
  p.scale[0] = 1;
  p.scale[1] = -1;
  p.scale[2] = 1;
  p.offset[0] = p.offset[1] = .5f;
  p.target[0] = p.target[1] = 2;
  p.flags[0] = 63;
  auto clear = [&](float z) {
    float black[4] = {0, 0, 0, 1};
    context->ClearRenderTargetView(rtv.Get(), black);
    context->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH, z, 0);
  };
  auto pixel = [&](BloodTestGpu::Frame frame) {
    ComPtr<ID3D11Resource> resource;
    frame.resource->GetResource(&resource);
    ComPtr<ID3D11Texture2D> texture;
    resource.As(&texture);
    D3D11_TEXTURE2D_DESC d{};
    texture->GetDesc(&d);
    d.Usage = D3D11_USAGE_STAGING;
    d.BindFlags = 0;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    device->CreateTexture2D(&d, nullptr, &staging);
    context->CopyResource(staging.Get(), texture.Get());
    D3D11_MAPPED_SUBRESOURCE m{};
    if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &m)))
      return 0u;
    uint32_t c;
    memcpy(&c,
           (char *)m.pData + (d.Height / 2) * m.RowPitch + (d.Width / 2) * 4,
           4);
    context->Unmap(staging.Get(), 0);
    return c;
  };
  auto observe = [&](unsigned w, unsigned h) {
    BloodTestGpu::Observe(device.Get(), context.Get(), rtv.Get(), dsv.Get(), p,
                          w, h);
  };
  auto parity = [&]() {
    context->Flush();
    for (int i = 0; i < 100 && !BloodTest::Get().validationReady; i++) {
      BloodTestGpu::FrozenFrame(device.Get(), context.Get());
      Sleep(1);
    }
    return BloodTest::Get().validationReady && BloodTest::Get().validationMatch;
  };
  clear(.25f);
  observe(64, 64);
  auto frame = BloodTestGpu::FrozenFrame(device.Get(), context.Get());
  CHECK(frame.resource);
  CHECK(parity());
  CHECK(BloodTest::Get().liveNormal > 0);
  uint32_t visible = pixel(frame);
  CHECK((visible & 0xffffff) != 0);
  // Frozen state survives live color, depth and camera changes.
  clear(.9f);
  p.matrix[3] = 1000;
  observe(64, 64);
  for (int i = 0; i < 120; i++) {
    auto f = BloodTestGpu::FrozenFrame(device.Get(), context.Get());
    CHECK(pixel(f) == visible);
  }
  BloodTest::Request(BloodTest::Action::Clear);
  BloodTest::Update(ctx, nullptr);
  frame = BloodTestGpu::FrozenFrame(device.Get(), context.Get());
  CHECK((pixel(frame) & 0xffffff) == 0);
  BloodTest::Request(BloodTest::Action::Regenerate);
  BloodTest::Update(ctx, nullptr);
  frame = BloodTestGpu::FrozenFrame(device.Get(), context.Get());
  CHECK(pixel(frame) == visible);
  // Recapture an occluder, compare exact source/copy coverage, then disable
  // depth.
  p.matrix[3] = 0;
  clear(.75f);
  observe(64, 64);
  BloodTest::Freeze(true);
  frame = BloodTestGpu::FrozenFrame(device.Get(), context.Get());
  CHECK(parity());
  CHECK(BloodTest::Get().liveNormal == 0);
  CHECK(BloodTest::Get().liveNoDepth > 0);
  CHECK((pixel(frame) & 0xffffff) == 0);
  BloodTest::DepthOff(true);
  frame = BloodTestGpu::FrozenFrame(device.Get(), context.Get());
  CHECK((pixel(frame) & 0xffffff) != 0);
  BloodTest::DepthOff(false);
  frame = BloodTestGpu::FrozenFrame(device.Get(), context.Get());
  CHECK((pixel(frame) & 0xffffff) == 0);
  // New resolution requires explicit recapture; geometry remains deterministic.
  surfaces(128, 96);
  clear(.25f);
  observe(128, 96);
  BloodTest::Freeze(true);
  frame = BloodTestGpu::FrozenFrame(device.Get(), context.Get());
  CHECK(frame.width == 128 && frame.height == 96);
  CHECK(parity());
  CHECK(BloodTest::Get().liveNormal > 0);
  CHECK(BloodTest::Build(b.data(), 36) == 36);
  CHECK(!memcmp(a.data(), b.data(), sizeof(a)));
  surfaces(128, 96, 4);
  CHECK(color && depth && rtv && dsv);
  clear(.25f);
  observe(128, 96);
  BloodTest::Freeze(true);
  frame = BloodTestGpu::FrozenFrame(device.Get(), context.Get());
  if (!frame.resource)
    printf("Native-format snapshot status: %s\n", BloodTest::Get().status);
  CHECK(frame.resource);
  CHECK(parity());
  CHECK(BloodTest::Get().liveNormal > 0);
  BloodTest::MapChanged();
  CHECK(BloodTest::Get().anchor);
  CHECK(pixel(BloodTestGpu::FrozenFrame(device.Get(), context.Get())) ==
        pixel(frame));
  BloodTest::Freeze(false);
  CHECK(!BloodTestGpu::FrozenFrame(device.Get(), context.Get()).resource);
  BloodTest::MapChanged();
  CHECK(!BloodTest::Get().anchor);
  BloodTest::Enable(false);
  CHECK(!BloodTest::Active());
  CHECK(BloodTest::Build(b.data(), 36) == 0);
  CHECK(!BloodTestGpu::FrozenFrame(device.Get(), context.Get()).resource);
  printf("PASS %d checks: deterministic placement/clear/regen, 120 frozen "
         "frames, live camera/depth isolation, source/copy parity, real depth "
         "rejection/toggle, resolution recapture, opt-out.\n",
         checks);
}
