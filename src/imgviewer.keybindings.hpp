#pragma once

#include <windows.h>

#include <vector>

#include <nlohmann/json.hpp>

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
void ApplyKeyBindingsConfig(const nlohmann::json& root, ActionBindings* bindings);
ImgViewerAction ActionForKey(const ActionBindings& bindings, UINT virtual_key, bool ctrl, bool shift, bool alt);

std::wstring KeyName(UINT virtual_key);
std::wstring GestureText(const KeyGesture& gesture);
