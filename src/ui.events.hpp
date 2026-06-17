#pragma once

#include <windows.h>

#include <d2d1_1.h>

#include <optional>
#include <string>
#include <vector>

#include "ui.action.hpp"

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
    FilesDropped,
    WindowMoved,
    WindowResized,
    DpiChanged,
    WindowClose,
    WindowDestroyed,
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
    PopupHost* popup_host = nullptr;
};

struct UiKeyEvent final {
    UiEventType type = UiEventType::KeyDown;
    UINT virtual_key = 0;
    UiModifiers modifiers;
    bool repeat = false;
    bool system = false;
    PopupHost* popup_host = nullptr;
};

struct UiInputEvent final {
    UiEventType type = UiEventType::PointerMove;
    UiPointerEvent pointer;
    UiKeyEvent key;
    wchar_t character = L'\0';
    std::wstring text;
    std::vector<std::wstring> file_paths;
    D2D1_POINT_2F point = {};
    POINT screen_point = {};
    UINT_PTR timer_id = 0;
    UINT dpi = 0;
    D2D1_SIZE_U pixel_size = {};
    D2D1_SIZE_F ui_size = {};
    HWND hwnd = nullptr;
    PopupHost* popup_host = nullptr;

    // Wraps a pointer event into an input event, mirroring its point and host so
    // both the pointer payload and the top-level fields stay in sync.
    static UiInputEvent Pointer(const UiPointerEvent& pointer, HWND hwnd)
    {
        return UiInputEvent{
            .type = pointer.type,
            .pointer = pointer,
            .point = pointer.point,
            .hwnd = hwnd,
            .popup_host = pointer.popup_host,
        };
    }

    // Wraps a key event into an input event, mirroring its host.
    static UiInputEvent Key(const UiKeyEvent& key, HWND hwnd)
    {
        return UiInputEvent{
            .type = key.type,
            .key = key,
            .hwnd = hwnd,
            .popup_host = key.popup_host,
        };
    }
};

enum class UiCaptureRequest {
    None,
    Capture,
    Release,
};

struct UiPopupRequest final {
    D2D1_POINT_2F origin = {};
    std::string state_json;
};

struct UiEventResult final {
    bool handled = false;
    UiCaptureRequest capture = UiCaptureRequest::None;
    UiAction action = kUiActionNone;
    bool wants_ime_position = false;
    std::optional<D2D1_POINT_2F> ime_caret_point;
    bool value_changed = false;
    bool close_popup = false;
    std::optional<UiPopupRequest> popup;
};
