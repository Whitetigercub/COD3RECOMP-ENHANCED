#pragma once
#include "blood.h"
#include <cstdint>
namespace BloodTest {
enum class Action { Capture, Clear, Regenerate };
struct State {
  bool floor = true;
  bool active = false, anchor = false, generated = false, frozen = true,
       snapshotReady = false, depthOff = false, logging = false;
  bool validationReady = false, validationMatch = false;
  uint64_t liveNormal = 0, liveNoDepth = 0, copyNormal = 0, copyNoDepth = 0;
  bool samplesReady = false;
  float point[3]{}, normal[3]{}, eye[3]{}, forward[3]{};
  float eyeDrift = 0, forwardDrift = 0, nearPlane = 0, farPlane = 0;
  uint64_t matrixFrame = 0;
  uint64_t revision = 0, generation = 0, draws = 0, frame = 0,
           normalSamples = 0, noDepthSamples = 0;
  int vertices = 0;
  uint32_t width = 0, height = 0;
  char status[128] = "Load a level, aim at a wall or floor, then F7.";
};
bool Available();
bool Active();
State Get();
void Floor(bool);
void Enable(bool);
void Request(Action);
void Freeze(bool);
void DepthOff(bool);
void Logging(bool);
// Called on the existing native game thread; no game state is written.
void Update(PPCContext &, uint8_t *);
void SceneSeen();
void MapChanged();
int Build(Blood::WorldVertex *, int);
void Submitted(uint64_t frame, int vertices, bool success);
void Samples(uint64_t generation, uint64_t normal, uint64_t noDepth);
void Validation(uint64_t, uint64_t, uint64_t, uint64_t);
void SnapshotReady(bool ready, uint32_t width = 0, uint32_t height = 0,
                   float nearPlane = 0, float farPlane = 0,
                   uint64_t matrixFrame = 0);
void Status(const char *);
void UI(bool compact);
bool Key(uint32_t key);
bool TakeScreenshot();
} // namespace BloodTest
