#pragma once

#include <d2d1_1.h>
#include <dwrite.h>

struct UiEventResult {
    bool handled = false;
    bool needs_render = false;
    bool captured = false;
    bool released_capture = false;
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
    void InvokeTestButton();

private:
    bool HitTestButton(D2D1_POINT_2F point) const;

    D2D1_RECT_F button_rect_ = D2D1_RECT_F{32.0f, 168.0f, 188.0f, 212.0f};
    bool button_hovered_ = false;
    bool button_pressed_ = false;
    unsigned int button_clicks_ = 0;
};
