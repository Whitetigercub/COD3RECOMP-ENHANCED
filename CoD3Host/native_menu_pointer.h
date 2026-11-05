#pragma once
#include <cstdint>
namespace NativeMenuPointer {
// True only for the supported campaign Single Player page; other menus keep
// their existing controls until their widgets have been mapped and tested.
bool Active();
bool InMission();
bool MenuVisible();
void Button(bool right,bool down,int x,int y);
void Cancel();
}
