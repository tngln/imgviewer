#pragma once

#include <windows.h>

#include "ui.events.hpp"

struct UiPostedActionMessage final {
    UiAction action = kUiActionNone;
};

UiPostedActionMessage DecodeUiPostedActionMessage(WPARAM wparam, LPARAM lparam);
void ApplyUiCaptureRequest(HWND hwnd, UiCaptureRequest capture);
void RequestWindowRender(HWND hwnd, bool render = true);
