#include "imgviewer.edit.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

#include <d2d1helper.h>
#include <wil/result_macros.h>

#include "math.hpp"

namespace {

constexpr BYTE kTextMarkerAlpha = 210;
constexpr BYTE kTextMarkerBlue = 32;
constexpr BYTE kTextMarkerGreen = 224;
constexpr BYTE kTextMarkerRed = 255;

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

void DrawRectMarker(std::vector<BYTE>* pixels, UINT width, UINT height, D2D1_POINT_2F origin, size_t text_length)
{
    const int left = static_cast<int>(std::floor(origin.x));
    const int top = static_cast<int>(std::floor(origin.y));
    const int rect_width = (std::max)(48, static_cast<int>(text_length) * 8 + 12);
    constexpr int kRectHeight = 24;
    for (int y = top; y < top + kRectHeight; ++y) {
        if (y < 0 || y >= static_cast<int>(height)) {
            continue;
        }
        for (int x = left; x < left + rect_width; ++x) {
            if (x < 0 || x >= static_cast<int>(width)) {
                continue;
            }
            BYTE* pixel = pixels->data() + (static_cast<size_t>(y) * width + static_cast<size_t>(x)) * 4;
            BlendPixel(pixel, kTextMarkerBlue, kTextMarkerGreen, kTextMarkerRed, kTextMarkerAlpha);
        }
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

ImgViewerEditSnapshot ImgViewerEditController::Snapshot() const
{
    return ImgViewerEditSnapshot{
        .active = active_ && HasDocument(),
        .tool = tool_,
        .rotation_quadrants = document_.rotation_quadrants,
        .strokes = document_.strokes,
        .texts = document_.texts,
        .drawing_stroke = drawing_stroke_,
        .current_stroke = current_stroke_,
        .drawing_crop = drawing_crop_,
        .crop_rect = document_.crop_rect,
        .current_crop_rect = current_crop_rect_,
        .editing_text = editing_text_,
        .editing_text_index = editing_text_index_,
    };
}

HRESULT ImgViewerEditController::Begin(IWICBitmapSource* source, D2D1_SIZE_U source_size)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, source);
    RETURN_HR_IF(E_INVALIDARG, source_size.width == 0 || source_size.height == 0);

    document_ = ImgViewerEditDocument{};
    document_.source = source;
    document_.source_size = source_size;
    undo_stack_.clear();
    redo_stack_.clear();
    current_stroke_ = ImgViewerEditStroke{};
    drawing_stroke_ = false;
    drawing_crop_ = false;
    editing_text_ = false;
    editing_text_index_ = 0;
    current_crop_rect_ = D2D1_RECT_F{};
    active_ = true;
    tool_ = ImgViewerEditTool::Pen;
    return S_OK;
}

void ImgViewerEditController::Clear()
{
    document_ = ImgViewerEditDocument{};
    undo_stack_.clear();
    redo_stack_.clear();
    current_stroke_ = ImgViewerEditStroke{};
    drawing_stroke_ = false;
    drawing_crop_ = false;
    editing_text_ = false;
    editing_text_index_ = 0;
    current_crop_rect_ = D2D1_RECT_F{};
    active_ = false;
    tool_ = ImgViewerEditTool::Select;
}

void ImgViewerEditController::SetActive(bool active)
{
    active_ = active && HasDocument();
}

void ImgViewerEditController::SetTool(ImgViewerEditTool tool)
{
    tool_ = tool;
    drawing_stroke_ = false;
    drawing_crop_ = false;
    editing_text_ = false;
    current_stroke_ = ImgViewerEditStroke{};
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
    }
    document_.dirty = !undo_stack_.empty();
    return true;
}

bool ImgViewerEditController::Redo()
{
    if (redo_stack_.empty()) {
        return false;
    }

    const HistoryEntry entry = redo_stack_.back();
    redo_stack_.pop_back();
    switch (entry.kind) {
    case HistoryKind::Stroke:
        document_.strokes.push_back(entry.stroke);
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
    }
    undo_stack_.push_back(entry);
    document_.dirty = true;
    return true;
}

void ImgViewerEditController::MarkSaved()
{
    document_.dirty = false;
}

bool ImgViewerEditController::OnTextInput(wchar_t character)
{
    if (!active_ || !editing_text_ || editing_text_index_ >= document_.texts.size()) {
        return false;
    }

    if (character < L' ' || character == 0x7F) {
        return false;
    }

    document_.texts[editing_text_index_].text.push_back(character);
    document_.dirty = true;
    return true;
}

bool ImgViewerEditController::OnTextKeyDown(UINT virtual_key)
{
    if (!active_ || !editing_text_ || editing_text_index_ >= document_.texts.size()) {
        return false;
    }

    if (virtual_key == VK_BACK) {
        std::wstring& text = document_.texts[editing_text_index_].text;
        if (!text.empty()) {
            text.pop_back();
            document_.dirty = true;
        }
        return true;
    }

    if (virtual_key == VK_SPACE) {
        return true;
    }

    if (virtual_key == VK_ESCAPE || virtual_key == VK_RETURN) {
        editing_text_ = false;
        return true;
    }

    return false;
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

    if (tool_ == ImgViewerEditTool::Pen) {
        current_stroke_ = ImgViewerEditStroke{};
        current_stroke_.points.push_back(document_point);
        drawing_stroke_ = true;
        return ImgViewerEventResult{.handled = true, .needs_render = true, .captured = true};
    }

    if (tool_ == ImgViewerEditTool::Text) {
        ImgViewerEditText text{
            .origin = document_point,
            .text = L"",
        };
        document_.texts.push_back(text);
        document_.dirty = true;
        PushHistory(HistoryEntry{.kind = HistoryKind::Text, .text = text});
        editing_text_index_ = document_.texts.size() - 1;
        editing_text_ = true;
        return ImgViewerEventResult{.handled = true, .needs_render = true};
    }

    if (tool_ == ImgViewerEditTool::Crop) {
        crop_start_ = document_point;
        current_crop_rect_ = NormalizedRect(crop_start_, document_point, document_.source_size);
        drawing_crop_ = true;
        return ImgViewerEventResult{.handled = true, .needs_render = true, .captured = true};
    }

    return {};
}

ImgViewerEventResult ImgViewerEditController::OnPointerMove(
    D2D1_POINT_2F point,
    const ImgViewerSnapshot& viewer,
    D2D1_SIZE_U viewport_size)
{
    if (!active_ || (!drawing_stroke_ && !drawing_crop_)) {
        return {};
    }

    D2D1_POINT_2F document_point = {};
    if (!DocumentPointFromViewportPoint(point, viewer, viewport_size, &document_point)) {
        return ImgViewerEventResult{.handled = true};
    }

    if (drawing_crop_) {
        current_crop_rect_ = NormalizedRect(crop_start_, document_point, document_.source_size);
        return ImgViewerEventResult{.handled = true, .needs_render = true};
    }

    current_stroke_.points.push_back(document_point);
    return ImgViewerEventResult{.handled = true, .needs_render = true};
}

ImgViewerEventResult ImgViewerEditController::OnPointerUp(
    D2D1_POINT_2F point,
    const ImgViewerSnapshot& viewer,
    D2D1_SIZE_U viewport_size)
{
    if (!active_ || (!drawing_stroke_ && !drawing_crop_)) {
        return {};
    }

    D2D1_POINT_2F document_point = {};
    if (DocumentPointFromViewportPoint(point, viewer, viewport_size, &document_point)) {
        current_stroke_.points.push_back(document_point);
    }

    if (drawing_crop_) {
        D2D1_RECT_F crop_rect = current_crop_rect_;
        if (DocumentPointFromViewportPoint(point, viewer, viewport_size, &document_point)) {
            crop_rect = NormalizedRect(crop_start_, document_point, document_.source_size);
        }
        if (IsUsefulRect(crop_rect)) {
            PushHistory(HistoryEntry{
                .kind = HistoryKind::Crop,
                .previous_crop_rect = document_.crop_rect,
                .previous_has_crop = document_.has_crop,
                .crop_rect = crop_rect,
            });
            document_.crop_rect = crop_rect;
            document_.has_crop = true;
            document_.dirty = true;
        }
        drawing_crop_ = false;
        current_crop_rect_ = D2D1_RECT_F{};
        return ImgViewerEventResult{.handled = true, .needs_render = true, .released_capture = true};
    }

    if (current_stroke_.points.size() > 1) {
        document_.strokes.push_back(current_stroke_);
        document_.dirty = true;
        PushHistory(HistoryEntry{.kind = HistoryKind::Stroke, .stroke = current_stroke_});
    }
    current_stroke_ = ImgViewerEditStroke{};
    drawing_stroke_ = false;
    return ImgViewerEventResult{.handled = true, .needs_render = true, .released_capture = true};
}

HRESULT ImgViewerEditController::ExportPngSource(IWICImagingFactory2* wic_factory, IWICBitmapSource** source) const
{
    RETURN_HR_IF_NULL(E_INVALIDARG, wic_factory);
    RETURN_HR_IF_NULL(E_POINTER, source);
    RETURN_HR_IF_NULL(E_UNEXPECTED, document_.source);

    const UINT source_width = document_.source_size.width;
    const UINT source_height = document_.source_size.height;
    RETURN_HR_IF(E_UNEXPECTED, source_width == 0 || source_height == 0);

    std::vector<BYTE> source_pixels(static_cast<size_t>(source_width) * source_height * 4);
    const UINT source_stride = source_width * 4;
    RETURN_IF_FAILED(document_.source->CopyPixels(nullptr, source_stride, static_cast<UINT>(source_pixels.size()), source_pixels.data()));

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
    for (const ImgViewerEditText& text : document_.texts) {
        const D2D1_POINT_2F origin = D2D1::Point2F(text.origin.x - crop.left, text.origin.y - crop.top);
        DrawRectMarker(
            &output_pixels,
            output_width,
            output_height,
            RotateDocumentPoint(origin, crop_width, crop_height, rotation),
            text.text.size());
    }

    wil::com_ptr<IWICBitmap> memory_bitmap;
    RETURN_IF_FAILED(wic_factory->CreateBitmapFromMemory(
        output_width,
        output_height,
        GUID_WICPixelFormat32bppBGRA,
        output_width * 4,
        static_cast<UINT>(output_pixels.size()),
        output_pixels.data(),
        memory_bitmap.put()));

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

    const float image_scale = math::FitScale(viewer.pixel_size, viewport_size) * viewer.zoom_multiplier;
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
    *document_point = D2D1::Point2F(
        viewer.view_center.x + image_delta.x / image_scale,
        viewer.view_center.y + image_delta.y / image_scale);
    return document_point->x >= 0.0f &&
        document_point->y >= 0.0f &&
        document_point->x < static_cast<float>(document_.source_size.width) &&
        document_point->y < static_cast<float>(document_.source_size.height);
}

void ImgViewerEditController::PushHistory(HistoryEntry entry)
{
    undo_stack_.push_back(std::move(entry));
    redo_stack_.clear();
}
