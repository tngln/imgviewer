#pragma once

#include <windows.h>

#include <d2d1_1.h>

#include "imgviewer.action.hpp"
#include "ui.element.hpp"

enum class UiEventType {
    PointerMove,
    PointerDown,
    PointerUp,
    PointerLeave,
    PointerWheel,
    KeyDown,
    KeyUp,
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
};

struct UiPointerEvent final {
    UiEventType type = UiEventType::PointerMove;
    D2D1_POINT_2F point = {};
    UiPointerButton button = UiPointerButton::None;
    int wheel_delta = 0;
    UiModifiers modifiers;
    UiElementId target = UiElementId::None;
    UiElementId captured = UiElementId::None;
};

struct UiKeyEvent final {
    UiEventType type = UiEventType::KeyDown;
    UINT virtual_key = 0;
    UiModifiers modifiers;
    bool repeat = false;
    UiElementId focused = UiElementId::None;
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
    ImgViewerAction action = ImgViewerAction::None;
};
