#pragma once

#include <vector>

#include "ui.action.hpp"

struct MenuItem final {
    const wchar_t* text = L"";
    UiAction action = kUiActionNone;
    bool separator = false;
    bool checked = false;
    bool enabled = true;
    std::vector<MenuItem> children;
};
