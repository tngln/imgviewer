#pragma once

#include <cstddef>

#include <d2d1_1.h>
#include <dwrite.h>

#include "icons.inc"

struct UiDrawContext final {
    ID2D1RenderTarget* d2d_context = nullptr;
    IDWriteFactory* dwrite_factory = nullptr;
    IDWriteTextFormat* body_text_format = nullptr;
    IDWriteTextFormat* icon_text_format = nullptr;
};

class UiDraw final {
public:
    explicit UiDraw(const UiDrawContext& context);

    void Clear(D2D1_COLOR_F color) const;
    void FillRect(D2D1_RECT_F rect, D2D1_COLOR_F color) const;
    void DrawRect(D2D1_RECT_F rect, D2D1_COLOR_F color, float stroke_width = 1.0f) const;
    void FillRoundedRect(D2D1_ROUNDED_RECT rect, D2D1_COLOR_F color) const;
    void DrawRoundedRect(D2D1_ROUNDED_RECT rect, D2D1_COLOR_F color, float stroke_width = 1.0f) const;
    void DrawBodyText(
        const wchar_t* text,
        UINT32 text_length,
        D2D1_RECT_F rect,
        D2D1_COLOR_F color,
        D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE measuring_mode = DWRITE_MEASURING_MODE_NATURAL) const;
    void DrawIconText(
        const wchar_t* text,
        UINT32 text_length,
        D2D1_RECT_F rect,
        D2D1_COLOR_F color,
        D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE measuring_mode = DWRITE_MEASURING_MODE_NATURAL) const;
    void DrawText(
        const wchar_t* text,
        UINT32 text_length,
        IDWriteTextFormat* text_format,
        D2D1_RECT_F rect,
        D2D1_COLOR_F color,
        D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE measuring_mode = DWRITE_MEASURING_MODE_NATURAL) const;
    void DrawGeometry(ID2D1Geometry* geometry, D2D1_COLOR_F color, float stroke_width = 1.0f) const;

private:
    ID2D1SolidColorBrush* CreateBrush(D2D1_COLOR_F color, ID2D1SolidColorBrush** brush) const;

    const UiDrawContext& context_;
};

HRESULT CreatePathGeometryFromIcon(
    ID2D1Factory* factory,
    const icons::PathCommand* commands,
    size_t command_count,
    ID2D1PathGeometry** geometry);
