#include "blood_test_mode.h"
#include "blood_math.h"
#include "imgui.h"
#include <Windows.h>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
namespace Blood {
bool CaptureTestSurface(PPCContext &, uint8_t *, float *, float *, float *,
                        float *);
}
namespace BloodTest {
namespace {
std::mutex mutex;
State state;
std::atomic<int> request{-1};
std::atomic<bool> screenshot{false};
uint64_t generatedAt = 0;
std::atomic<uint64_t> sceneAt{0};
} // namespace
bool Marker() {
  static const bool marker = []() {
    wchar_t path[32768]{};
    DWORD n = GetModuleFileNameW(nullptr, path, 32768);
    if (!n || n >= 32768)
      return false;
    wchar_t *slash = wcsrchr(path, L'\\');
    if (!slash)
      return false;
    slash[1] = 0;
    std::wstring file(path);
    file += L"BloodTestMode.enable";
    DWORD attributes = GetFileAttributesW(file.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           !(attributes & FILE_ATTRIBUTE_DIRECTORY);
  }();
  return marker;
}
bool AutoCapture() {
  return getenv("COD3_BLOOD_TEST_AUTO") != nullptr || Marker();
}
bool Available() {
  static const bool on = getenv("COD3_BLOOD_TEST_MODE") != nullptr || Marker();
  return on;
}
State Get() {
  std::lock_guard<std::mutex> lock(mutex);
  return state;
}
bool Active() { return Available() && Get().active; }
void Floor(bool on) {
  std::lock_guard<std::mutex> lock(mutex);
  state.floor = on;
}
void Enable(bool on) {
  if (!Available())
    return;
  std::lock_guard<std::mutex> lock(mutex);
  state.active = on;
  state.samplesReady = false;
  ++state.generation;
  state.snapshotReady = false;
  state.validationReady = false;
  ++state.revision;
}
void Request(Action a) {
  if (Available())
    request.store(int(a));
}
void Freeze(bool on) {
  std::lock_guard<std::mutex> lock(mutex);
  state.frozen = on;
  state.snapshotReady = false;
  state.validationReady = false;
  ++state.revision;
}
void DepthOff(bool on) {
  std::lock_guard<std::mutex> lock(mutex);
  state.depthOff = on;
}
void Logging(bool on) {
  std::lock_guard<std::mutex> lock(mutex);
  state.logging = on;
}
void Status(const char *s) {
  std::lock_guard<std::mutex> lock(mutex);
  snprintf(state.status, sizeof(state.status), "%s", s);
}
void Validation(uint64_t a, uint64_t b, uint64_t c, uint64_t d) {
  std::lock_guard<std::mutex> lock(mutex);
  state.validationReady = true;
  state.validationMatch = a == c && b == d;
  state.liveNormal = a;
  state.liveNoDepth = b;
  state.copyNormal = c;
  state.copyNoDepth = d;
  if (state.logging) {
    printf("BTEST parity live=%llu/%llu copy=%llu/%llu match=%d "
           "matrix_frame=%llu near=%.6f far=%.3f\n",
           a, b, c, d, state.validationMatch, state.matrixFrame,
           state.nearPlane, state.farPlane);
    fflush(stdout);
  }
  snprintf(
      state.status, sizeof(state.status), "%s",
      state.validationMatch
          ? "Snapshot depth coverage matches source (check nonzero coverage)."
          : "DEPTH PARITY FAILED: use LIVE for depth conclusions.");
}
void SceneSeen() { sceneAt.store(GetTickCount64()); }
void MapChanged() {
  if (!Available())
    return;
  std::lock_guard<std::mutex> lock(mutex);
  if (state.active && state.frozen && state.snapshotReady)
    return;
  sceneAt.store(0);
  state.anchor = state.generated = state.snapshotReady = state.samplesReady =
      state.validationReady = false;
  ++state.revision;
  if (AutoCapture())
    request.store(int(Action::Capture));
}
void SnapshotReady(bool ready, uint32_t w, uint32_t h, float nearPlane,
                   float farPlane, uint64_t matrixFrame) {
  std::lock_guard<std::mutex> lock(mutex);
  state.snapshotReady = ready;
  state.width = w;
  state.height = h;
  state.nearPlane = nearPlane;
  state.farPlane = farPlane;
  state.matrixFrame = matrixFrame;
}
void Update(PPCContext &ctx, uint8_t *base) {
  if (!Available())
    return;
  static bool init = false;
  if (!init) {
    init = true;
    Enable(true);
    if ((getenv("COD3_BLOOD_TEST_LOG") || Marker()))
      Logging(true);
    if (AutoCapture())
      Request(Action::Capture);
  }
  if (!Get().active)
    return;
  int a = request.exchange(-1);
  if (a < 0)
    return;
  if (a == int(Action::Capture)) {
    if (!sceneAt.load() || GetTickCount64() - sceneAt.load() > 1000) {
      Status("Waiting for a live native world pass before surface capture.");
      request.store(a);
      return;
    }
    float p[3], n[3], eye[3], f[3];
    if (!Blood::CaptureTestSurface(ctx, base, p, n, eye, f)) {
      Status("No unobstructed static surface found. Aim closer, then F7.");
      if (AutoCapture())
        request.store(a);
      return;
    }
    std::lock_guard<std::mutex> lock(mutex);
    memcpy(state.point, p, 12);
    memcpy(state.normal, n, 12);
    memcpy(state.eye, eye, 12);
    memcpy(state.forward, f, 12);
    state.anchor = state.generated = true;
    state.samplesReady = false;
    state.snapshotReady = false;
    state.validationReady = false;
    state.draws = 0;
    ++state.revision;
    ++state.generation;
    generatedAt = GetTickCount64();
    snprintf(state.status, sizeof(state.status),
             "Surface captured; fixed world coordinates, no lifetime expiry.");
  } else {
    std::lock_guard<std::mutex> lock(mutex);
    if (a == int(Action::Clear)) {
      state.generated = false;
      ++state.generation;
      state.normalSamples = state.noDepthSamples = 0;
      state.vertices = 0;
      state.samplesReady = false;
      snprintf(state.status, sizeof(state.status),
               "Test decal cleared; anchor retained.");
    } else if (state.anchor) {
      state.generated = true;
      state.samplesReady = false;
      ++state.generation;
      state.draws = 0;
      generatedAt = GetTickCount64();
      snprintf(state.status, sizeof(state.status),
               "Identical decal regenerated at retained anchor.");
    }
  }
}
int Build(Blood::WorldVertex *out, int capacity) {
  auto s = Get();
  if (!Available() || !s.active || !s.anchor || !s.generated || capacity < 36)
    return 0;
  using namespace BloodMath;
  V n{s.normal[0], s.normal[1], s.normal[2]},
      center{s.point[0], s.point[1], s.point[2]};
  center = center + n * .04f;
  V u = Unit(Cross(n, std::fabs(n.z) < .9f ? V{0, 0, 1} : V{1, 0, 0})),
    v = Cross(n, u);
  V points[12];
  uint32_t seed = 0x93A71B2D;
  for (int k = 0; k < 12; k++) {
    seed = seed * 1664525u + 1013904223u;
    float radius = 8.f * (.65f + (seed & 255) / 255.f * .35f),
          a = k * 6.2831853f / 12;
    points[k] = center + u * (cosf(a) * radius) + v * (sinf(a) * radius);
  }
  int count = 0;
  auto emit = [&](V p) { out[count++] = {p.x, p.y, p.z, 0xB703044Cu}; };
  for (int k = 0; k < 12; k++) {
    emit(center);
    emit(points[k]);
    emit(points[(k + 1) % 12]);
  }
  return count;
}
void Submitted(uint64_t frame, int vertices, bool success) {
  std::lock_guard<std::mutex> lock(mutex);
  state.frame = frame;
  state.vertices = vertices;
  if (success)
    ++state.draws;
}
void Samples(uint64_t generation, uint64_t normal, uint64_t noDepth) {
  std::lock_guard<std::mutex> lock(mutex);
  if (generation != state.generation)
    return;
  state.normalSamples = normal;
  state.noDepthSamples = noDepth;
  state.samplesReady = true;
  static uint64_t last = 0;
  auto now = GetTickCount64();
  if (state.logging && now - last >= 1000) {
    last = now;
    printf("BTEST generation=%llu frame=%llu age_ms=%llu vertices=%d "
           "normal=%llu no_depth=%llu frozen=%d\n",
           generation, state.frame, now - generatedAt, state.vertices, normal,
           noDepth, state.frozen);
    fflush(stdout);
  }
}
bool TakeScreenshot(){return screenshot.exchange(false);}
bool Key(uint32_t k) {
  if (!Available())
    return false;
  if (k == VK_F6) {
    Enable(!Get().active);
    return true;
  }
  if (!Active())
    return false;
  if(k==VK_F12){screenshot.store(true);return true;}
  if (k == VK_F5) {
    DepthOff(!Get().depthOff);
    return true;
  }
  if (k == VK_F7) {
    Request(Action::Capture);
    return true;
  }
  if (k == VK_F8) {
    Request(Action::Clear);
    return true;
  }
  if (k == VK_F9) {
    Request(Action::Regenerate);
    return true;
  }
  return false;
}
void UI(bool compact) {
  if (!Available())
    return;
  auto s = Get();
  if (compact && !s.active)
    return;
  if (compact) {
    ImGui::SetNextWindowPos(ImVec2(8, 75), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(.82f);
    if (!ImGui::Begin("Blood Test Mode", nullptr,
                      ImGuiWindowFlags_AlwaysAutoResize |
                          ImGuiWindowFlags_NoSavedSettings |
                          ImGuiWindowFlags_NoInputs)) {
      ImGui::End();
      return;
    }
  }
  bool enabled = s.active;
  if (ImGui::Checkbox("Developer Blood Test Mode", &enabled))
    Enable(enabled);
  if (s.active) {
    bool floor = s.floor;
    if (ImGui::Checkbox("Trace floor ahead (off: aimed wall)", &floor))
      Floor(floor);
    bool frozen = s.frozen;
    if (ImGui::Checkbox("Frozen level snapshot", &frozen))
      Freeze(frozen);
    bool off = s.depthOff;
    if (ImGui::Checkbox("Disable depth test (diagnostic, F5)", &off))
      DepthOff(off);
    if (ImGui::Button("Capture aimed surface / snapshot (F7)"))
      Request(Action::Capture);
    if (ImGui::Button("Clear (F8)"))
      Request(Action::Clear);
    ImGui::SameLine();
    if (ImGui::Button("Regenerate same decal (F9)"))
      Request(Action::Regenerate);
    ImGui::Text("%s", s.status);
    ImGui::Text("Decals %d | vertices %d | draws %llu | generation %llu",
                s.anchor && s.generated ? 1 : 0, s.vertices, s.draws,
                s.generation);
    ImGui::Text("Anchor %.3f %.3f %.3f", s.point[0], s.point[1], s.point[2]);
    ImGui::Text("Rendering: %s | depth %s | snapshot %s %ux%u",
                s.frozen ? "frozen copy" : "LIVE native pass",
                s.depthOff ? "OFF" : "normal GREATER_EQUAL",
                s.snapshotReady ? "ready" : "pending", s.width, s.height);
    if (s.samplesReady)
      ImGui::Text("GPU coverage: normal %llu | no depth %llu | %s",
                  s.normalSamples, s.noDepthSamples,
                  s.normalSamples    ? "passes depth"
                  : s.noDepthSamples ? "depth-rejected"
                                     : "clipped / offscreen / empty");
    else
      ImGui::TextUnformatted(
          "GPU coverage: pending (not a final-pixel visibility verdict)");
    if (s.frozen) {
      ImGui::Text("Depth copy validation: %s",
                  s.validationReady
                      ? (s.validationMatch ? "MATCH" : "FAILED - use LIVE")
                      : "pending");
      ImGui::Text("Captured frame %llu | near %.4f far %.2f", s.matrixFrame,
                  s.nearPlane, s.farPlane);
      if (s.validationReady)
        ImGui::Text("Live %llu/%llu | copy %llu/%llu (normal/no-depth)",
                    s.liveNormal, s.liveNoDepth, s.copyNormal, s.copyNoDepth);
    }
    bool logs = s.logging;
    if (ImGui::Checkbox("Log once per second", &logs))
      Logging(logs);
    ImGui::TextWrapped(
        "Frozen mode copies a native scene, depth and projection. It does not "
        "freeze game simulation. Use LIVE for camera movement and native "
        "occlusion/order checks. F6 exits without saving settings.");
  }
  if (compact)
    ImGui::End();
}
} // namespace BloodTest
