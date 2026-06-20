#pragma once

#include <windows.h>

#include <string>
#include <vector>

#include "imgviewer.action.hpp"

struct KeyGesture final {
    UINT virtual_key = 0;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
};

struct KeyBinding final {
    KeyGesture gesture;
    ImgViewerAction action = ImgViewerAction::None;
};

struct ActionBindings final {
    std::vector<KeyBinding> key_bindings;
};

ActionBindings DefaultActionBindings();
void ApplyKeyBindingConfig(ImgViewerAction action, const std::vector<std::string>& key_texts, ActionBindings* bindings);
ImgViewerAction ActionForKey(const ActionBindings& bindings, UINT virtual_key, bool ctrl, bool shift, bool alt);

std::wstring KeyName(UINT virtual_key);
std::string GestureConfigText(const KeyGesture& gesture);
std::wstring GestureText(const KeyGesture& gesture);
