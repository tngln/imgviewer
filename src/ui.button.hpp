#pragma once

#include <d2d1_1.h>
#include <dwrite.h>

enum class UiCommand {
    None,
    OpenImage,
    ToggleTopMost,
    Minimize,
    ToggleMaximize,
    Close,
};

enum class UiElementId {
    None,
    OpenImage,
    Test,
    TopMost,
    Minimize,
    MaximizeRestore,
    Close,
};

struct UiButtonMetadata final {
    UiElementId id = UiElementId::None;
    UiCommand command = UiCommand::None;
    const wchar_t* name = L"";
    const wchar_t* automation_id = L"";
    int runtime_id = 0;
};

struct UiButtonState final {
    bool hovered = false;
    bool pressed = false;
    bool active = false;
    bool danger = false;
};

class Button final {
public:
    Button(UiButtonMetadata metadata, const wchar_t* icon, const wchar_t* text);

    void SetRect(D2D1_RECT_F rect);
    D2D1_RECT_F Rect() const;
    const UiButtonMetadata& Metadata() const;
    bool Contains(D2D1_POINT_2F point) const;
    void Draw(
        ID2D1DeviceContext* d2d_context,
        IDWriteTextFormat* body_text_format,
        IDWriteTextFormat* icon_text_format,
        UiButtonState state) const;

private:
    UiButtonMetadata metadata_;
    const wchar_t* icon_ = L"";
    const wchar_t* text_ = L"";
    D2D1_RECT_F rect_ = {};
};

class IconButton final {
public:
    IconButton(UiButtonMetadata metadata, const wchar_t* icon);

    void SetIcon(const wchar_t* icon);
    void SetRect(D2D1_RECT_F rect);
    D2D1_RECT_F Rect() const;
    const UiButtonMetadata& Metadata() const;
    bool Contains(D2D1_POINT_2F point) const;
    void Draw(
        ID2D1DeviceContext* d2d_context,
        IDWriteTextFormat* icon_text_format,
        UiButtonState state) const;

private:
    UiButtonMetadata metadata_;
    const wchar_t* icon_ = L"";
    D2D1_RECT_F rect_ = {};
};
