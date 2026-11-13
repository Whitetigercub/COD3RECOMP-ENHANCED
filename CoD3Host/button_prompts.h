#pragma once
#include "settings.h"
#include <string>
namespace ButtonPrompts {
    void SetStickSwirlPrompt(uint32_t widget, bool clockwise);
    bool StickSwirlClockwise();
    void KeyboardUsed();
    void ControllerUsed();
    bool KeyboardActive();
    std::string ActionKey(Settings::Action action);
}
