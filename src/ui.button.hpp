#pragma once

#include <dwrite.h>

#include "ui.element.hpp"

class Button final : public UiElement {
public:
    Button(UiElementMetadata metadata, const wchar_t* icon, const wchar_t* text);

    void Draw(
        ID2D1DeviceContext* d2d_context,
        IDWriteTextFormat* body_text_format,
        IDWriteTextFormat* icon_text_format,
        UiElementState state) const;

private:
    const wchar_t* icon_ = L"";
    const wchar_t* text_ = L"";
};

class IconButton final : public UiElement {
public:
    IconButton(UiElementMetadata metadata, const wchar_t* icon);

    void SetIcon(const wchar_t* icon);
    void Draw(
        ID2D1DeviceContext* d2d_context,
        IDWriteTextFormat* icon_text_format,
        UiElementState state) const;

private:
    const wchar_t* icon_ = L"";
};
