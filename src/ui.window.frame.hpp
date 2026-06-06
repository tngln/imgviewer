#pragma once

#include <array>
#include <cstddef>
#include <string>

#include <d2d1_1.h>

#include "ui.button.hpp"
#include "ui.root.hpp"

struct UiWindowFrameOptions final {
    const wchar_t* title = L"";
    bool show_minimize = true;
    bool show_maximize = true;
    bool show_close = true;
    bool show_border = true;
    float title_left_reserved_width = 0.0f;
    float title_right_reserved_width = 0.0f;
};

struct UiWindowFrameState final {
    bool maximized = false;
};

class UiWindowFrame final {
public:
    enum class ButtonKey : size_t {
        Minimize,
        MaximizeRestore,
        Close,
        Count,
    };

    static constexpr size_t kButtonCount = static_cast<size_t>(ButtonKey::Count);
    static constexpr size_t ButtonIndex(ButtonKey button);

    UiWindowFrame(UiElement& root, UiElementIdGenerator& ids, UiWindowFrameOptions options);

    void Draw(const UiDrawContext& draw_context, UiRootState root_state, UiWindowFrameState frame_state);
    bool IsPointInCaptionDragArea(const UiElement& root, D2D1_POINT_2F point) const;
    void SetTitleText(const wchar_t* title);
    D2D1_RECT_F ButtonRect(ButtonKey button) const;
    D2D1_RECT_F TitleBarRect() const;

private:
    struct ButtonInstance final {
        UiElementId id = UiElementId::None;
        IconButton* element = nullptr;
    };

    void Layout(D2D1_SIZE_F viewport_size);
    IconButton* Button(ButtonKey button);
    const IconButton* Button(ButtonKey button) const;
    UiElementState ButtonState(ButtonKey button, UiRootState root_state, bool danger = false) const;
    void DrawButton(ButtonKey button, const UiDrawContext& draw_context, UiRootState root_state, bool danger = false) const;

    UiWindowFrameOptions options_;
    std::array<ButtonInstance, kButtonCount> buttons_{};
    std::wstring title_text_;
    D2D1_RECT_F titlebar_rect_ = D2D1_RECT_F{0.0f, 0.0f, 960.0f, 48.0f};
    D2D1_RECT_F title_text_rect_ = D2D1_RECT_F{16.0f, 0.0f, 720.0f, 48.0f};
};
