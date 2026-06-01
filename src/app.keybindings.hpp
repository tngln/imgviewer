#pragma once

#include <windows.h>

#include <vector>

#include <nlohmann/json.hpp>

#include "app.action.hpp"

struct KeyGesture final {
    UINT virtual_key = 0;
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
};

struct KeyBinding final {
    KeyGesture gesture;
    AppAction action = AppAction::None;
};

struct ActionBindings final {
    std::vector<KeyBinding> key_bindings;
};

ActionBindings DefaultActionBindings();
void ApplyKeyBindingsConfig(const nlohmann::json& root, ActionBindings* bindings);
AppAction ActionForKey(const ActionBindings& bindings, UINT virtual_key, bool ctrl, bool shift, bool alt);
