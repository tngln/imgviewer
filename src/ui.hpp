#pragma once

#include <d2d1_1.h>
#include <dwrite.h>
#include <string>

enum class UiCommand {
    None,
    OpenImage,
    ToggleTopMost,
    Minimize,
    ToggleMaximize,
    Close,
};

enum class UiElementId {
    OpenImage,
    Test,
    TopMost,
    Minimize,
    MaximizeRestore,
    Close,
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
        D2D1_SIZE_F viewport_size,
        IDWriteTextFormat* body_text_format,
        IDWriteTextFormat* icon_text_format);
    D2D1_RECT_F ElementRect(UiElementId id) const;
    bool IsPointInCaptionDragArea(D2D1_POINT_2F point) const;
    void SetTitleText(const wchar_t* title);
    void SetWindowState(bool top_most, bool maximized);
    void InvokeTestButton();

private:
    enum class ButtonId {
        None,
        OpenImage,
        Test,
        TopMost,
        Minimize,
        MaximizeRestore,
        Close,
    };

    ButtonId HitTest(D2D1_POINT_2F point) const;
    UiCommand CommandFor(ButtonId button) const;
    D2D1_RECT_F RectFor(ButtonId button) const;

    std::wstring title_text_ = L"ImgViewer";
    D2D1_RECT_F titlebar_rect_ = D2D1_RECT_F{0.0f, 0.0f, 960.0f, 48.0f};
    D2D1_RECT_F title_text_rect_ = D2D1_RECT_F{16.0f, 0.0f, 720.0f, 48.0f};
    D2D1_RECT_F top_most_button_rect_ = D2D1_RECT_F{720.0f, 0.0f, 768.0f, 48.0f};
    D2D1_RECT_F minimize_button_rect_ = D2D1_RECT_F{768.0f, 0.0f, 816.0f, 48.0f};
    D2D1_RECT_F maximize_button_rect_ = D2D1_RECT_F{816.0f, 0.0f, 864.0f, 48.0f};
    D2D1_RECT_F close_button_rect_ = D2D1_RECT_F{864.0f, 0.0f, 912.0f, 48.0f};
    D2D1_RECT_F open_button_rect_ = D2D1_RECT_F{32.0f, 128.0f, 232.0f, 172.0f};
    D2D1_RECT_F test_button_rect_ = D2D1_RECT_F{244.0f, 128.0f, 400.0f, 172.0f};
    ButtonId hovered_button_ = ButtonId::None;
    ButtonId pressed_button_ = ButtonId::None;
    bool top_most_ = false;
    bool maximized_ = false;
    unsigned int button_clicks_ = 0;
};
