#pragma once

#include <windows.h>

#include "ui.hpp"

struct UiPostedActionMessage final {
    UiAction action = kUiActionNone;
    UiElementId effect_target = UiElementId::None;
};

UiPostedActionMessage DecodeUiPostedActionMessage(WPARAM wparam, LPARAM lparam);
void ApplyUiCaptureRequest(HWND hwnd, UiCaptureRequest capture);
bool ApplyUiEffect(UiController* ui, UiElementId effect_target);
bool ApplyUiEffectAndInvalidate(HWND hwnd, UiController* ui, UiElementId effect_target);
void RequestWindowRender(HWND hwnd, bool needs_render = true);
