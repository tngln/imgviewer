#include "ui.draw.hpp"

#include <d2d1helper.h>
#include <wil/com.h>
#include <wil/result_macros.h>

UiDraw::UiDraw(const UiDrawContext& context) : context_(context) {}

void UiDraw::Clear(D2D1_COLOR_F color) const
{
    if (context_.d2d_context == nullptr) {
        return;
    }

    context_.d2d_context->Clear(color);
}

void UiDraw::FillRect(D2D1_RECT_F rect, D2D1_COLOR_F color) const
{
    ID2D1SolidColorBrush* brush = ResolveBrush(color);
    if (brush == nullptr) {
        return;
    }

    context_.d2d_context->FillRectangle(rect, brush);
}

void UiDraw::DrawRect(D2D1_RECT_F rect, D2D1_COLOR_F color, float stroke_width) const
{
    ID2D1SolidColorBrush* brush = ResolveBrush(color);
    if (brush == nullptr) {
        return;
    }

    context_.d2d_context->DrawRectangle(rect, brush, stroke_width);
}

void UiDraw::FillRoundedRect(D2D1_ROUNDED_RECT rect, D2D1_COLOR_F color) const
{
    ID2D1SolidColorBrush* brush = ResolveBrush(color);
    if (brush == nullptr) {
        return;
    }

    context_.d2d_context->FillRoundedRectangle(rect, brush);
}

void UiDraw::DrawRoundedRect(D2D1_ROUNDED_RECT rect, D2D1_COLOR_F color, float stroke_width) const
{
    ID2D1SolidColorBrush* brush = ResolveBrush(color);
    if (brush == nullptr) {
        return;
    }

    context_.d2d_context->DrawRoundedRectangle(rect, brush, stroke_width);
}

void UiDraw::DrawBodyText(
    std::wstring_view text,
    D2D1_RECT_F rect,
    D2D1_COLOR_F color,
    D2D1_DRAW_TEXT_OPTIONS options,
    DWRITE_MEASURING_MODE measuring_mode) const
{
    DrawText(text, context_.body_text_format, rect, color, options, measuring_mode);
}

void UiDraw::DrawIconText(
    std::wstring_view text,
    D2D1_RECT_F rect,
    D2D1_COLOR_F color,
    D2D1_DRAW_TEXT_OPTIONS options,
    DWRITE_MEASURING_MODE measuring_mode) const
{
    DrawText(text, context_.icon_text_format, rect, color, options, measuring_mode);
}

void UiDraw::DrawText(
    std::wstring_view text,
    IDWriteTextFormat* text_format,
    D2D1_RECT_F rect,
    D2D1_COLOR_F color,
    D2D1_DRAW_TEXT_OPTIONS options,
    DWRITE_MEASURING_MODE measuring_mode) const
{
    if (text_format == nullptr) {
        return;
    }

    ID2D1SolidColorBrush* brush = ResolveBrush(color);
    if (brush == nullptr) {
        return;
    }

    context_.d2d_context->DrawTextW(
        text.data(),
        static_cast<UINT32>(text.size()),
        text_format,
        rect,
        brush,
        options,
        measuring_mode);
}

void UiDraw::DrawGeometry(ID2D1Geometry* geometry, D2D1_COLOR_F color, float stroke_width) const
{
    if (geometry == nullptr) {
        return;
    }

    ID2D1SolidColorBrush* brush = ResolveBrush(color);
    if (brush == nullptr) {
        return;
    }

    context_.d2d_context->DrawGeometry(geometry, brush, stroke_width);
}

ID2D1SolidColorBrush* UiDraw::ResolveBrush(D2D1_COLOR_F color) const
{
    if (context_.d2d_context == nullptr) {
        return nullptr;
    }

    // Prefer the pass-shared scratch brush when the caller supplied one.
    if (context_.brush != nullptr) {
        context_.brush->SetColor(color);
        return context_.brush;
    }

    // Otherwise lazily create a single brush for this UiDraw's lifetime.
    if (!cached_brush_) {
        if (FAILED(context_.d2d_context->CreateSolidColorBrush(color, cached_brush_.put()))) {
            return nullptr;
        }
        return cached_brush_.get();
    }

    cached_brush_->SetColor(color);
    return cached_brush_.get();
}

HRESULT CreatePathGeometryFromIcon(
    ID2D1Factory1* factory,
    const icons::PathCommand* commands,
    size_t command_count,
    ID2D1PathGeometry** geometry)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, factory);
    RETURN_HR_IF_NULL(E_INVALIDARG, commands);
    RETURN_HR_IF_NULL(E_INVALIDARG, geometry);

    wil::com_ptr<ID2D1PathGeometry> path_geometry;
    RETURN_IF_FAILED(factory->CreatePathGeometry(path_geometry.put()));

    wil::com_ptr<ID2D1GeometrySink> sink;
    RETURN_IF_FAILED(path_geometry->Open(sink.put()));

    bool figure_is_open = false;
    for (size_t index = 0; index < command_count; ++index) {
        const icons::PathCommand& command = commands[index];
        switch (command.verb) {
        case icons::PathVerb::MoveTo:
            if (figure_is_open) {
                sink->EndFigure(D2D1_FIGURE_END_OPEN);
            }
            sink->BeginFigure(command.points[0], D2D1_FIGURE_BEGIN_HOLLOW);
            figure_is_open = true;
            break;

        case icons::PathVerb::LineTo:
            sink->AddLine(command.points[0]);
            break;

        case icons::PathVerb::CubicTo:
            sink->AddBezier(D2D1::BezierSegment(command.points[0], command.points[1], command.points[2]));
            break;

        case icons::PathVerb::Close:
            if (figure_is_open) {
                sink->EndFigure(D2D1_FIGURE_END_CLOSED);
                figure_is_open = false;
            }
            break;
        }
    }

    if (figure_is_open) {
        sink->EndFigure(D2D1_FIGURE_END_OPEN);
    }

    RETURN_IF_FAILED(sink->Close());
    *geometry = path_geometry.detach();

    return S_OK;
}
