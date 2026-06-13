#pragma once

#include <cstddef>
#include <string_view>

#include <d2d1_1.h>
#include <dwrite.h>
#include <wil/com.h>

#include "icons.inc"

struct UiDrawContext final {
    ID2D1DeviceContext* d2d_context = nullptr;
    ID2D1Factory1* d2d_factory = nullptr;
    IDWriteFactory* dwrite_factory = nullptr;
    IDWriteTextFormat* body_text_format = nullptr;
    IDWriteTextFormat* icon_text_format = nullptr;
    D2D1_SIZE_F viewport_size = {};
    float dpi_scale = 1.0f;
    // Optional scratch brush shared across a whole draw pass. When set, UiDraw
    // recolours it per primitive instead of allocating a new brush each call.
    // When null, UiDraw lazily caches one brush for its own lifetime.
    ID2D1SolidColorBrush* brush = nullptr;
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
        std::wstring_view text,
        D2D1_RECT_F rect,
        D2D1_COLOR_F color,
        D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE measuring_mode = DWRITE_MEASURING_MODE_NATURAL) const;
    void DrawIconText(
        std::wstring_view text,
        D2D1_RECT_F rect,
        D2D1_COLOR_F color,
        D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE measuring_mode = DWRITE_MEASURING_MODE_NATURAL) const;
    void DrawText(
        std::wstring_view text,
        IDWriteTextFormat* text_format,
        D2D1_RECT_F rect,
        D2D1_COLOR_F color,
        D2D1_DRAW_TEXT_OPTIONS options = D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE measuring_mode = DWRITE_MEASURING_MODE_NATURAL) const;
    void DrawGeometry(ID2D1Geometry* geometry, D2D1_COLOR_F color, float stroke_width = 1.0f) const;

private:
    ID2D1SolidColorBrush* ResolveBrush(D2D1_COLOR_F color) const;

    const UiDrawContext& context_;
    mutable wil::com_ptr<ID2D1SolidColorBrush> cached_brush_;
};

HRESULT CreatePathGeometryFromIcon(
    ID2D1Factory1* factory,
    const icons::PathCommand* commands,
    size_t command_count,
    ID2D1PathGeometry** geometry);
