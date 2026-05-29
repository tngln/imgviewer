#pragma once

#include <d2d1_1.h>
#include <dwrite.h>

enum class UiCommand {
    None,
    OpenImage,
};

struct UiEventResult {
    bool handled = false;
    bool needs_render = false;
    bool captured = false;
    bool released_capture = false;
    UiCommand command = UiCommand::None;
};

class UiController final {
public:
    UiEventResult OnPointerMove(D2D1_POINT_2F point);
    UiEventResult OnPointerDown(D2D1_POINT_2F point);
    UiEventResult OnPointerUp(D2D1_POINT_2F point);
    UiEventResult OnPointerLeave();

    void Draw(
        ID2D1DeviceContext* d2d_context,
        IDWriteTextFormat* body_text_format,
        IDWriteTextFormat* icon_text_format);
    D2D1_RECT_F TestButtonRect() const;
    D2D1_RECT_F OpenButtonRect() const;
    void InvokeTestButton();

private:
    enum class ButtonId {
        None,
        OpenImage,
        Test,
    };

    ButtonId HitTest(D2D1_POINT_2F point) const;

    D2D1_RECT_F open_button_rect_ = D2D1_RECT_F{32.0f, 168.0f, 232.0f, 212.0f};
    D2D1_RECT_F test_button_rect_ = D2D1_RECT_F{244.0f, 168.0f, 400.0f, 212.0f};
    ButtonId hovered_button_ = ButtonId::None;
    ButtonId pressed_button_ = ButtonId::None;
    unsigned int button_clicks_ = 0;
};
