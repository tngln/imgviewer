#pragma once

#include <windows.h>

#include <d2d1_1.h>

#include <string>

#include "ui.element.hpp"

class PopupHost;

enum class UiEventType {
    PointerMove,
    PointerDown,
    PointerUp,
    PointerLeave,
    PointerWheel,
    KeyDown,
    KeyUp,
    TextChar,
    ImeStartComposition,
    ImeComposition,
    ImeEndComposition,
    ContextMenu,
    Timer,
    Cancel,
    OwnerDeactivated,
    FocusGained,
    FocusLost,
};

enum class UiPointerButton {
    None,
    Left,
    Right,
    Middle,
};

struct UiModifiers final {
    bool ctrl = false;
    bool shift = false;
    bool alt = false;

    static UiModifiers Current();
};

struct UiPointerEvent final {
    UiEventType type = UiEventType::PointerMove;
    D2D1_POINT_2F point = {};
    UiPointerButton button = UiPointerButton::None;
    int wheel_delta = 0;
    UiModifiers modifiers;
    UiElementId target = UiElementId::None;
    UiElementId captured = UiElementId::None;
    PopupHost* popup_host = nullptr;
};

struct UiKeyEvent final {
    UiEventType type = UiEventType::KeyDown;
    UINT virtual_key = 0;
    UiModifiers modifiers;
    bool repeat = false;
    UiElementId focused = UiElementId::None;
    PopupHost* popup_host = nullptr;
};

struct UiInputEvent final {
    UiEventType type = UiEventType::PointerMove;
    UiPointerEvent pointer;
    UiKeyEvent key;
    UiElementId focused = UiElementId::None;
    wchar_t character = L'\0';
    std::wstring text;
    D2D1_POINT_2F point = {};
    POINT screen_point = {};
    UINT_PTR timer_id = 0;
    HWND hwnd = nullptr;
    PopupHost* popup_host = nullptr;
};

enum class UiCaptureRequest {
    None,
    Capture,
    Release,
};

enum class UiFocusRequest {
    None,
    FocusTarget,
    ClearFocus,
};

struct UiEventResult final {
    bool handled = false;
    bool needs_render = false;
    UiCaptureRequest capture = UiCaptureRequest::None;
    UiFocusRequest focus = UiFocusRequest::None;
    UiElementId focus_target = UiElementId::None;
    UiAction action = kUiActionNone;
    bool wants_ime_position = false;
    bool value_changed = false;
    bool close_popup = false;
    UiElementId effect_target = UiElementId::None;
};
