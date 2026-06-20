#include "imgviewer.edit.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include <d2d1helper.h>
#include <dwrite.h>
#include <wil/result_macros.h>

#include "imgviewer.edit_geometry.hpp"
#include "math.hpp"
#include "ui.graphics_device.hpp"
#include "ui.text.hpp"
#include "win32.clipboard.hpp"

namespace {

int NormalizeQuadrants(int quadrants)
{
    int normalized = quadrants % 4;
    if (normalized < 0) {
        normalized += 4;
    }
    return normalized;
}

BYTE ColorChannel(float value)
{
    return static_cast<BYTE>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

void BlendPixel(BYTE* pixel, BYTE blue, BYTE green, BYTE red, BYTE alpha)
{
    const UINT inverse_alpha = 255U - alpha;
    pixel[0] = static_cast<BYTE>((static_cast<UINT>(blue) * alpha + static_cast<UINT>(pixel[0]) * inverse_alpha) / 255U);
    pixel[1] = static_cast<BYTE>((static_cast<UINT>(green) * alpha + static_cast<UINT>(pixel[1]) * inverse_alpha) / 255U);
    pixel[2] = static_cast<BYTE>((static_cast<UINT>(red) * alpha + static_cast<UINT>(pixel[2]) * inverse_alpha) / 255U);
    pixel[3] = 255;
}

void DrawPoint(std::vector<BYTE>* pixels, UINT width, UINT height, int x, int y, float radius, D2D1_COLOR_F color)
{
    if (pixels == nullptr || width == 0 || height == 0) {
        return;
    }

    const int min_x = (std::max)(0, static_cast<int>(std::floor(static_cast<float>(x) - radius)));
    const int max_x = (std::min)(static_cast<int>(width) - 1, static_cast<int>(std::ceil(static_cast<float>(x) + radius)));
    const int min_y = (std::max)(0, static_cast<int>(std::floor(static_cast<float>(y) - radius)));
    const int max_y = (std::min)(static_cast<int>(height) - 1, static_cast<int>(std::ceil(static_cast<float>(y) + radius)));
    const float radius_squared = radius * radius;
    const BYTE blue = ColorChannel(color.b);
    const BYTE green = ColorChannel(color.g);
    const BYTE red = ColorChannel(color.r);
    const BYTE alpha = ColorChannel(color.a);
    for (int yy = min_y; yy <= max_y; ++yy) {
        for (int xx = min_x; xx <= max_x; ++xx) {
            const float dx = static_cast<float>(xx - x);
            const float dy = static_cast<float>(yy - y);
            if (dx * dx + dy * dy > radius_squared) {
                continue;
            }

            BYTE* pixel = pixels->data() + (static_cast<size_t>(yy) * width + static_cast<size_t>(xx)) * 4;
            BlendPixel(pixel, blue, green, red, alpha);
        }
    }
}

void DrawLine(std::vector<BYTE>* pixels, UINT width, UINT height, D2D1_POINT_2F a, D2D1_POINT_2F b, float stroke_width, D2D1_COLOR_F color)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const int steps = (std::max)(1, static_cast<int>(std::ceil((std::max)(std::abs(dx), std::abs(dy)))));
    const float radius = (std::max)(1.0f, stroke_width * 0.5f);
    for (int step = 0; step <= steps; ++step) {
        const float t = static_cast<float>(step) / static_cast<float>(steps);
        DrawPoint(
            pixels,
            width,
            height,
            static_cast<int>(std::round(a.x + dx * t)),
            static_cast<int>(std::round(a.y + dy * t)),
            radius,
            color);
    }
}

D2D1_POINT_2F RotateDocumentPoint(D2D1_POINT_2F point, UINT source_width, UINT source_height, int rotation)
{
    if (rotation == 1) {
        return D2D1::Point2F(static_cast<float>(source_height) - point.y, point.x);
    }
    if (rotation == 2) {
        return D2D1::Point2F(static_cast<float>(source_width) - point.x, static_cast<float>(source_height) - point.y);
    }
    if (rotation == 3) {
        return D2D1::Point2F(point.y, static_cast<float>(source_width) - point.x);
    }
    return point;
}

D2D1_RECT_F NormalizedRect(D2D1_POINT_2F a, D2D1_POINT_2F b, D2D1_SIZE_U bounds)
{
    return D2D1::RectF(
        std::clamp((std::min)(a.x, b.x), 0.0f, static_cast<float>(bounds.width)),
        std::clamp((std::min)(a.y, b.y), 0.0f, static_cast<float>(bounds.height)),
        std::clamp((std::max)(a.x, b.x), 0.0f, static_cast<float>(bounds.width)),
        std::clamp((std::max)(a.y, b.y), 0.0f, static_cast<float>(bounds.height)));
}

bool IsUsefulRect(D2D1_RECT_F rect)
{
    return rect.right - rect.left >= 2.0f && rect.bottom - rect.top >= 2.0f;
}

bool SameRect(D2D1_RECT_F left, D2D1_RECT_F right)
{
    constexpr float kTolerance = 0.001f;
    return std::abs(left.left - right.left) < kTolerance &&
        std::abs(left.top - right.top) < kTolerance &&
        std::abs(left.right - right.right) < kTolerance &&
        std::abs(left.bottom - right.bottom) < kTolerance;
}

bool SamePoint(D2D1_POINT_2F left, D2D1_POINT_2F right)
{
    constexpr float kTolerance = 0.001f;
    return std::abs(left.x - right.x) < kTolerance && std::abs(left.y - right.y) < kTolerance;
}

bool SameStroke(const ImgViewerEditStroke& left, const ImgViewerEditStroke& right)
{
    if (left.points.size() != right.points.size() ||
        std::abs(left.width - right.width) >= 0.001f ||
        left.color.r != right.color.r ||
        left.color.g != right.color.g ||
        left.color.b != right.color.b ||
        left.color.a != right.color.a) {
        return false;
    }
    for (size_t index = 0; index < left.points.size(); ++index) {
        if (!SamePoint(left.points[index], right.points[index])) {
            return false;
        }
    }
    return true;
}

float StrokeSampleSpacing(const ImgViewerEditStroke& stroke)
{
    return (std::max)(0.75f, stroke.width * 0.25f);
}

void AppendStrokePoint(ImgViewerEditStroke* stroke, D2D1_POINT_2F point)
{
    if (stroke == nullptr) {
        return;
    }

    if (stroke->points.empty()) {
        stroke->points.push_back(point);
        return;
    }

    const D2D1_POINT_2F last = stroke->points.back();
    const float dx = point.x - last.x;
    const float dy = point.y - last.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    if (distance <= 0.001f) {
        return;
    }

    const float spacing = StrokeSampleSpacing(*stroke);
    const int steps = (std::max)(1, static_cast<int>(std::ceil(distance / spacing)));
    for (int step = 1; step <= steps; ++step) {
        const float t = static_cast<float>(step) / static_cast<float>(steps);
        stroke->points.push_back(D2D1::Point2F(last.x + dx * t, last.y + dy * t));
    }
}

bool SameTextObject(const ImgViewerEditText& left, const ImgViewerEditText& right)
{
    return SamePoint(left.origin, right.origin) &&
        left.text == right.text &&
        left.style.font_family == right.style.font_family &&
        std::abs(left.style.font_size - right.style.font_size) < 0.001f &&
        left.style.has_background == right.style.has_background &&
        left.style.text_color.r == right.style.text_color.r &&
        left.style.text_color.g == right.style.text_color.g &&
        left.style.text_color.b == right.style.text_color.b &&
        left.style.text_color.a == right.style.text_color.a &&
        left.style.background_color.r == right.style.background_color.r &&
        left.style.background_color.g == right.style.background_color.g &&
        left.style.background_color.b == right.style.background_color.b &&
        left.style.background_color.a == right.style.background_color.a;
}

bool SameMosaic(const ImgViewerEditMosaic& left, const ImgViewerEditMosaic& right)
{
    return SameRect(left.rect, right.rect) && left.block_size == right.block_size;
}

bool SameShape(const ImgViewerEditShape& left, const ImgViewerEditShape& right)
{
    return left.kind == right.kind &&
        SameRect(left.rect, right.rect) &&
        SamePoint(left.start, right.start) &&
        SamePoint(left.end, right.end) &&
        std::abs(left.width - right.width) < 0.001f &&
        left.color.r == right.color.r &&
        left.color.g == right.color.g &&
        left.color.b == right.color.b &&
        left.color.a == right.color.a;
}

D2D1_RECT_F OffsetRect(D2D1_RECT_F rect, D2D1_POINT_2F offset)
{
    return D2D1::RectF(rect.left + offset.x, rect.top + offset.y, rect.right + offset.x, rect.bottom + offset.y);
}

ImgViewerEditShape OffsetShape(ImgViewerEditShape shape, D2D1_POINT_2F offset)
{
    shape.rect = OffsetRect(shape.rect, offset);
    shape.start = D2D1::Point2F(shape.start.x + offset.x, shape.start.y + offset.y);
    shape.end = D2D1::Point2F(shape.end.x + offset.x, shape.end.y + offset.y);
    return shape;
}

bool IsUsefulShape(const ImgViewerEditShape& shape)
{
    if (shape.kind == ImgViewerShapeKind::Rectangle || shape.kind == ImgViewerShapeKind::Ellipse) {
        return IsUsefulRect(shape.rect);
    }
    const float dx = shape.end.x - shape.start.x;
    const float dy = shape.end.y - shape.start.y;
    return dx * dx + dy * dy >= 4.0f;
}

float DistanceSquaredToSegment(D2D1_POINT_2F point, D2D1_POINT_2F a, D2D1_POINT_2F b);

bool HitTestShape(const ImgViewerEditShape& shape, D2D1_POINT_2F point, float hit_slop)
{
    if (!IsUsefulShape(shape)) {
        return false;
    }

    const float stroke_slop = hit_slop + shape.width * 0.5f;
    if (shape.kind == ImgViewerShapeKind::Line || shape.kind == ImgViewerShapeKind::Arrow) {
        return DistanceSquaredToSegment(point, shape.start, shape.end) <= stroke_slop * stroke_slop;
    }

    const D2D1_RECT_F rect = shape.rect;
    if (shape.kind == ImgViewerShapeKind::Rectangle) {
        const bool near_left = std::abs(point.x - rect.left) <= stroke_slop && point.y >= rect.top - stroke_slop && point.y <= rect.bottom + stroke_slop;
        const bool near_right = std::abs(point.x - rect.right) <= stroke_slop && point.y >= rect.top - stroke_slop && point.y <= rect.bottom + stroke_slop;
        const bool near_top = std::abs(point.y - rect.top) <= stroke_slop && point.x >= rect.left - stroke_slop && point.x <= rect.right + stroke_slop;
        const bool near_bottom = std::abs(point.y - rect.bottom) <= stroke_slop && point.x >= rect.left - stroke_slop && point.x <= rect.right + stroke_slop;
        return near_left || near_right || near_top || near_bottom;
    }

    const float rx = (rect.right - rect.left) * 0.5f;
    const float ry = (rect.bottom - rect.top) * 0.5f;
    if (rx <= 0.0f || ry <= 0.0f) {
        return false;
    }
    const float cx = (rect.left + rect.right) * 0.5f;
    const float cy = (rect.top + rect.bottom) * 0.5f;
    const float nx = (point.x - cx) / rx;
    const float ny = (point.y - cy) / ry;
    const float distance = std::sqrt(nx * nx + ny * ny);
    const float normalized_slop = stroke_slop / (std::max)(1.0f, (rx + ry) * 0.5f);
    return std::abs(distance - 1.0f) <= normalized_slop;
}

float DistanceSquaredToSegment(D2D1_POINT_2F point, D2D1_POINT_2F a, D2D1_POINT_2F b)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float length_squared = dx * dx + dy * dy;
    if (length_squared <= 0.0001f) {
        const float px = point.x - a.x;
        const float py = point.y - a.y;
        return px * px + py * py;
    }

    const float t = std::clamp(((point.x - a.x) * dx + (point.y - a.y) * dy) / length_squared, 0.0f, 1.0f);
    const D2D1_POINT_2F projected = D2D1::Point2F(a.x + dx * t, a.y + dy * t);
    const float px = point.x - projected.x;
    const float py = point.y - projected.y;
    return px * px + py * py;
}

D2D1_RECT_F PixelAlignedRect(D2D1_RECT_F rect, D2D1_SIZE_U bounds)
{
    return D2D1::RectF(
        std::clamp(std::floor(rect.left), 0.0f, static_cast<float>(bounds.width)),
        std::clamp(std::floor(rect.top), 0.0f, static_cast<float>(bounds.height)),
        std::clamp(std::ceil(rect.right), 0.0f, static_cast<float>(bounds.width)),
        std::clamp(std::ceil(rect.bottom), 0.0f, static_cast<float>(bounds.height)));
}

WICRect WicRectFromD2DRect(D2D1_RECT_F rect)
{
    const int left = static_cast<int>(std::floor(rect.left));
    const int top = static_cast<int>(std::floor(rect.top));
    return WICRect{
        left,
        top,
        (std::max)(1, static_cast<int>(std::ceil(rect.right)) - left),
        (std::max)(1, static_cast<int>(std::ceil(rect.bottom)) - top),
    };
}

void DrawMosaic(std::vector<BYTE>* pixels, UINT width, UINT height, D2D1_RECT_F rect, UINT block_size)
{
    if (pixels == nullptr || width == 0 || height == 0 || block_size == 0) {
        return;
    }

    const UINT left = (std::min)(width, static_cast<UINT>((std::max)(0.0f, std::floor(rect.left))));
    const UINT top = (std::min)(height, static_cast<UINT>((std::max)(0.0f, std::floor(rect.top))));
    const UINT right = (std::min)(width, static_cast<UINT>((std::max)(0.0f, std::ceil(rect.right))));
    const UINT bottom = (std::min)(height, static_cast<UINT>((std::max)(0.0f, std::ceil(rect.bottom))));
    for (UINT block_y = top; block_y < bottom; block_y += block_size) {
        for (UINT block_x = left; block_x < right; block_x += block_size) {
            const UINT block_right = (std::min)(right, block_x + block_size);
            const UINT block_bottom = (std::min)(bottom, block_y + block_size);
            UINT64 blue = 0;
            UINT64 green = 0;
            UINT64 red = 0;
            UINT64 alpha = 0;
            UINT64 count = 0;
            for (UINT y = block_y; y < block_bottom; ++y) {
                for (UINT x = block_x; x < block_right; ++x) {
                    const BYTE* pixel = pixels->data() + (static_cast<size_t>(y) * width + x) * 4;
                    blue += pixel[0];
                    green += pixel[1];
                    red += pixel[2];
                    alpha += pixel[3];
                    ++count;
                }
            }
            if (count == 0) {
                continue;
            }
            const BYTE average_blue = static_cast<BYTE>(blue / count);
            const BYTE average_green = static_cast<BYTE>(green / count);
            const BYTE average_red = static_cast<BYTE>(red / count);
            const BYTE average_alpha = static_cast<BYTE>(alpha / count);
            for (UINT y = block_y; y < block_bottom; ++y) {
                for (UINT x = block_x; x < block_right; ++x) {
                    BYTE* pixel = pixels->data() + (static_cast<size_t>(y) * width + x) * 4;
                    pixel[0] = average_blue;
                    pixel[1] = average_green;
                    pixel[2] = average_red;
                    pixel[3] = average_alpha;
                }
            }
        }
    }
}

D2D1_RECT_F TextLayoutRect(
    IDWriteFactory* dwrite_factory,
    const ImgViewerEditText& text,
    D2D1_POINT_2F origin)
{
    constexpr float kPaddingX = 6.0f;
    constexpr float kPaddingY = 4.0f;
    const float font_size = (std::max)(6.0f, text.style.font_size);
    const std::wstring text_to_measure = text.text.empty() ? L" " : text.text;
    float width = (std::max)(48.0f, static_cast<float>(text_to_measure.size()) * font_size * 0.55f + kPaddingX * 2.0f);
    float height = font_size * 1.35f + kPaddingY * 2.0f;

    if (dwrite_factory != nullptr) {
        wil::com_ptr<IDWriteTextFormat> format;
        if (SUCCEEDED(ui_text::CreateTextFormat(
                dwrite_factory,
                ui_text::TypeFace{
                    .family = text.style.font_family,
                    .size = font_size,
                    .weight = DWRITE_FONT_WEIGHT_NORMAL,
                },
                format.put()))) {
            wil::com_ptr<IDWriteTextLayout> layout;
            if (SUCCEEDED(dwrite_factory->CreateTextLayout(
                    text_to_measure.c_str(),
                    static_cast<UINT32>(text_to_measure.size()),
                    format.get(),
                    4096.0f,
                    4096.0f,
                    layout.put()))) {
                DWRITE_TEXT_METRICS metrics = {};
                if (SUCCEEDED(layout->GetMetrics(&metrics))) {
                    width = (std::max)(48.0f, metrics.widthIncludingTrailingWhitespace + kPaddingX * 2.0f);
                    height = (std::max)(font_size + kPaddingY * 2.0f, metrics.height + kPaddingY * 2.0f);
                }
            }
        }
    }

    return D2D1::RectF(origin.x, origin.y, origin.x + width, origin.y + height);
}

HRESULT DrawTextObject(ID2D1DeviceContext* render_target, IDWriteFactory* dwrite_factory, const ImgViewerEditText& text, D2D1_POINT_2F origin)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, render_target);
    RETURN_HR_IF_NULL(E_INVALIDARG, dwrite_factory);

    constexpr float kPaddingX = 6.0f;
    constexpr float kPaddingY = 4.0f;
    const D2D1_RECT_F rect = TextLayoutRect(dwrite_factory, text, origin);
    wil::com_ptr<ID2D1SolidColorBrush> brush;
    if (text.style.has_background) {
        RETURN_IF_FAILED(render_target->CreateSolidColorBrush(text.style.background_color, brush.put()));
        render_target->FillRectangle(rect, brush.get());
        brush.reset();
    }

    wil::com_ptr<IDWriteTextFormat> format;
    RETURN_IF_FAILED(ui_text::CreateTextFormat(
        dwrite_factory,
        ui_text::TypeFace{
            .family = text.style.font_family,
            .size = (std::max)(6.0f, text.style.font_size),
            .weight = DWRITE_FONT_WEIGHT_NORMAL,
        },
        format.put()));
    RETURN_IF_FAILED(render_target->CreateSolidColorBrush(text.style.text_color, brush.put()));
    const std::wstring text_to_draw = text.text.empty() ? L" " : text.text;
    render_target->DrawTextW(
        text_to_draw.c_str(),
        static_cast<UINT32>(text_to_draw.size()),
        format.get(),
        D2D1::RectF(rect.left + kPaddingX, rect.top + kPaddingY, rect.right - kPaddingX, rect.bottom - kPaddingY),
        brush.get(),
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
    return S_OK;
}

HRESULT DrawShapeObject(ID2D1DeviceContext* render_target, const ImgViewerEditShape& shape)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, render_target);

    wil::com_ptr<ID2D1SolidColorBrush> brush;
    RETURN_IF_FAILED(render_target->CreateSolidColorBrush(shape.color, brush.put()));
    const float width = (std::max)(1.0f, shape.width);
    switch (shape.kind) {
    case ImgViewerShapeKind::Rectangle:
        render_target->DrawRectangle(shape.rect, brush.get(), width);
        break;
    case ImgViewerShapeKind::Ellipse:
        render_target->DrawEllipse(
            D2D1::Ellipse(
                D2D1::Point2F((shape.rect.left + shape.rect.right) * 0.5f, (shape.rect.top + shape.rect.bottom) * 0.5f),
                (shape.rect.right - shape.rect.left) * 0.5f,
                (shape.rect.bottom - shape.rect.top) * 0.5f),
            brush.get(),
            width);
        break;
    case ImgViewerShapeKind::Line:
    case ImgViewerShapeKind::Arrow: {
        render_target->DrawLine(shape.start, shape.end, brush.get(), width);
        if (shape.kind == ImgViewerShapeKind::Arrow) {
            const float dx = shape.end.x - shape.start.x;
            const float dy = shape.end.y - shape.start.y;
            const float length = std::sqrt(dx * dx + dy * dy);
            if (length > 0.001f) {
                const float ux = dx / length;
                const float uy = dy / length;
                const float head = (std::max)(10.0f, width * 3.0f);
                const float wing = head * 0.55f;
                const D2D1_POINT_2F base = D2D1::Point2F(shape.end.x - ux * head, shape.end.y - uy * head);
                const D2D1_POINT_2F left = D2D1::Point2F(base.x - uy * wing, base.y + ux * wing);
                const D2D1_POINT_2F right = D2D1::Point2F(base.x + uy * wing, base.y - ux * wing);
                render_target->DrawLine(shape.end, left, brush.get(), width);
                render_target->DrawLine(shape.end, right, brush.get(), width);
            }
        }
        break;
    }
    }
    return S_OK;
}

} // namespace

bool ImgViewerEditController::Active() const
{
    return active_;
}

bool ImgViewerEditController::HasDocument() const
{
    return document_.source != nullptr;
}

bool ImgViewerEditController::Dirty() const
{
    return document_.dirty;
}

bool ImgViewerEditController::CanUndo() const
{
    return !undo_stack_.empty();
}

bool ImgViewerEditController::CanRedo() const
{
    return !redo_stack_.empty();
}

ImgViewerEditTool ImgViewerEditController::Tool() const
{
    return tool_;
}

D2D1_COLOR_F ImgViewerEditController::PenColor() const
{
    return pen_color_;
}

float ImgViewerEditController::PenWidth() const
{
    return pen_width_;
}

ImgViewerShapeKind ImgViewerEditController::ShapeKind() const
{
    return shape_kind_;
}

const ImgViewerTextStyle& ImgViewerEditController::TextStyle() const
{
    return text_style_;
}

bool ImgViewerEditController::IsEditingText() const
{
    return active_ && editing_text_ && editing_text_index_ < document_.texts.size();
}

bool ImgViewerEditController::IsDrawing() const
{
    return active_ && (drawing_stroke_ || drawing_shape_ || dragging_crop_edge_);
}

bool ImgViewerEditController::HasTransientCapture() const
{
    return IsDrawing() || drawing_pixel_selection_ || moving_selected_object_;
}

bool ImgViewerEditController::HasPixelSelection() const
{
    return active_ && has_pixel_selection_ && IsUsefulRect(pixel_selection_rect_);
}

bool ImgViewerEditController::HasCrop() const
{
    return HasDocument() && document_.has_crop && IsUsefulRect(document_.crop_rect);
}

D2D1_RECT_F ImgViewerEditController::CropRect() const
{
    return document_.crop_rect;
}

bool ImgViewerEditController::HasSelection() const
{
    return active_ && has_selected_object_ && IsValidObject(selected_object_);
}

bool ImgViewerEditController::SourceIsHdr() const
{
    return document_.source_metadata.color_info.dynamic_range.high_dynamic_range;
}

ImgViewerEditObjectRef ImgViewerEditController::SelectedObject() const
{
    return HasSelection() ? selected_object_ : ImgViewerEditObjectRef{};
}

ImgViewerEditSnapshot ImgViewerEditController::Snapshot() const
{
    return ImgViewerEditSnapshot{
        .active = active_ && HasDocument(),
        .tool = tool_,
        .rotation_quadrants = document_.rotation_quadrants,
        .strokes = document_.strokes,
        .shapes = document_.shapes,
        .texts = document_.texts,
        .mosaics = document_.mosaics,
        .drawing_stroke = drawing_stroke_,
        .current_stroke = current_stroke_,
        .drawing_shape = drawing_shape_,
        .current_shape = current_shape_,
        .drawing_crop = dragging_crop_edge_,
        .has_crop = HasCrop(),
        .crop_rect = document_.crop_rect,
        .current_crop_rect = has_pending_crop_ ? pending_crop_rect_ : current_crop_rect_,
        .has_pending_crop = has_pending_crop_,
        .pending_crop_rect = pending_crop_rect_,
        .active_crop_edge = active_crop_edge_,
        .dragging_crop_edge = dragging_crop_edge_,
        .drawing_pixel_selection = drawing_pixel_selection_,
        .has_pixel_selection = has_pixel_selection_,
        .pixel_selection_rect = pixel_selection_rect_,
        .current_pixel_selection_rect = current_pixel_selection_rect_,
        .has_selected_object = HasSelection(),
        .selected_object = SelectedObject(),
        .editing_text = editing_text_,
        .editing_text_index = editing_text_index_,
        .editing_text_state = text_edit_,
    };
}

HRESULT ImgViewerEditController::Begin(
    IWICBitmapSource* source,
    D2D1_SIZE_U source_size,
    const ImageMetadata& source_metadata,
    std::wstring source_path)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, source);
    RETURN_HR_IF(E_INVALIDARG, source_size.width == 0 || source_size.height == 0);

    document_ = ImgViewerEditDocument{};
    document_.source = source;
    document_.source_size = source_size;
    document_.source_metadata = source_metadata;
    document_.source_path = std::move(source_path);
    undo_stack_.clear();
    redo_stack_.clear();
    pen_color_ = D2D1::ColorF(D2D1::ColorF::Red);
    pen_width_ = 4.0f;
    shape_kind_ = ImgViewerShapeKind::Rectangle;
    text_style_ = ImgViewerTextStyle{};
    current_stroke_ = ImgViewerEditStroke{};
    current_shape_ = ImgViewerEditShape{};
    drawing_stroke_ = false;
    drawing_shape_ = false;
    drawing_crop_ = false;
    has_pending_crop_ = false;
    dragging_crop_edge_ = false;
    drawing_pixel_selection_ = false;
    has_pixel_selection_ = false;
    editing_text_ = false;
    has_selected_object_ = false;
    moving_selected_object_ = false;
    editing_text_index_ = 0;
    text_edit_ = TextEditState{};
    text_edit_original_ = ImgViewerEditText{};
    text_edit_created_new_object_ = false;
    current_crop_rect_ = D2D1_RECT_F{};
    pending_crop_rect_ = D2D1_RECT_F{};
    crop_session_original_rect_ = D2D1_RECT_F{};
    crop_session_original_has_crop_ = false;
    active_crop_edge_ = ImgViewerCropEdge::None;
    dragging_crop_edge_kind_ = ImgViewerCropEdge::None;
    pixel_selection_rect_ = D2D1_RECT_F{};
    current_pixel_selection_rect_ = D2D1_RECT_F{};
    selected_object_ = ImgViewerEditObjectRef{};
    active_ = true;
    tool_ = ImgViewerEditTool::Pen;
    return S_OK;
}

void ImgViewerEditController::Clear()
{
    document_ = ImgViewerEditDocument{};
    undo_stack_.clear();
    redo_stack_.clear();
    pen_color_ = D2D1::ColorF(D2D1::ColorF::Red);
    pen_width_ = 4.0f;
    shape_kind_ = ImgViewerShapeKind::Rectangle;
    text_style_ = ImgViewerTextStyle{};
    current_stroke_ = ImgViewerEditStroke{};
    current_shape_ = ImgViewerEditShape{};
    drawing_stroke_ = false;
    drawing_shape_ = false;
    drawing_crop_ = false;
    has_pending_crop_ = false;
    dragging_crop_edge_ = false;
    drawing_pixel_selection_ = false;
    has_pixel_selection_ = false;
    editing_text_ = false;
    has_selected_object_ = false;
    moving_selected_object_ = false;
    editing_text_index_ = 0;
    text_edit_ = TextEditState{};
    text_edit_original_ = ImgViewerEditText{};
    text_edit_created_new_object_ = false;
    current_crop_rect_ = D2D1_RECT_F{};
    pending_crop_rect_ = D2D1_RECT_F{};
    crop_session_original_rect_ = D2D1_RECT_F{};
    crop_session_original_has_crop_ = false;
    active_crop_edge_ = ImgViewerCropEdge::None;
    dragging_crop_edge_kind_ = ImgViewerCropEdge::None;
    pixel_selection_rect_ = D2D1_RECT_F{};
    current_pixel_selection_rect_ = D2D1_RECT_F{};
    selected_object_ = ImgViewerEditObjectRef{};
    active_ = false;
    tool_ = ImgViewerEditTool::Select;
}

void ImgViewerEditController::SetActive(bool active)
{
    if (!active && active_ && tool_ == ImgViewerEditTool::Crop) {
        CommitCropSession();
    }
    if (!active && active_) {
        CommitTextEditSession();
    }
    active_ = active && HasDocument();
    if (active_ && tool_ == ImgViewerEditTool::Crop) {
        BeginCropSession();
    }
}

void ImgViewerEditController::SetTool(ImgViewerEditTool tool)
{
    if (tool_ == ImgViewerEditTool::Crop && tool != ImgViewerEditTool::Crop) {
        CommitCropSession();
    }
    if (tool_ != tool) {
        CommitTextEditSession();
    }
    tool_ = tool;
    drawing_stroke_ = false;
    drawing_shape_ = false;
    drawing_crop_ = false;
    dragging_crop_edge_ = false;
    dragging_crop_edge_kind_ = ImgViewerCropEdge::None;
    drawing_pixel_selection_ = false;
    moving_selected_object_ = false;
    current_stroke_ = ImgViewerEditStroke{};
    current_shape_ = ImgViewerEditShape{};
    current_pixel_selection_rect_ = D2D1_RECT_F{};
    if (tool_ == ImgViewerEditTool::Crop) {
        BeginCropSession();
    } else {
        active_crop_edge_ = ImgViewerCropEdge::None;
    }
    if (tool_ != ImgViewerEditTool::PixelSelect) {
        has_pixel_selection_ = false;
        pixel_selection_rect_ = D2D1_RECT_F{};
    }
    if (tool_ != ImgViewerEditTool::Select) {
        has_selected_object_ = false;
        selected_object_ = ImgViewerEditObjectRef{};
    }
}

void ImgViewerEditController::SetPenColor(D2D1_COLOR_F color)
{
    pen_color_ = color;
}

void ImgViewerEditController::SetPenWidth(float width)
{
    pen_width_ = std::clamp(width, 1.0f, 32.0f);
}

void ImgViewerEditController::SetShapeKind(ImgViewerShapeKind kind)
{
    shape_kind_ = kind;
}

void ImgViewerEditController::SetTextFontFamily(std::wstring font_family)
{
    if (font_family.empty()) {
        font_family = L"Segoe UI";
    }
    text_style_.font_family = std::move(font_family);
    if (IsEditingText()) {
        document_.texts[editing_text_index_].style = text_style_;
    }
}

void ImgViewerEditController::SetTextFontSize(float font_size)
{
    text_style_.font_size = std::clamp(font_size, 6.0f, 256.0f);
    if (IsEditingText()) {
        document_.texts[editing_text_index_].style = text_style_;
    }
}

void ImgViewerEditController::SetTextColor(D2D1_COLOR_F color)
{
    text_style_.text_color = color;
    if (IsEditingText()) {
        document_.texts[editing_text_index_].style = text_style_;
    }
}

void ImgViewerEditController::SetTextBackground(D2D1_COLOR_F color, bool has_background)
{
    text_style_.background_color = color;
    text_style_.has_background = has_background;
    if (IsEditingText()) {
        document_.texts[editing_text_index_].style = text_style_;
    }
}

void ImgViewerEditController::BeginCropSession()
{
    if (!HasDocument()) {
        return;
    }
    if (has_pending_crop_) {
        current_crop_rect_ = pending_crop_rect_;
        return;
    }

    crop_session_original_rect_ = document_.crop_rect;
    crop_session_original_has_crop_ = document_.has_crop;
    pending_crop_rect_ = document_.has_crop && IsUsefulRect(document_.crop_rect)
        ? document_.crop_rect
        : D2D1::RectF(0.0f, 0.0f, static_cast<float>(document_.source_size.width), static_cast<float>(document_.source_size.height));
    pending_crop_rect_ = ClampCropRect(pending_crop_rect_);
    current_crop_rect_ = pending_crop_rect_;
    has_pending_crop_ = true;
    dragging_crop_edge_ = false;
    dragging_crop_edge_kind_ = ImgViewerCropEdge::None;
    active_crop_edge_ = ImgViewerCropEdge::None;
}

bool ImgViewerEditController::CommitCropSession()
{
    if (!HasDocument() || !has_pending_crop_ || !IsUsefulRect(pending_crop_rect_)) {
        has_pending_crop_ = false;
        dragging_crop_edge_ = false;
        active_crop_edge_ = ImgViewerCropEdge::None;
        dragging_crop_edge_kind_ = ImgViewerCropEdge::None;
        return false;
    }

    const D2D1_RECT_F crop_rect = ClampCropRect(pending_crop_rect_);
    const D2D1_RECT_F default_crop_rect = D2D1::RectF(
        0.0f,
        0.0f,
        static_cast<float>(document_.source_size.width),
        static_cast<float>(document_.source_size.height));
    const bool changed = crop_session_original_has_crop_
        ? !SameRect(crop_rect, crop_session_original_rect_)
        : !SameRect(crop_rect, default_crop_rect);
    has_pending_crop_ = false;
    dragging_crop_edge_ = false;
    active_crop_edge_ = ImgViewerCropEdge::None;
    dragging_crop_edge_kind_ = ImgViewerCropEdge::None;
    current_crop_rect_ = D2D1_RECT_F{};
    if (!changed) {
        return false;
    }

    PushHistory(HistoryEntry{
        .kind = HistoryKind::Crop,
        .previous_crop_rect = crop_session_original_rect_,
        .previous_has_crop = crop_session_original_has_crop_,
        .crop_rect = crop_rect,
    });
    document_.crop_rect = crop_rect;
    document_.has_crop = true;
    document_.dirty = true;
    return true;
}

bool ImgViewerEditController::CancelCropSession()
{
    if (!has_pending_crop_ && !dragging_crop_edge_) {
        return false;
    }

    has_pending_crop_ = false;
    dragging_crop_edge_ = false;
    active_crop_edge_ = ImgViewerCropEdge::None;
    dragging_crop_edge_kind_ = ImgViewerCropEdge::None;
    current_crop_rect_ = D2D1_RECT_F{};
    return true;
}

bool ImgViewerEditController::RotateClockwise()
{
    if (!HasDocument()) {
        return false;
    }

    document_.rotation_quadrants = NormalizeQuadrants(document_.rotation_quadrants + 1);
    document_.dirty = true;
    PushHistory(HistoryEntry{.kind = HistoryKind::RotateClockwise});
    return true;
}

bool ImgViewerEditController::Undo()
{
    CommitTextEditSession();
    if (undo_stack_.empty()) {
        return false;
    }

    const HistoryEntry entry = undo_stack_.back();
    undo_stack_.pop_back();
    redo_stack_.push_back(entry);
    switch (entry.kind) {
    case HistoryKind::Stroke:
        if (!document_.strokes.empty()) {
            document_.strokes.pop_back();
        }
        break;
    case HistoryKind::Shape:
        if (!document_.shapes.empty()) {
            document_.shapes.pop_back();
        }
        break;
    case HistoryKind::Text:
        if (!document_.texts.empty()) {
            document_.texts.pop_back();
        }
        break;
    case HistoryKind::RotateClockwise:
        document_.rotation_quadrants = NormalizeQuadrants(document_.rotation_quadrants - 1);
        break;
    case HistoryKind::Crop:
        document_.has_crop = entry.previous_has_crop;
        document_.crop_rect = entry.previous_crop_rect;
        break;
    case HistoryKind::Mosaic:
        if (!document_.mosaics.empty()) {
            document_.mosaics.pop_back();
        }
        break;
    case HistoryKind::DeleteObject:
        switch (entry.object.kind) {
        case ImgViewerEditObjectKind::Stroke:
            document_.strokes.insert(
                document_.strokes.begin() + static_cast<std::ptrdiff_t>((std::min)(entry.object.index, document_.strokes.size())),
                entry.stroke);
            break;
        case ImgViewerEditObjectKind::Shape:
            document_.shapes.insert(
                document_.shapes.begin() + static_cast<std::ptrdiff_t>((std::min)(entry.object.index, document_.shapes.size())),
                entry.shape);
            break;
        case ImgViewerEditObjectKind::Text:
            document_.texts.insert(
                document_.texts.begin() + static_cast<std::ptrdiff_t>((std::min)(entry.object.index, document_.texts.size())),
                entry.text);
            break;
        case ImgViewerEditObjectKind::Mosaic:
            document_.mosaics.insert(
                document_.mosaics.begin() + static_cast<std::ptrdiff_t>((std::min)(entry.object.index, document_.mosaics.size())),
                entry.mosaic);
            break;
        case ImgViewerEditObjectKind::None:
            break;
        }
        CancelSelection();
        break;
    case HistoryKind::MoveObject:
        switch (entry.object.kind) {
        case ImgViewerEditObjectKind::Stroke:
            if (entry.object.index < document_.strokes.size()) document_.strokes[entry.object.index] = entry.stroke;
            break;
        case ImgViewerEditObjectKind::Shape:
            if (entry.object.index < document_.shapes.size()) document_.shapes[entry.object.index] = entry.shape;
            break;
        case ImgViewerEditObjectKind::Text:
            if (entry.object.index < document_.texts.size()) document_.texts[entry.object.index] = entry.text;
            break;
        case ImgViewerEditObjectKind::Mosaic:
            if (entry.object.index < document_.mosaics.size()) document_.mosaics[entry.object.index] = entry.mosaic;
            break;
        case ImgViewerEditObjectKind::None:
            break;
        }
        CancelSelection();
        break;
    case HistoryKind::EditTextObject:
        if (entry.object.index < document_.texts.size()) {
            document_.texts[entry.object.index] = entry.text;
        }
        CancelSelection();
        break;
    }
    document_.dirty = !undo_stack_.empty();
    return true;
}

bool ImgViewerEditController::Redo()
{
    CommitTextEditSession();
    if (redo_stack_.empty()) {
        return false;
    }

    const HistoryEntry entry = redo_stack_.back();
    redo_stack_.pop_back();
    switch (entry.kind) {
    case HistoryKind::Stroke:
        document_.strokes.push_back(entry.stroke);
        break;
    case HistoryKind::Shape:
        document_.shapes.push_back(entry.shape);
        break;
    case HistoryKind::Text:
        document_.texts.push_back(entry.text);
        break;
    case HistoryKind::RotateClockwise:
        document_.rotation_quadrants = NormalizeQuadrants(document_.rotation_quadrants + 1);
        break;
    case HistoryKind::Crop:
        document_.has_crop = true;
        document_.crop_rect = entry.crop_rect;
        break;
    case HistoryKind::Mosaic:
        document_.mosaics.push_back(entry.mosaic);
        break;
    case HistoryKind::DeleteObject:
        switch (entry.object.kind) {
        case ImgViewerEditObjectKind::Stroke:
            if (entry.object.index < document_.strokes.size()) {
                document_.strokes.erase(document_.strokes.begin() + static_cast<std::ptrdiff_t>(entry.object.index));
            }
            break;
        case ImgViewerEditObjectKind::Shape:
            if (entry.object.index < document_.shapes.size()) {
                document_.shapes.erase(document_.shapes.begin() + static_cast<std::ptrdiff_t>(entry.object.index));
            }
            break;
        case ImgViewerEditObjectKind::Text:
            if (entry.object.index < document_.texts.size()) {
                document_.texts.erase(document_.texts.begin() + static_cast<std::ptrdiff_t>(entry.object.index));
            }
            break;
        case ImgViewerEditObjectKind::Mosaic:
            if (entry.object.index < document_.mosaics.size()) {
                document_.mosaics.erase(document_.mosaics.begin() + static_cast<std::ptrdiff_t>(entry.object.index));
            }
            break;
        case ImgViewerEditObjectKind::None:
            break;
        }
        CancelSelection();
        break;
    case HistoryKind::MoveObject:
        switch (entry.object.kind) {
        case ImgViewerEditObjectKind::Stroke:
            if (entry.object.index < document_.strokes.size()) document_.strokes[entry.object.index] = entry.after_stroke;
            break;
        case ImgViewerEditObjectKind::Shape:
            if (entry.object.index < document_.shapes.size()) document_.shapes[entry.object.index] = entry.after_shape;
            break;
        case ImgViewerEditObjectKind::Text:
            if (entry.object.index < document_.texts.size()) document_.texts[entry.object.index] = entry.after_text;
            break;
        case ImgViewerEditObjectKind::Mosaic:
            if (entry.object.index < document_.mosaics.size()) document_.mosaics[entry.object.index] = entry.after_mosaic;
            break;
        case ImgViewerEditObjectKind::None:
            break;
        }
        CancelSelection();
        break;
    case HistoryKind::EditTextObject:
        if (entry.object.index < document_.texts.size()) {
            document_.texts[entry.object.index] = entry.after_text;
        }
        CancelSelection();
        break;
    }
    undo_stack_.push_back(entry);
    document_.dirty = true;
    return true;
}

void ImgViewerEditController::MarkSaved()
{
    document_.dirty = false;
}

void ImgViewerEditController::CancelTransientTool()
{
    CommitTextEditSession();
    current_stroke_ = ImgViewerEditStroke{};
    current_shape_ = ImgViewerEditShape{};
    drawing_stroke_ = false;
    drawing_shape_ = false;
    drawing_crop_ = false;
    dragging_crop_edge_ = false;
    dragging_crop_edge_kind_ = ImgViewerCropEdge::None;
    drawing_pixel_selection_ = false;
    moving_selected_object_ = false;
    editing_text_index_ = 0;
    current_crop_rect_ = has_pending_crop_ ? pending_crop_rect_ : D2D1_RECT_F{};
    current_pixel_selection_rect_ = D2D1_RECT_F{};
}

void ImgViewerEditController::CancelSelection()
{
    has_selected_object_ = false;
    moving_selected_object_ = false;
    selected_object_ = ImgViewerEditObjectRef{};
}

void ImgViewerEditController::ClearPixelSelection()
{
    has_pixel_selection_ = false;
    drawing_pixel_selection_ = false;
    pixel_selection_rect_ = D2D1_RECT_F{};
    current_pixel_selection_rect_ = D2D1_RECT_F{};
}

bool ImgViewerEditController::DeleteSelection()
{
    CommitTextEditSession();
    if (!HasSelection()) {
        return false;
    }

    const ImgViewerEditObjectRef object = selected_object_;
    HistoryEntry entry{
        .kind = HistoryKind::DeleteObject,
        .object = object,
    };
    switch (object.kind) {
    case ImgViewerEditObjectKind::Stroke:
        entry.stroke = document_.strokes[object.index];
        document_.strokes.erase(document_.strokes.begin() + static_cast<std::ptrdiff_t>(object.index));
        break;
    case ImgViewerEditObjectKind::Shape:
        entry.shape = document_.shapes[object.index];
        document_.shapes.erase(document_.shapes.begin() + static_cast<std::ptrdiff_t>(object.index));
        break;
    case ImgViewerEditObjectKind::Text:
        entry.text = document_.texts[object.index];
        document_.texts.erase(document_.texts.begin() + static_cast<std::ptrdiff_t>(object.index));
        break;
    case ImgViewerEditObjectKind::Mosaic:
        entry.mosaic = document_.mosaics[object.index];
        document_.mosaics.erase(document_.mosaics.begin() + static_cast<std::ptrdiff_t>(object.index));
        break;
    case ImgViewerEditObjectKind::None:
        return false;
    }

    CancelSelection();
    editing_text_ = false;
    document_.dirty = true;
    PushHistory(entry);
    return true;
}

bool ImgViewerEditController::BeginTextEditOnSelection()
{
    if (!HasSelection() || selected_object_.kind != ImgViewerEditObjectKind::Text) {
        return false;
    }

    return BeginTextEditSession(selected_object_.index, false);
}

bool ImgViewerEditController::ExecuteTextEditAction(UiAction action, HWND hwnd)
{
    if (!IsEditingText()) {
        return false;
    }

    if (action == kUiActionTextCopy) {
        const std::wstring selected = text_edit_.SelectedText();
        return !selected.empty() && win32::CopyTextToClipboard(hwnd, selected.c_str());
    }

    if (action == kUiActionTextCut) {
        const std::wstring selected = text_edit_.SelectedText();
        if (selected.empty() || !win32::CopyTextToClipboard(hwnd, selected.c_str())) {
            return false;
        }
        const bool changed = text_edit_.Delete();
        SyncTextEditObject();
        return changed;
    }

    if (action == kUiActionTextPaste) {
        std::wstring text;
        if (!win32::ReadClipboardText(hwnd, &text) || !text_edit_.InsertText(text)) {
            return false;
        }
        SyncTextEditObject();
        return true;
    }

    if (action == kUiActionTextSelectAll) {
        text_edit_.SelectAll();
        return true;
    }

    return false;
}

HRESULT ImgViewerEditController::CopySelectedPixels(IWICImagingFactory2* wic_factory, IWICBitmapSource** source) const
{
    RETURN_HR_IF_NULL(E_INVALIDARG, wic_factory);
    RETURN_HR_IF_NULL(E_POINTER, source);
    RETURN_HR_IF_NULL(E_UNEXPECTED, document_.source);
    RETURN_HR_IF(E_UNEXPECTED, !HasPixelSelection());

    wil::com_ptr<IWICBitmapClipper> clipper;
    RETURN_IF_FAILED(wic_factory->CreateBitmapClipper(clipper.put()));
    const WICRect rect = WicRectFromD2DRect(pixel_selection_rect_);
    RETURN_IF_FAILED(clipper->Initialize(document_.source.get(), &rect));

    wil::com_ptr<IWICBitmap> cached_bitmap;
    RETURN_IF_FAILED(wic_factory->CreateBitmapFromSource(clipper.get(), WICBitmapCacheOnLoad, cached_bitmap.put()));
    *source = cached_bitmap.detach();
    return S_OK;
}

bool ImgViewerEditController::MosaicSelection()
{
    if (!HasPixelSelection()) {
        return false;
    }

    ImgViewerEditMosaic mosaic{
        .rect = pixel_selection_rect_,
        .block_size = 12,
    };
    document_.mosaics.push_back(mosaic);
    document_.dirty = true;
    PushHistory(HistoryEntry{.kind = HistoryKind::Mosaic, .mosaic = mosaic});
    ClearPixelSelection();
    return true;
}

bool ImgViewerEditController::OnTextInput(wchar_t character)
{
    if (!IsEditingText()) {
        return false;
    }

    if (!text_edit_.InsertCharacter(character)) {
        return false;
    }

    SyncTextEditObject();
    return true;
}

bool ImgViewerEditController::OnTextKeyDown(UINT virtual_key, bool shift)
{
    if (!IsEditingText()) {
        return false;
    }

    if (virtual_key == VK_BACK) {
        text_edit_.Backspace();
        SyncTextEditObject();
        return true;
    }

    if (virtual_key == VK_DELETE) {
        text_edit_.Delete();
        SyncTextEditObject();
        return true;
    }

    if (virtual_key == VK_LEFT) {
        text_edit_.MoveLeft(shift);
        return true;
    }

    if (virtual_key == VK_RIGHT) {
        text_edit_.MoveRight(shift);
        return true;
    }

    if (virtual_key == VK_HOME) {
        text_edit_.MoveHome(shift);
        return true;
    }

    if (virtual_key == VK_END) {
        text_edit_.MoveEnd(shift);
        return true;
    }

    if (virtual_key == VK_ESCAPE) {
        CancelTextEditSession();
        return true;
    }

    if (virtual_key == VK_RETURN) {
        CommitTextEditSession();
        return true;
    }

    return false;
}

bool ImgViewerEditController::UpdateTextImeComposition(std::wstring composition)
{
    if (!IsEditingText()) {
        return false;
    }

    text_edit_.SetComposition(std::move(composition));
    return true;
}

bool ImgViewerEditController::CommitTextImeResult(std::wstring text)
{
    if (!IsEditingText()) {
        return false;
    }

    text_edit_.ClearComposition();
    if (!text_edit_.InsertText(text)) {
        return true;
    }

    SyncTextEditObject();
    return true;
}

bool ImgViewerEditController::EndTextImeComposition()
{
    if (!IsEditingText()) {
        return false;
    }

    text_edit_.ClearComposition();
    return true;
}

ImgViewerEventResult ImgViewerEditController::OnPointerDown(
    D2D1_POINT_2F point,
    const ImgViewerSnapshot& viewer,
    D2D1_SIZE_U viewport_size)
{
    if (!active_ || !HasDocument()) {
        return {};
    }

    D2D1_POINT_2F document_point = {};
    if (!DocumentPointFromViewportPoint(point, viewer, viewport_size, &document_point)) {
        return {};
    }

    if (tool_ == ImgViewerEditTool::Select) {
        CommitTextEditSession();
        ImgViewerEditObjectRef object;
        if (!HitTestObject(document_point, DocumentHitSlop(viewer, viewport_size), &object)) {
            const bool had_selection = HasSelection();
            CancelSelection();
            return ImgViewerEventResult{.handled = had_selection};
        }

        selected_object_ = object;
        has_selected_object_ = true;
        move_start_document_point_ = document_point;
        moving_selected_object_ = CaptureMoveOriginal(object);
        return ImgViewerEventResult{.handled = true, .captured = moving_selected_object_};
    }

    if (tool_ == ImgViewerEditTool::Pen) {
        current_stroke_ = ImgViewerEditStroke{
            .color = pen_color_,
            .width = pen_width_,
        };
        AppendStrokePoint(&current_stroke_, document_point);
        drawing_stroke_ = true;
        return ImgViewerEventResult{.handled = true, .captured = true};
    }

    if (tool_ == ImgViewerEditTool::Shape) {
        current_shape_ = ImgViewerEditShape{
            .kind = shape_kind_,
            .rect = NormalizedRect(document_point, document_point, document_.source_size),
            .start = document_point,
            .end = document_point,
            .color = pen_color_,
            .width = pen_width_,
        };
        drawing_shape_ = true;
        return ImgViewerEventResult{.handled = true, .captured = true};
    }

    if (tool_ == ImgViewerEditTool::PixelSelect) {
        pixel_selection_start_ = document_point;
        current_pixel_selection_rect_ = PixelAlignedRect(
            NormalizedRect(pixel_selection_start_, document_point, document_.source_size),
            document_.source_size);
        drawing_pixel_selection_ = true;
        has_pixel_selection_ = false;
        return ImgViewerEventResult{.handled = true, .captured = true};
    }

    if (tool_ == ImgViewerEditTool::Text) {
        CommitTextEditSession();
        ImgViewerEditText text{
            .origin = document_point,
            .text = L"",
            .style = text_style_,
        };
        document_.texts.push_back(text);
        BeginTextEditSession(document_.texts.size() - 1, true);
        return ImgViewerEventResult{.handled = true};
    }

    if (tool_ == ImgViewerEditTool::Crop) {
        BeginCropSession();
        const float hit_slop = DocumentHitSlop(viewer, viewport_size);
        const ImgViewerCropEdge edge = CropEdgeAt(document_point, hit_slop);
        if (edge != ImgViewerCropEdge::None) {
            active_crop_edge_ = edge;
            dragging_crop_edge_kind_ = edge;
            dragging_crop_edge_ = true;
            return ImgViewerEventResult{.handled = true, .captured = true};
        }

        crop_start_ = document_point;
        pending_crop_rect_ = NormalizedRect(crop_start_, document_point, document_.source_size);
        current_crop_rect_ = pending_crop_rect_;
        has_pending_crop_ = true;
        active_crop_edge_ = ImgViewerCropEdge::None;
        dragging_crop_edge_kind_ = ImgViewerCropEdge::None;
        dragging_crop_edge_ = true;
        return ImgViewerEventResult{.handled = true, .captured = true};
    }

    return {};
}

ImgViewerEventResult ImgViewerEditController::OnPointerMove(
    D2D1_POINT_2F point,
    const ImgViewerSnapshot& viewer,
    D2D1_SIZE_U viewport_size)
{
    if (!active_ ||
        (!drawing_stroke_ &&
            !drawing_shape_ &&
            !dragging_crop_edge_ &&
            !drawing_pixel_selection_ &&
            !moving_selected_object_ &&
            tool_ != ImgViewerEditTool::Crop)) {
        return {};
    }

    D2D1_POINT_2F document_point = {};
    if (!DocumentPointFromViewportPoint(point, viewer, viewport_size, &document_point)) {
        if (tool_ == ImgViewerEditTool::Crop && !dragging_crop_edge_ && active_crop_edge_ != ImgViewerCropEdge::None) {
            active_crop_edge_ = ImgViewerCropEdge::None;
            return ImgViewerEventResult{.handled = true};
        }
        return ImgViewerEventResult{.handled = true};
    }

    if (moving_selected_object_) {
        const D2D1_POINT_2F offset = D2D1::Point2F(
            document_point.x - move_start_document_point_.x,
            document_point.y - move_start_document_point_.y);
        return ImgViewerEventResult{
            .handled = true,
        };
    }

    if (drawing_shape_) {
        current_shape_.end = document_point;
        current_shape_.rect = NormalizedRect(current_shape_.start, document_point, document_.source_size);
        return ImgViewerEventResult{.handled = true};
    }

    if (tool_ == ImgViewerEditTool::Crop && dragging_crop_edge_) {
        if (dragging_crop_edge_kind_ == ImgViewerCropEdge::None) {
            pending_crop_rect_ = ClampCropRect(NormalizedRect(crop_start_, document_point, document_.source_size));
        } else {
            UpdatePendingCropEdge(document_point);
        }
        current_crop_rect_ = pending_crop_rect_;
        return ImgViewerEventResult{.handled = true};
    }

    if (tool_ == ImgViewerEditTool::Crop && has_pending_crop_) {
        const ImgViewerCropEdge edge = CropEdgeAt(document_point, DocumentHitSlop(viewer, viewport_size));
        active_crop_edge_ = edge;
        return ImgViewerEventResult{.handled = edge != ImgViewerCropEdge::None};
    }

    if (drawing_pixel_selection_) {
        current_pixel_selection_rect_ = PixelAlignedRect(
            NormalizedRect(pixel_selection_start_, document_point, document_.source_size),
            document_.source_size);
        return ImgViewerEventResult{.handled = true};
    }

    AppendStrokePoint(&current_stroke_, document_point);
    return ImgViewerEventResult{.handled = true};
}

ImgViewerEventResult ImgViewerEditController::OnPointerUp(
    D2D1_POINT_2F point,
    const ImgViewerSnapshot& viewer,
    D2D1_SIZE_U viewport_size)
{
    if (!active_ || (!drawing_stroke_ && !drawing_shape_ && !dragging_crop_edge_ && !drawing_pixel_selection_ && !moving_selected_object_)) {
        return {};
    }

    D2D1_POINT_2F document_point = {};
    if (moving_selected_object_) {
        if (DocumentPointFromViewportPoint(point, viewer, viewport_size, &document_point)) {
            const D2D1_POINT_2F offset = D2D1::Point2F(
                document_point.x - move_start_document_point_.x,
                document_point.y - move_start_document_point_.y);
            ApplyObjectOffset(selected_object_, offset);
        }
        CommitObjectMove();
        moving_selected_object_ = false;
        return ImgViewerEventResult{.handled = true, .released_capture = true};
    }

    if (drawing_stroke_ && DocumentPointFromViewportPoint(point, viewer, viewport_size, &document_point)) {
        AppendStrokePoint(&current_stroke_, document_point);
    }

    if (drawing_shape_) {
        if (DocumentPointFromViewportPoint(point, viewer, viewport_size, &document_point)) {
            current_shape_.end = document_point;
            current_shape_.rect = NormalizedRect(current_shape_.start, document_point, document_.source_size);
        }
        if (IsUsefulShape(current_shape_)) {
            document_.shapes.push_back(current_shape_);
            document_.dirty = true;
            PushHistory(HistoryEntry{.kind = HistoryKind::Shape, .shape = current_shape_});
        }
        current_shape_ = ImgViewerEditShape{};
        drawing_shape_ = false;
        return ImgViewerEventResult{.handled = true, .released_capture = true};
    }

    if (dragging_crop_edge_) {
        if (DocumentPointFromViewportPoint(point, viewer, viewport_size, &document_point)) {
            if (dragging_crop_edge_kind_ == ImgViewerCropEdge::None) {
                pending_crop_rect_ = ClampCropRect(NormalizedRect(crop_start_, document_point, document_.source_size));
            } else {
                UpdatePendingCropEdge(document_point);
            }
        }
        drawing_crop_ = false;
        dragging_crop_edge_ = false;
        dragging_crop_edge_kind_ = ImgViewerCropEdge::None;
        current_crop_rect_ = pending_crop_rect_;
        return ImgViewerEventResult{.handled = true, .released_capture = true};
    }

    if (drawing_pixel_selection_) {
        D2D1_RECT_F selection_rect = current_pixel_selection_rect_;
        if (DocumentPointFromViewportPoint(point, viewer, viewport_size, &document_point)) {
            selection_rect = PixelAlignedRect(
                NormalizedRect(pixel_selection_start_, document_point, document_.source_size),
                document_.source_size);
        }
        has_pixel_selection_ = IsUsefulRect(selection_rect);
        pixel_selection_rect_ = has_pixel_selection_ ? selection_rect : D2D1_RECT_F{};
        current_pixel_selection_rect_ = D2D1_RECT_F{};
        drawing_pixel_selection_ = false;
        return ImgViewerEventResult{.handled = true, .released_capture = true};
    }

    if (current_stroke_.points.size() > 1) {
        document_.strokes.push_back(current_stroke_);
        document_.dirty = true;
        PushHistory(HistoryEntry{.kind = HistoryKind::Stroke, .stroke = current_stroke_});
    }
    current_stroke_ = ImgViewerEditStroke{};
    drawing_stroke_ = false;
    return ImgViewerEventResult{.handled = true, .released_capture = true};
}

ImgViewerEventResult ImgViewerEditController::OnPointerDoubleClick(
    D2D1_POINT_2F point,
    const ImgViewerSnapshot& viewer,
    D2D1_SIZE_U viewport_size)
{
    if (!active_ || !HasDocument() || tool_ != ImgViewerEditTool::Select) {
        return {};
    }

    D2D1_POINT_2F document_point = {};
    if (!DocumentPointFromViewportPoint(point, viewer, viewport_size, &document_point)) {
        return {};
    }

    ImgViewerEditObjectRef object;
    if (!HitTestObject(document_point, DocumentHitSlop(viewer, viewport_size), &object)) {
        return {};
    }

    selected_object_ = object;
    has_selected_object_ = true;
    if (object.kind == ImgViewerEditObjectKind::Text) {
        BeginTextEditSession(object.index, false);
    }
    return ImgViewerEventResult{.handled = true};
}

HRESULT ImgViewerEditController::ExportPngSource(
    IWICImagingFactory2* wic_factory,
    GraphicsDevice* graphics,
    IWICBitmapSource** source) const
{
    RETURN_HR_IF_NULL(E_INVALIDARG, wic_factory);
    RETURN_HR_IF_NULL(E_INVALIDARG, graphics);
    RETURN_HR_IF_NULL(E_POINTER, source);
    RETURN_HR_IF_NULL(E_UNEXPECTED, document_.source);

    const UINT source_width = document_.source_size.width;
    const UINT source_height = document_.source_size.height;
    RETURN_HR_IF(E_UNEXPECTED, source_width == 0 || source_height == 0);

    std::vector<BYTE> source_pixels(static_cast<size_t>(source_width) * source_height * 4);
    const UINT source_stride = source_width * 4;
    RETURN_IF_FAILED(document_.source->CopyPixels(nullptr, source_stride, static_cast<UINT>(source_pixels.size()), source_pixels.data()));
    for (const ImgViewerEditMosaic& mosaic : document_.mosaics) {
        DrawMosaic(&source_pixels, source_width, source_height, mosaic.rect, mosaic.block_size);
    }

    const D2D1_RECT_F crop = document_.has_crop && IsUsefulRect(document_.crop_rect)
        ? document_.crop_rect
        : D2D1::RectF(0.0f, 0.0f, static_cast<float>(source_width), static_cast<float>(source_height));
    const UINT crop_left = static_cast<UINT>(std::floor(crop.left));
    const UINT crop_top = static_cast<UINT>(std::floor(crop.top));
    const UINT crop_right = (std::min)(source_width, static_cast<UINT>(std::ceil(crop.right)));
    const UINT crop_bottom = (std::min)(source_height, static_cast<UINT>(std::ceil(crop.bottom)));
    const UINT crop_width = (std::max)(1U, crop_right - crop_left);
    const UINT crop_height = (std::max)(1U, crop_bottom - crop_top);

    const int rotation = NormalizeQuadrants(document_.rotation_quadrants);
    const UINT output_width = (rotation % 2 == 0) ? crop_width : crop_height;
    const UINT output_height = (rotation % 2 == 0) ? crop_height : crop_width;
    std::vector<BYTE> output_pixels(static_cast<size_t>(output_width) * output_height * 4);

    for (UINT y = 0; y < crop_height; ++y) {
        for (UINT x = 0; x < crop_width; ++x) {
            UINT target_x = x;
            UINT target_y = y;
            if (rotation == 1) {
                target_x = crop_height - 1 - y;
                target_y = x;
            } else if (rotation == 2) {
                target_x = crop_width - 1 - x;
                target_y = crop_height - 1 - y;
            } else if (rotation == 3) {
                target_x = y;
                target_y = crop_width - 1 - x;
            }
            const BYTE* source_pixel = source_pixels.data() + (static_cast<size_t>(y + crop_top) * source_width + (x + crop_left)) * 4;
            BYTE* target_pixel = output_pixels.data() + (static_cast<size_t>(target_y) * output_width + target_x) * 4;
            target_pixel[0] = source_pixel[0];
            target_pixel[1] = source_pixel[1];
            target_pixel[2] = source_pixel[2];
            target_pixel[3] = source_pixel[3];
        }
    }

    for (const ImgViewerEditStroke& stroke : document_.strokes) {
        for (size_t index = 1; index < stroke.points.size(); ++index) {
            const D2D1_POINT_2F a = D2D1::Point2F(stroke.points[index - 1].x - crop.left, stroke.points[index - 1].y - crop.top);
            const D2D1_POINT_2F b = D2D1::Point2F(stroke.points[index].x - crop.left, stroke.points[index].y - crop.top);
            DrawLine(
                &output_pixels,
                output_width,
                output_height,
                RotateDocumentPoint(a, crop_width, crop_height, rotation),
                RotateDocumentPoint(b, crop_width, crop_height, rotation),
                stroke.width,
                stroke.color);
        }
    }
    wil::com_ptr<IWICBitmap> memory_bitmap;
    RETURN_IF_FAILED(wic_factory->CreateBitmapFromMemory(
        output_width,
        output_height,
        GUID_WICPixelFormat32bppPBGRA,
        output_width * 4,
        static_cast<UINT>(output_pixels.size()),
        output_pixels.data(),
        memory_bitmap.put()));

    if (!document_.shapes.empty() || !document_.texts.empty()) {
        D2D1_MATRIX_3X2_F transform = D2D1::Matrix3x2F::Identity();
        if (rotation == 1) {
            transform = D2D1::Matrix3x2F::Rotation(90.0f) *
                D2D1::Matrix3x2F::Translation(static_cast<float>(crop_height), 0.0f);
        } else if (rotation == 2) {
            transform = D2D1::Matrix3x2F::Rotation(180.0f) *
                D2D1::Matrix3x2F::Translation(static_cast<float>(crop_width), static_cast<float>(crop_height));
        } else if (rotation == 3) {
            transform = D2D1::Matrix3x2F::Rotation(270.0f) *
                D2D1::Matrix3x2F::Translation(0.0f, static_cast<float>(crop_width));
        }

        struct ExportRenderState final {
            const ImgViewerEditController* controller;
            IWICBitmap* base_bitmap;
            GraphicsDevice* graphics;
            D2D1_RECT_F crop;
            D2D1_MATRIX_3X2_F transform;
        } render_state{this, memory_bitmap.get(), graphics, crop, transform};

        return graphics->RenderTextureToWicBitmap(
            wic_factory,
            output_width,
            output_height,
            [](ID2D1DeviceContext* render_target, void* user_data) -> HRESULT {
                const auto* state = static_cast<const ExportRenderState*>(user_data);
                RETURN_HR_IF_NULL(E_INVALIDARG, state);
                RETURN_HR_IF_NULL(E_INVALIDARG, render_target);

                wil::com_ptr<ID2D1Bitmap> base_bitmap;
                RETURN_IF_FAILED(render_target->CreateBitmapFromWicBitmap(state->base_bitmap, base_bitmap.put()));
                render_target->SetTransform(D2D1::Matrix3x2F::Identity());
                render_target->DrawBitmap(base_bitmap.get());

                render_target->SetTransform(state->transform);
                for (const ImgViewerEditShape& shape : state->controller->document_.shapes) {
                    RETURN_IF_FAILED(DrawShapeObject(
                        render_target,
                        OffsetShape(shape, D2D1::Point2F(-state->crop.left, -state->crop.top))));
                }
                for (const ImgViewerEditText& text : state->controller->document_.texts) {
                    const D2D1_POINT_2F origin = D2D1::Point2F(text.origin.x - state->crop.left, text.origin.y - state->crop.top);
                    RETURN_IF_FAILED(DrawTextObject(render_target, state->graphics->DWriteFactory(), text, origin));
                }
                return S_OK;
            },
            &render_state,
            source);
    }

    wil::com_ptr<IWICBitmap> cached_bitmap;
    RETURN_IF_FAILED(wic_factory->CreateBitmapFromSource(memory_bitmap.get(), WICBitmapCacheOnLoad, cached_bitmap.put()));
    *source = cached_bitmap.detach();
    return S_OK;
}

bool ImgViewerEditController::DocumentPointFromViewportPoint(
    D2D1_POINT_2F point,
    const ImgViewerSnapshot& viewer,
    D2D1_SIZE_U viewport_size,
    D2D1_POINT_2F* document_point) const
{
    if (document_point == nullptr || viewport_size.width == 0 || viewport_size.height == 0 || viewer.pixel_size.width == 0 || viewer.pixel_size.height == 0) {
        return false;
    }

    const D2D1_SIZE_U preview_size =
        imgviewer_edit_geometry::EditPreviewSize(document_.source_size, document_.rotation_quadrants);
    const float image_scale = math::FitScale(preview_size, viewport_size) * viewer.zoom_multiplier;
    if (image_scale <= 0.0f) {
        return false;
    }

    const D2D1_POINT_2F viewport_center = D2D1::Point2F(
        static_cast<float>(viewport_size.width) * 0.5f,
        static_cast<float>(viewport_size.height) * 0.5f);
    const D2D1_POINT_2F screen_delta = D2D1::Point2F(point.x - viewport_center.x, point.y - viewport_center.y);
    const float flip_x = viewer.flipped_horizontal ? -1.0f : 1.0f;
    const float flip_y = viewer.flipped_vertical ? -1.0f : 1.0f;
    const D2D1_POINT_2F image_delta = math::TransformVector(
        D2D1::Matrix3x2F::Rotation(-viewer.rotation_degrees) *
            D2D1::Matrix3x2F::Scale(flip_x, flip_y),
        screen_delta);
    const D2D1_POINT_2F preview_view_center =
        imgviewer_edit_geometry::SourcePointToEditPreviewPoint(
            viewer.view_center,
            document_.source_size,
            document_.rotation_quadrants);
    const D2D1_POINT_2F preview_point = D2D1::Point2F(
        preview_view_center.x + image_delta.x / image_scale,
        preview_view_center.y + image_delta.y / image_scale);
    *document_point = imgviewer_edit_geometry::EditPreviewPointToSourcePoint(
        preview_point,
        document_.source_size,
        document_.rotation_quadrants);
    return document_point->x >= 0.0f &&
        document_point->y >= 0.0f &&
        document_point->x < static_cast<float>(document_.source_size.width) &&
        document_point->y < static_cast<float>(document_.source_size.height);
}

float ImgViewerEditController::DocumentHitSlop(const ImgViewerSnapshot& viewer, D2D1_SIZE_U viewport_size) const
{
    if (viewport_size.width == 0 || viewport_size.height == 0 || viewer.pixel_size.width == 0 || viewer.pixel_size.height == 0) {
        return 8.0f;
    }

    const D2D1_SIZE_U preview_size =
        imgviewer_edit_geometry::EditPreviewSize(document_.source_size, document_.rotation_quadrants);
    const float image_scale = math::FitScale(preview_size, viewport_size) * viewer.zoom_multiplier;
    return image_scale > 0.0f ? (std::max)(2.0f, 8.0f / image_scale) : 8.0f;
}

bool ImgViewerEditController::HitTestObject(
    D2D1_POINT_2F document_point,
    float hit_slop,
    ImgViewerEditObjectRef* object) const
{
    if (object == nullptr) {
        return false;
    }

    for (size_t offset = 0; offset < document_.texts.size(); ++offset) {
        const size_t index = document_.texts.size() - 1 - offset;
        if (math::Contains(TextLayoutRect(nullptr, document_.texts[index], document_.texts[index].origin), document_point)) {
            *object = ImgViewerEditObjectRef{.kind = ImgViewerEditObjectKind::Text, .index = index};
            return true;
        }
    }

    for (size_t offset = 0; offset < document_.mosaics.size(); ++offset) {
        const size_t index = document_.mosaics.size() - 1 - offset;
        if (math::Contains(document_.mosaics[index].rect, document_point)) {
            *object = ImgViewerEditObjectRef{.kind = ImgViewerEditObjectKind::Mosaic, .index = index};
            return true;
        }
    }

    for (size_t offset = 0; offset < document_.shapes.size(); ++offset) {
        const size_t index = document_.shapes.size() - 1 - offset;
        if (HitTestShape(document_.shapes[index], document_point, hit_slop)) {
            *object = ImgViewerEditObjectRef{.kind = ImgViewerEditObjectKind::Shape, .index = index};
            return true;
        }
    }

    const float slop_squared = hit_slop * hit_slop;
    for (size_t offset = 0; offset < document_.strokes.size(); ++offset) {
        const size_t index = document_.strokes.size() - 1 - offset;
        const ImgViewerEditStroke& stroke = document_.strokes[index];
        const float stroke_slop = hit_slop + stroke.width * 0.5f;
        const float stroke_slop_squared = stroke_slop * stroke_slop;
        if (stroke.points.size() == 1 && DistanceSquaredToSegment(document_point, stroke.points[0], stroke.points[0]) <= slop_squared) {
            *object = ImgViewerEditObjectRef{.kind = ImgViewerEditObjectKind::Stroke, .index = index};
            return true;
        }
        for (size_t point_index = 1; point_index < stroke.points.size(); ++point_index) {
            if (DistanceSquaredToSegment(document_point, stroke.points[point_index - 1], stroke.points[point_index]) <= stroke_slop_squared) {
                *object = ImgViewerEditObjectRef{.kind = ImgViewerEditObjectKind::Stroke, .index = index};
                return true;
            }
        }
    }

    *object = ImgViewerEditObjectRef{};
    return false;
}

bool ImgViewerEditController::IsValidObject(ImgViewerEditObjectRef object) const
{
    switch (object.kind) {
    case ImgViewerEditObjectKind::Stroke:
        return object.index < document_.strokes.size();
    case ImgViewerEditObjectKind::Shape:
        return object.index < document_.shapes.size();
    case ImgViewerEditObjectKind::Text:
        return object.index < document_.texts.size();
    case ImgViewerEditObjectKind::Mosaic:
        return object.index < document_.mosaics.size();
    case ImgViewerEditObjectKind::None:
        return false;
    }
    return false;
}

bool ImgViewerEditController::CaptureMoveOriginal(ImgViewerEditObjectRef object)
{
    if (!IsValidObject(object)) {
        return false;
    }

    switch (object.kind) {
    case ImgViewerEditObjectKind::Stroke:
        move_original_stroke_ = document_.strokes[object.index];
        break;
    case ImgViewerEditObjectKind::Shape:
        move_original_shape_ = document_.shapes[object.index];
        break;
    case ImgViewerEditObjectKind::Text:
        move_original_text_ = document_.texts[object.index];
        break;
    case ImgViewerEditObjectKind::Mosaic:
        move_original_mosaic_ = document_.mosaics[object.index];
        break;
    case ImgViewerEditObjectKind::None:
        return false;
    }
    return true;
}

bool ImgViewerEditController::ApplyObjectOffset(ImgViewerEditObjectRef object, D2D1_POINT_2F offset)
{
    if (!IsValidObject(object)) {
        return false;
    }

    switch (object.kind) {
    case ImgViewerEditObjectKind::Stroke:
        document_.strokes[object.index] = move_original_stroke_;
        for (D2D1_POINT_2F& point : document_.strokes[object.index].points) {
            point.x = std::clamp(point.x + offset.x, 0.0f, static_cast<float>(document_.source_size.width));
            point.y = std::clamp(point.y + offset.y, 0.0f, static_cast<float>(document_.source_size.height));
        }
        return true;
    case ImgViewerEditObjectKind::Shape:
        document_.shapes[object.index] = OffsetShape(move_original_shape_, offset);
        return true;
    case ImgViewerEditObjectKind::Text:
        document_.texts[object.index] = move_original_text_;
        document_.texts[object.index].origin = D2D1::Point2F(
            std::clamp(move_original_text_.origin.x + offset.x, 0.0f, static_cast<float>(document_.source_size.width)),
            std::clamp(move_original_text_.origin.y + offset.y, 0.0f, static_cast<float>(document_.source_size.height)));
        return true;
    case ImgViewerEditObjectKind::Mosaic:
        document_.mosaics[object.index] = move_original_mosaic_;
        document_.mosaics[object.index].rect = ClampCropRect(OffsetRect(move_original_mosaic_.rect, offset));
        return true;
    case ImgViewerEditObjectKind::None:
        return false;
    }
    return false;
}

bool ImgViewerEditController::CommitObjectMove()
{
    if (!IsValidObject(selected_object_)) {
        return false;
    }

    HistoryEntry entry{
        .kind = HistoryKind::MoveObject,
        .object = selected_object_,
    };
    bool changed = false;
    switch (selected_object_.kind) {
    case ImgViewerEditObjectKind::Stroke:
        entry.stroke = move_original_stroke_;
        entry.after_stroke = document_.strokes[selected_object_.index];
        changed = !SameStroke(entry.stroke, entry.after_stroke);
        break;
    case ImgViewerEditObjectKind::Shape:
        entry.shape = move_original_shape_;
        entry.after_shape = document_.shapes[selected_object_.index];
        changed = !SameShape(entry.shape, entry.after_shape);
        break;
    case ImgViewerEditObjectKind::Text:
        entry.text = move_original_text_;
        entry.after_text = document_.texts[selected_object_.index];
        changed = !SameTextObject(entry.text, entry.after_text);
        break;
    case ImgViewerEditObjectKind::Mosaic:
        entry.mosaic = move_original_mosaic_;
        entry.after_mosaic = document_.mosaics[selected_object_.index];
        changed = !SameMosaic(entry.mosaic, entry.after_mosaic);
        break;
    case ImgViewerEditObjectKind::None:
        break;
    }

    if (!changed) {
        return false;
    }
    document_.dirty = true;
    PushHistory(entry);
    return true;
}

bool ImgViewerEditController::BeginTextEditSession(size_t index, bool created_new_object)
{
    if (!active_ || index >= document_.texts.size()) {
        return false;
    }

    if (IsEditingText() && editing_text_index_ == index) {
        return true;
    }
    CommitTextEditSession();

    editing_text_index_ = index;
    editing_text_ = true;
    text_edit_original_ = document_.texts[index];
    text_edit_created_new_object_ = created_new_object;
    text_edit_ = TextEditState{};
    text_edit_.SetText(document_.texts[index].text);
    text_edit_.MoveEnd(false);
    text_style_ = document_.texts[index].style;
    return true;
}

bool ImgViewerEditController::CommitTextEditSession()
{
    if (!IsEditingText()) {
        return false;
    }

    SyncTextEditObject();
    const ImgViewerEditText after = document_.texts[editing_text_index_];
    const size_t index = editing_text_index_;
    const bool created_new_object = text_edit_created_new_object_;
    const bool changed = !SameTextObject(text_edit_original_, after);
    editing_text_ = false;
    editing_text_index_ = 0;
    text_edit_ = TextEditState{};
    text_edit_created_new_object_ = false;

    if (created_new_object && after.text.empty()) {
        if (index < document_.texts.size()) {
            document_.texts.erase(document_.texts.begin() + static_cast<std::ptrdiff_t>(index));
        }
        CancelSelection();
        return true;
    }

    if (!changed && !created_new_object) {
        return false;
    }

    document_.dirty = true;
    if (created_new_object) {
        PushHistory(HistoryEntry{.kind = HistoryKind::Text, .text = after});
    } else {
        PushHistory(HistoryEntry{
            .kind = HistoryKind::EditTextObject,
            .object = ImgViewerEditObjectRef{.kind = ImgViewerEditObjectKind::Text, .index = index},
            .text = text_edit_original_,
            .after_text = after,
        });
    }
    return true;
}

bool ImgViewerEditController::CancelTextEditSession()
{
    if (!IsEditingText()) {
        return false;
    }

    const size_t index = editing_text_index_;
    const bool created_new_object = text_edit_created_new_object_;
    if (created_new_object) {
        if (index < document_.texts.size()) {
            document_.texts.erase(document_.texts.begin() + static_cast<std::ptrdiff_t>(index));
        }
        CancelSelection();
    } else {
        document_.texts[index] = text_edit_original_;
    }
    editing_text_ = false;
    editing_text_index_ = 0;
    text_edit_ = TextEditState{};
    text_edit_created_new_object_ = false;
    return true;
}

void ImgViewerEditController::SyncTextEditObject()
{
    if (!IsEditingText()) {
        return;
    }
    document_.texts[editing_text_index_].text = text_edit_.Text();
    document_.texts[editing_text_index_].style = text_style_;
}

ImgViewerCropEdge ImgViewerEditController::CropEdgeAt(D2D1_POINT_2F document_point, float hit_slop) const
{
    if (!has_pending_crop_ || !IsUsefulRect(pending_crop_rect_)) {
        return ImgViewerCropEdge::None;
    }

    const bool within_vertical_span =
        document_point.y >= pending_crop_rect_.top - hit_slop &&
        document_point.y <= pending_crop_rect_.bottom + hit_slop;
    const bool within_horizontal_span =
        document_point.x >= pending_crop_rect_.left - hit_slop &&
        document_point.x <= pending_crop_rect_.right + hit_slop;
    const float left_distance = std::abs(document_point.x - pending_crop_rect_.left);
    const float right_distance = std::abs(document_point.x - pending_crop_rect_.right);
    const float top_distance = std::abs(document_point.y - pending_crop_rect_.top);
    const float bottom_distance = std::abs(document_point.y - pending_crop_rect_.bottom);

    ImgViewerCropEdge edge = ImgViewerCropEdge::None;
    float best_distance = hit_slop;
    if (within_vertical_span && left_distance <= best_distance) {
        edge = ImgViewerCropEdge::Left;
        best_distance = left_distance;
    }
    if (within_vertical_span && right_distance <= best_distance) {
        edge = ImgViewerCropEdge::Right;
        best_distance = right_distance;
    }
    if (within_horizontal_span && top_distance <= best_distance) {
        edge = ImgViewerCropEdge::Top;
        best_distance = top_distance;
    }
    if (within_horizontal_span && bottom_distance <= best_distance) {
        edge = ImgViewerCropEdge::Bottom;
    }
    return edge;
}

D2D1_RECT_F ImgViewerEditController::ClampCropRect(D2D1_RECT_F rect) const
{
    constexpr float kMinimumCropSize = 2.0f;
    const float width = static_cast<float>(document_.source_size.width);
    const float height = static_cast<float>(document_.source_size.height);
    rect.left = std::clamp(rect.left, 0.0f, (std::max)(0.0f, width - kMinimumCropSize));
    rect.top = std::clamp(rect.top, 0.0f, (std::max)(0.0f, height - kMinimumCropSize));
    rect.right = std::clamp(rect.right, rect.left + kMinimumCropSize, width);
    rect.bottom = std::clamp(rect.bottom, rect.top + kMinimumCropSize, height);
    return PixelAlignedRect(rect, document_.source_size);
}

void ImgViewerEditController::UpdatePendingCropEdge(D2D1_POINT_2F document_point)
{
    constexpr float kMinimumCropSize = 2.0f;
    D2D1_RECT_F rect = pending_crop_rect_;
    const float width = static_cast<float>(document_.source_size.width);
    const float height = static_cast<float>(document_.source_size.height);
    switch (dragging_crop_edge_kind_) {
    case ImgViewerCropEdge::Left:
        rect.left = std::clamp(document_point.x, 0.0f, rect.right - kMinimumCropSize);
        break;
    case ImgViewerCropEdge::Right:
        rect.right = std::clamp(document_point.x, rect.left + kMinimumCropSize, width);
        break;
    case ImgViewerCropEdge::Top:
        rect.top = std::clamp(document_point.y, 0.0f, rect.bottom - kMinimumCropSize);
        break;
    case ImgViewerCropEdge::Bottom:
        rect.bottom = std::clamp(document_point.y, rect.top + kMinimumCropSize, height);
        break;
    case ImgViewerCropEdge::None:
        break;
    }
    pending_crop_rect_ = ClampCropRect(rect);
    active_crop_edge_ = dragging_crop_edge_kind_;
}

void ImgViewerEditController::PushHistory(HistoryEntry entry)
{
    undo_stack_.push_back(std::move(entry));
    redo_stack_.clear();
}
