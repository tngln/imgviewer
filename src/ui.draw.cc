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
    wil::com_ptr<ID2D1SolidColorBrush> brush;
    if (CreateBrush(color, brush.put()) == nullptr) {
        return;
    }

    context_.d2d_context->FillRectangle(rect, brush.get());
}

void UiDraw::DrawRect(D2D1_RECT_F rect, D2D1_COLOR_F color, float stroke_width) const
{
    wil::com_ptr<ID2D1SolidColorBrush> brush;
    if (CreateBrush(color, brush.put()) == nullptr) {
        return;
    }

    context_.d2d_context->DrawRectangle(rect, brush.get(), stroke_width);
}

void UiDraw::FillRoundedRect(D2D1_ROUNDED_RECT rect, D2D1_COLOR_F color) const
{
    wil::com_ptr<ID2D1SolidColorBrush> brush;
    if (CreateBrush(color, brush.put()) == nullptr) {
        return;
    }

    context_.d2d_context->FillRoundedRectangle(rect, brush.get());
}

void UiDraw::DrawRoundedRect(D2D1_ROUNDED_RECT rect, D2D1_COLOR_F color, float stroke_width) const
{
    wil::com_ptr<ID2D1SolidColorBrush> brush;
    if (CreateBrush(color, brush.put()) == nullptr) {
        return;
    }

    context_.d2d_context->DrawRoundedRectangle(rect, brush.get(), stroke_width);
}

void UiDraw::DrawBodyText(
    const wchar_t* text,
    UINT32 text_length,
    D2D1_RECT_F rect,
    D2D1_COLOR_F color,
    D2D1_DRAW_TEXT_OPTIONS options,
    DWRITE_MEASURING_MODE measuring_mode) const
{
    DrawText(text, text_length, context_.body_text_format, rect, color, options, measuring_mode);
}

void UiDraw::DrawIconText(
    const wchar_t* text,
    UINT32 text_length,
    D2D1_RECT_F rect,
    D2D1_COLOR_F color,
    D2D1_DRAW_TEXT_OPTIONS options,
    DWRITE_MEASURING_MODE measuring_mode) const
{
    DrawText(text, text_length, context_.icon_text_format, rect, color, options, measuring_mode);
}

void UiDraw::DrawText(
    const wchar_t* text,
    UINT32 text_length,
    IDWriteTextFormat* text_format,
    D2D1_RECT_F rect,
    D2D1_COLOR_F color,
    D2D1_DRAW_TEXT_OPTIONS options,
    DWRITE_MEASURING_MODE measuring_mode) const
{
    if (text == nullptr || text_format == nullptr) {
        return;
    }

    wil::com_ptr<ID2D1SolidColorBrush> brush;
    if (CreateBrush(color, brush.put()) == nullptr) {
        return;
    }

    context_.d2d_context->DrawTextW(
        text,
        text_length,
        text_format,
        rect,
        brush.get(),
        options,
        measuring_mode);
}

void UiDraw::DrawGeometry(ID2D1Geometry* geometry, D2D1_COLOR_F color, float stroke_width) const
{
    if (geometry == nullptr) {
        return;
    }

    wil::com_ptr<ID2D1SolidColorBrush> brush;
    if (CreateBrush(color, brush.put()) == nullptr) {
        return;
    }

    context_.d2d_context->DrawGeometry(geometry, brush.get(), stroke_width);
}

ID2D1SolidColorBrush* UiDraw::CreateBrush(D2D1_COLOR_F color, ID2D1SolidColorBrush** brush) const
{
    if (context_.d2d_context == nullptr || brush == nullptr) {
        return nullptr;
    }

    *brush = nullptr;
    return SUCCEEDED(context_.d2d_context->CreateSolidColorBrush(color, brush)) ? *brush : nullptr;
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
