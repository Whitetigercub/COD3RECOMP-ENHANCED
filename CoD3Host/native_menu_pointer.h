#pragma once
#include <cstdint>
namespace NativeMenuPointer {
// The supported campaign menu/dialog owns pointer input, including while
// locked or transitioning, to prevent legacy clicks reaching an underlying UI.
bool Active();
bool InMission();
bool MenuVisible();
void Button(bool right,bool down,int x,int y);
void Cancel();
}
