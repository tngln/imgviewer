#include "imgviewer.renderer.hpp"

#include <algorithm>
#include <cwchar>
#include <cmath>
#include <string>

#include <d2d1helper.h>
#include <wil/com.h>
#include <wil/result_macros.h>

#include "math.hpp"
#include "imgviewer.edit_geometry.hpp"
#include "ui.draw.hpp"
#include "ui.theme.hpp"

namespace {

constexpr float kCheckerboardCellSize = 8.0f;

struct ImageLayerRenderState final {
    ImgViewerSnapshot image;
    ImgViewerEditSnapshot edit;
    bool checkerboard_background = false;
    float dpi_scale = 1.0f;
};

struct EditLayerRenderState final {
    ImgViewerSnapshot image;
    ImgViewerEditSnapshot edit;
};

HRESULT DrawCheckerboard(ID2D1DeviceContext* context, D2D1_SIZE_F size, float cell_size)
{
    RETURN_HR_IF_NULL(E_POINTER, context);

    wil::com_ptr<ID2D1SolidColorBrush> light_brush;
    wil::com_ptr<ID2D1SolidColorBrush> dark_brush;
    RETURN_IF_FAILED(context->CreateSolidColorBrush(ui_theme::color::kCheckerboardLight, light_brush.put()));
    RETURN_IF_FAILED(context->CreateSolidColorBrush(ui_theme::color::kCheckerboardDark, dark_brush.put()));

    for (float y = 0.0f; y < size.height; y += cell_size) {
        const int row = static_cast<int>(y / cell_size);
        for (float x = 0.0f; x < size.width; x += cell_size) {
            const int column = static_cast<int>(x / cell_size);
            ID2D1SolidColorBrush* brush = ((row + column) % 2 == 0) ? light_brush.get() : dark_brush.get();
            context->FillRectangle(
                D2D1::RectF(
                    x,
                    y,
                    (std::min)(x + cell_size, size.width),
                    (std::min)(y + cell_size, size.height)),
                brush);
        }
    }

    return S_OK;
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
        if (SUCCEEDED(dwrite_factory->CreateTextFormat(
                text.style.font_family.c_str(),
                nullptr,
                DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL,
                DWRITE_FONT_STRETCH_NORMAL,
                font_size,
                L"",
                format.put()))) {
            format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
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

D2D1_RECT_F StrokeBounds(const ImgViewerEditStroke& stroke)
{
    if (stroke.points.empty()) {
        return D2D1::RectF();
    }

    float left = stroke.points[0].x;
    float top = stroke.points[0].y;
    float right = stroke.points[0].x;
    float bottom = stroke.points[0].y;
    for (const D2D1_POINT_2F point : stroke.points) {
        left = (std::min)(left, point.x);
        top = (std::min)(top, point.y);
        right = (std::max)(right, point.x);
        bottom = (std::max)(bottom, point.y);
    }

    const float padding = (std::max)(2.0f, stroke.width * 0.5f + 2.0f);
    return D2D1::RectF(left - padding, top - padding, right + padding, bottom + padding);
}

D2D1_RECT_F ShapeObjectBounds(const ImgViewerEditShape& shape)
{
    if (shape.kind == ImgViewerShapeKind::Rectangle || shape.kind == ImgViewerShapeKind::Ellipse) {
        const float padding = (std::max)(2.0f, shape.width * 0.5f + 2.0f);
        return D2D1::RectF(
            shape.rect.left - padding,
            shape.rect.top - padding,
            shape.rect.right + padding,
            shape.rect.bottom + padding);
    }

    const float padding = (std::max)(2.0f, shape.width * 0.5f + 8.0f);
    return D2D1::RectF(
        (std::min)(shape.start.x, shape.end.x) - padding,
        (std::min)(shape.start.y, shape.end.y) - padding,
        (std::max)(shape.start.x, shape.end.x) + padding,
        (std::max)(shape.start.y, shape.end.y) + padding);
}

HRESULT DrawEditShapeObject(ID2D1DeviceContext* d2d_context, const ImgViewerEditShape& shape)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, d2d_context);

    wil::com_ptr<ID2D1SolidColorBrush> brush;
    RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(shape.color, brush.put()));
    const float width = (std::max)(1.0f, shape.width);
    switch (shape.kind) {
    case ImgViewerShapeKind::Rectangle:
        d2d_context->DrawRectangle(shape.rect, brush.get(), width);
        break;
    case ImgViewerShapeKind::Ellipse:
        d2d_context->DrawEllipse(
            D2D1::Ellipse(
                D2D1::Point2F((shape.rect.left + shape.rect.right) * 0.5f, (shape.rect.top + shape.rect.bottom) * 0.5f),
                (shape.rect.right - shape.rect.left) * 0.5f,
                (shape.rect.bottom - shape.rect.top) * 0.5f),
            brush.get(),
            width);
        break;
    case ImgViewerShapeKind::Line:
    case ImgViewerShapeKind::Arrow: {
        d2d_context->DrawLine(shape.start, shape.end, brush.get(), width);
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
                d2d_context->DrawLine(shape.end, left, brush.get(), width);
                d2d_context->DrawLine(shape.end, right, brush.get(), width);
            }
        }
        break;
    }
    }
    return S_OK;
}

bool SelectedObjectRect(
    IDWriteFactory* dwrite_factory,
    const ImgViewerEditSnapshot& edit,
    D2D1_RECT_F* rect)
{
    if (rect == nullptr || !edit.has_selected_object) {
        return false;
    }

    const ImgViewerEditObjectRef object = edit.selected_object;
    switch (object.kind) {
    case ImgViewerEditObjectKind::Stroke:
        if (object.index < edit.strokes.size()) {
            *rect = StrokeBounds(edit.strokes[object.index]);
            return rect->right > rect->left && rect->bottom > rect->top;
        }
        return false;
    case ImgViewerEditObjectKind::Shape:
        if (object.index < edit.shapes.size()) {
            *rect = ShapeObjectBounds(edit.shapes[object.index]);
            return rect->right > rect->left && rect->bottom > rect->top;
        }
        return false;
    case ImgViewerEditObjectKind::Text:
        if (object.index < edit.texts.size()) {
            *rect = TextLayoutRect(dwrite_factory, edit.texts[object.index], edit.texts[object.index].origin);
            return true;
        }
        return false;
    case ImgViewerEditObjectKind::Mosaic:
        if (object.index < edit.mosaics.size()) {
            *rect = edit.mosaics[object.index].rect;
            return rect->right > rect->left && rect->bottom > rect->top;
        }
        return false;
    case ImgViewerEditObjectKind::None:
        return false;
    }
    return false;
}

HRESULT DrawEditTextObject(
    ID2D1DeviceContext* d2d_context,
    IDWriteFactory* dwrite_factory,
    const ImgViewerEditText& text,
    bool editing,
    const TextEditState* edit_state,
    float image_scale)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, d2d_context);
    RETURN_HR_IF_NULL(E_INVALIDARG, dwrite_factory);

    constexpr float kPaddingX = 6.0f;
    constexpr float kPaddingY = 4.0f;
    const D2D1_RECT_F rect = TextLayoutRect(dwrite_factory, text, text.origin);
    wil::com_ptr<ID2D1SolidColorBrush> brush;
    if (text.style.has_background) {
        RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(text.style.background_color, brush.put()));
        d2d_context->FillRectangle(rect, brush.get());
        brush.reset();
    }

    RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(
        editing ? ui_theme::color::kAccent : D2D1::ColorF(D2D1::ColorF::White, 0.7f),
        brush.put()));
    d2d_context->DrawRectangle(rect, brush.get(), 1.0f / (std::max)(0.01f, image_scale));
    brush.reset();

    wil::com_ptr<IDWriteTextFormat> format;
    RETURN_IF_FAILED(dwrite_factory->CreateTextFormat(
        text.style.font_family.c_str(),
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        (std::max)(6.0f, text.style.font_size),
        L"",
        format.put()));
    RETURN_IF_FAILED(format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
    const D2D1_RECT_F text_rect = D2D1::RectF(rect.left + kPaddingX, rect.top + kPaddingY, rect.right - kPaddingX, rect.bottom - kPaddingY);
    const std::wstring text_to_draw = editing && edit_state != nullptr
        ? (edit_state->DisplayText().empty() ? L" " : edit_state->DisplayText())
        : (text.text.empty() && editing ? L" " : text.text);
    wil::com_ptr<IDWriteTextLayout> layout;
    RETURN_IF_FAILED(dwrite_factory->CreateTextLayout(
        text_to_draw.c_str(),
        static_cast<UINT32>(text_to_draw.size()),
        format.get(),
        (std::max)(1.0f, rect.right - rect.left),
        4096.0f,
        layout.put()));

    if (editing && edit_state != nullptr && edit_state->HasSelection()) {
        RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(D2D1::ColorF(ui_theme::color::kAccent.r, ui_theme::color::kAccent.g, ui_theme::color::kAccent.b, 0.32f), brush.put()));
        for (const DWRITE_HIT_TEST_METRICS& metric : edit_state->SelectionMetrics(layout.get(), D2D1::Point2F(text_rect.left, text_rect.top))) {
            d2d_context->FillRectangle(
                D2D1::RectF(metric.left, metric.top, metric.left + metric.width, metric.top + metric.height),
                brush.get());
        }
        brush.reset();
    }

    RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(text.style.text_color, brush.put()));
    d2d_context->DrawTextLayout(D2D1::Point2F(text_rect.left, text_rect.top), layout.get(), brush.get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
    brush.reset();

    if (editing) {
        D2D1_POINT_2F caret_top = D2D1::Point2F(text_rect.left, text_rect.top);
        D2D1_POINT_2F caret_bottom = D2D1::Point2F(text_rect.left, rect.bottom - kPaddingY);
        if (edit_state != nullptr) {
            edit_state->CaretMetrics(layout.get(), D2D1::Point2F(text_rect.left, text_rect.top), &caret_top, &caret_bottom);
        }
        RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(text.style.text_color, brush.put()));
        d2d_context->DrawLine(
            caret_top,
            caret_bottom,
            brush.get(),
            1.0f / (std::max)(0.01f, image_scale));
    }
    return S_OK;
}

HRESULT DrawCropOverlay(
    ID2D1DeviceContext* d2d_context,
    IDWriteTextFormat* text_format,
    const ImgViewerEditSnapshot& edit,
    D2D1_RECT_F image_rect,
    float image_scale)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, d2d_context);

    const D2D1_RECT_F crop_rect = edit.has_pending_crop
        ? edit.pending_crop_rect
        : (edit.drawing_crop ? edit.current_crop_rect : edit.crop_rect);
    if (crop_rect.right <= crop_rect.left || crop_rect.bottom <= crop_rect.top) {
        return S_OK;
    }

    wil::com_ptr<ID2D1SolidColorBrush> brush;
    RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 0.42f), brush.put()));
    d2d_context->FillRectangle(D2D1::RectF(image_rect.left, image_rect.top, image_rect.right, crop_rect.top), brush.get());
    d2d_context->FillRectangle(D2D1::RectF(image_rect.left, crop_rect.bottom, image_rect.right, image_rect.bottom), brush.get());
    d2d_context->FillRectangle(D2D1::RectF(image_rect.left, crop_rect.top, crop_rect.left, crop_rect.bottom), brush.get());
    d2d_context->FillRectangle(D2D1::RectF(crop_rect.right, crop_rect.top, image_rect.right, crop_rect.bottom), brush.get());
    brush.reset();

    const float stroke_width = 2.0f / (std::max)(0.01f, image_scale);
    const auto draw_edge = [&](ImgViewerCropEdge edge, D2D1_POINT_2F a, D2D1_POINT_2F b) -> HRESULT {
        const bool active = edit.active_crop_edge == edge || (edit.dragging_crop_edge && edit.active_crop_edge == edge);
        RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(
            active ? ui_theme::color::kAccent : D2D1::ColorF(D2D1::ColorF::White, 0.95f),
            brush.put()));
        d2d_context->DrawLine(a, b, brush.get(), active ? stroke_width * 1.35f : stroke_width);
        brush.reset();
        return S_OK;
    };
    RETURN_IF_FAILED(draw_edge(ImgViewerCropEdge::Top, D2D1::Point2F(crop_rect.left, crop_rect.top), D2D1::Point2F(crop_rect.right, crop_rect.top)));
    RETURN_IF_FAILED(draw_edge(ImgViewerCropEdge::Right, D2D1::Point2F(crop_rect.right, crop_rect.top), D2D1::Point2F(crop_rect.right, crop_rect.bottom)));
    RETURN_IF_FAILED(draw_edge(ImgViewerCropEdge::Bottom, D2D1::Point2F(crop_rect.right, crop_rect.bottom), D2D1::Point2F(crop_rect.left, crop_rect.bottom)));
    RETURN_IF_FAILED(draw_edge(ImgViewerCropEdge::Left, D2D1::Point2F(crop_rect.left, crop_rect.bottom), D2D1::Point2F(crop_rect.left, crop_rect.top)));

    const int crop_width = (std::max)(1, static_cast<int>(std::ceil(crop_rect.right) - std::floor(crop_rect.left)));
    const int crop_height = (std::max)(1, static_cast<int>(std::ceil(crop_rect.bottom) - std::floor(crop_rect.top)));
    wchar_t size_text[64] = {};
    swprintf_s(size_text, L"%d x %d", crop_width, crop_height);

    const float label_padding = 6.0f / (std::max)(0.01f, image_scale);
    const float label_width = 92.0f / (std::max)(0.01f, image_scale);
    const float label_height = 24.0f / (std::max)(0.01f, image_scale);
    const D2D1_RECT_F label_rect = D2D1::RectF(
        crop_rect.left + label_padding,
        crop_rect.top + label_padding,
        (std::min)(crop_rect.right - label_padding, crop_rect.left + label_padding + label_width),
        (std::min)(crop_rect.bottom - label_padding, crop_rect.top + label_padding + label_height));
    if (label_rect.right > label_rect.left && label_rect.bottom > label_rect.top && text_format != nullptr) {
        RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 0.68f), brush.put()));
        d2d_context->FillRectangle(label_rect, brush.get());
        brush.reset();
        RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 0.96f), brush.put()));
        d2d_context->DrawTextW(
            size_text,
            static_cast<UINT32>(wcslen(size_text)),
            text_format,
            D2D1::RectF(label_rect.left + label_padding, label_rect.top, label_rect.right, label_rect.bottom),
            brush.get(),
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    return S_OK;
}

} // namespace

HRESULT ImgViewerRenderer::Initialize(HWND hwnd)
{
    RETURN_IF_FAILED(ui_renderer_.Initialize(hwnd));
    RETURN_IF_FAILED(ui_renderer_.RegisterSurface(
        UiSurfaceDescriptor{
            .name = L"image",
            .format = DXGI_FORMAT_B8G8R8A8_UNORM,
            .alpha_mode = DXGI_ALPHA_MODE_IGNORE,
            .z_order = 0,
            .auto_resize = true,
            .initially_visible = true,
        },
        &image_surface_));
    RETURN_IF_FAILED(ui_renderer_.RegisterSurface(
        UiSurfaceDescriptor{
            .name = L"edit-overlay",
            .alpha_mode = DXGI_ALPHA_MODE_PREMULTIPLIED,
            .z_order = 50,
            .auto_resize = true,
            .initially_visible = true,
        },
        &edit_surface_));
    RETURN_IF_FAILED(ui_renderer_.RegisterSurface(
        UiSurfaceDescriptor{
            .name = L"ui-overlay",
            .alpha_mode = DXGI_ALPHA_MODE_PREMULTIPLIED,
            .z_order = 100,
            .auto_resize = true,
            .initially_visible = true,
        },
        &ui_overlay_surface_));
    return ui_renderer_.Commit();
}

HRESULT ImgViewerRenderer::Resize()
{
    return ui_renderer_.Resize();
}

HRESULT ImgViewerRenderer::Render(const ImgViewerController& viewer, const ImgViewerEditController& edit, UiController& ui)
{
    const ImgViewerSnapshot image = viewer.Snapshot();
    const ImgViewerEditSnapshot edit_snapshot = edit.Snapshot();
    RETURN_IF_FAILED(RenderImageLayer(image, edit_snapshot));
    RETURN_IF_FAILED(RenderEditLayer(image, edit_snapshot));
    RETURN_IF_FAILED(ui_renderer_.RenderUiOverlay(ui_overlay_surface_, ui));
    return ui_renderer_.Commit();
}

HRESULT ImgViewerRenderer::SetUiOverlayVisible(bool visible)
{
    if (ui_overlay_visible_ == visible) {
        return S_OK;
    }

    RETURN_IF_FAILED(ui_renderer_.SetSurfaceVisible(ui_overlay_surface_, visible));
    ui_overlay_visible_ = visible;
    return S_OK;
}

void ImgViewerRenderer::SetCheckerboardBackground(bool enabled)
{
    checkerboard_background_ = enabled;
}

D2D1_SIZE_U ImgViewerRenderer::ViewportPixelSize() const
{
    return ui_renderer_.ViewportPixelSize();
}

ID2D1Factory1* ImgViewerRenderer::D2DFactory() const
{
    return ui_renderer_.D2DFactory();
}

IDWriteFactory* ImgViewerRenderer::DWriteFactory() const
{
    return ui_renderer_.DWriteFactory();
}

IDWriteTextFormat* ImgViewerRenderer::BodyTextFormat() const
{
    return ui_renderer_.BodyTextFormat();
}

IDWriteTextFormat* ImgViewerRenderer::IconTextFormat() const
{
    return ui_renderer_.IconTextFormat();
}

ID2D1DeviceContext* ImgViewerRenderer::BitmapDeviceContext() const
{
    return ui_renderer_.BitmapDeviceContext();
}

HRESULT ImgViewerRenderer::RenderImageLayer(const ImgViewerSnapshot& image, const ImgViewerEditSnapshot& edit)
{
    const DXGI_FORMAT image_surface_format = image.bitmap != nullptr
        ? image.display_format
        : DXGI_FORMAT_B8G8R8A8_UNORM;
    RETURN_IF_FAILED(ui_renderer_.SetSurfaceFormat(image_surface_, image_surface_format));

    ImageLayerRenderState state{
        .image = image,
        .edit = edit,
        .checkerboard_background = checkerboard_background_,
        .dpi_scale = ui_renderer_.DpiScale(),
    };
    return ui_renderer_.DrawSurface(
        image_surface_,
        [](const UiSurfaceDrawContext& context, void* user_data) -> HRESULT {
            const auto* state = static_cast<const ImageLayerRenderState*>(user_data);
            RETURN_HR_IF_NULL(E_INVALIDARG, state);
            const ImgViewerSnapshot* image = &state->image;

            const UiDraw draw(context.draw);
            auto* d2d_context = static_cast<ID2D1DeviceContext*>(context.draw.d2d_context);
            wil::com_ptr<ID2D1PathGeometry> icon_geometry;
            RETURN_IF_FAILED(CreatePathGeometryFromIcon(
                context.d2d_factory,
                icons::kImageIcon.commands,
                icons::kImageIcon.command_count,
                icon_geometry.put()));

            // Known issue: Windows HDR composition can present this SDR theme color differently on FP16 surfaces.
            draw.Clear(ui_theme::color::kWindowBackground);
            const float width = context.draw.viewport_size.width;
            const float height = context.draw.viewport_size.height;
            d2d_context->SetTransform(context.root_transform);
            if (state->checkerboard_background) {
                RETURN_IF_FAILED(DrawCheckerboard(
                    d2d_context,
                    context.draw.viewport_size,
                    kCheckerboardCellSize * state->dpi_scale));
            }

            if (image->bitmap != nullptr) {
                const float image_width = static_cast<float>(image->pixel_size.width);
                const float image_height = static_cast<float>(image->pixel_size.height);
                const int edit_rotation = state->edit.active ? state->edit.rotation_quadrants : 0;
                const D2D1_SIZE_U preview_size =
                    imgviewer_edit_geometry::EditPreviewSize(image->pixel_size, edit_rotation);
                const float image_scale =
                    math::FitScale(preview_size, context.viewport_pixel_size) * image->zoom_multiplier;
                const D2D1_POINT_2F viewport_center = D2D1::Point2F(width * 0.5f, height * 0.5f);
                const D2D1_POINT_2F preview_view_center =
                    imgviewer_edit_geometry::SourcePointToEditPreviewPoint(
                        image->view_center,
                        image->pixel_size,
                        edit_rotation);
                const float flip_x = image->flipped_horizontal ? -1.0f : 1.0f;
                const float flip_y = image->flipped_vertical ? -1.0f : 1.0f;
                const D2D1_MATRIX_3X2_F source_to_viewport =
                    imgviewer_edit_geometry::SourceToEditPreviewTransform(image->pixel_size, edit_rotation) *
                    D2D1::Matrix3x2F::Translation(-preview_view_center.x, -preview_view_center.y) *
                    D2D1::Matrix3x2F::Scale(image_scale, image_scale) *
                    D2D1::Matrix3x2F::Scale(flip_x, flip_y, D2D1::Point2F(0.0f, 0.0f)) *
                    D2D1::Matrix3x2F::Rotation(image->rotation_degrees, D2D1::Point2F(0.0f, 0.0f)) *
                    D2D1::Matrix3x2F::Translation(viewport_center.x, viewport_center.y) *
                    context.root_transform;
                d2d_context->SetTransform(source_to_viewport);
                d2d_context->DrawBitmap(
                    image->bitmap,
                    D2D1::RectF(0.0f, 0.0f, image_width, image_height),
                    1.0f,
                    image->pixelated_sampling
                        ? D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR
                        : D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
                return S_OK;
            }

            const float icon_size = (std::max)(
                ui_theme::metrics::kIconPlaceholderMinimumSize * state->dpi_scale,
                (std::min)(
                    ui_theme::metrics::kIconPlaceholderSize * state->dpi_scale,
                    (std::min)(width, height) - ui_theme::metrics::kIconPlaceholderPadding * state->dpi_scale));
            const float icon_width = icons::kImageIcon.view_box.right - icons::kImageIcon.view_box.left;
            const float icon_height = icons::kImageIcon.view_box.bottom - icons::kImageIcon.view_box.top;
            const float icon_viewport = (std::max)(icon_width, icon_height);
            const float scale = icon_size / icon_viewport;
            const float left = (width - icon_size) * 0.5f;
            const float top = (height - icon_size) * 0.5f;
            const D2D1_MATRIX_3X2_F transform =
                D2D1::Matrix3x2F::Translation(-icons::kImageIcon.view_box.left, -icons::kImageIcon.view_box.top) *
                D2D1::Matrix3x2F::Scale(scale, scale) *
                D2D1::Matrix3x2F::Translation(left, top) *
                context.root_transform;
            d2d_context->SetTransform(transform);
            draw.DrawGeometry(
                icon_geometry.get(),
                ui_theme::color::kAccent,
                ui_theme::metrics::kPathIconStrokeWidth / scale);
            return S_OK;
        },
        &state);
}

HRESULT ImgViewerRenderer::RenderEditLayer(const ImgViewerSnapshot& image, const ImgViewerEditSnapshot& edit)
{
    EditLayerRenderState state{
        .image = image,
        .edit = edit,
    };
    return ui_renderer_.DrawSurface(
        edit_surface_,
        [](const UiSurfaceDrawContext& context, void* user_data) -> HRESULT {
            const auto* state = static_cast<const EditLayerRenderState*>(user_data);
            RETURN_HR_IF_NULL(E_INVALIDARG, state);

            const UiDraw draw(context.draw);
            draw.Clear(D2D1::ColorF(D2D1::ColorF::Black, 0.0f));
            if (!state->edit.active || state->image.bitmap == nullptr) {
                return S_OK;
            }

            auto* d2d_context = static_cast<ID2D1DeviceContext*>(context.draw.d2d_context);
            const float width = context.draw.viewport_size.width;
            const float height = context.draw.viewport_size.height;
            const float image_width = static_cast<float>(state->image.pixel_size.width);
            const float image_height = static_cast<float>(state->image.pixel_size.height);
            const D2D1_SIZE_U preview_size =
                imgviewer_edit_geometry::EditPreviewSize(state->image.pixel_size, state->edit.rotation_quadrants);
            const float image_scale =
                math::FitScale(preview_size, context.viewport_pixel_size) * state->image.zoom_multiplier;
            const D2D1_POINT_2F viewport_center = D2D1::Point2F(width * 0.5f, height * 0.5f);
            const D2D1_POINT_2F preview_view_center =
                imgviewer_edit_geometry::SourcePointToEditPreviewPoint(
                    state->image.view_center,
                    state->image.pixel_size,
                    state->edit.rotation_quadrants);
            const D2D1_RECT_F image_rect = D2D1::RectF(0.0f, 0.0f, image_width, image_height);
            const D2D1_MATRIX_3X2_F document_transform =
                imgviewer_edit_geometry::SourceToEditPreviewTransform(
                    state->image.pixel_size,
                    state->edit.rotation_quadrants) *
                D2D1::Matrix3x2F::Translation(-preview_view_center.x, -preview_view_center.y) *
                D2D1::Matrix3x2F::Scale(image_scale, image_scale) *
                D2D1::Matrix3x2F::Scale(
                    state->image.flipped_horizontal ? -1.0f : 1.0f,
                    state->image.flipped_vertical ? -1.0f : 1.0f,
                    D2D1::Point2F(0.0f, 0.0f)) *
                D2D1::Matrix3x2F::Rotation(state->image.rotation_degrees, D2D1::Point2F(0.0f, 0.0f)) *
                D2D1::Matrix3x2F::Translation(viewport_center.x, viewport_center.y) *
                context.root_transform;
            d2d_context->SetTransform(document_transform);

            wil::com_ptr<ID2D1SolidColorBrush> brush;
            for (const ImgViewerEditStroke& stroke : state->edit.strokes) {
                RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(stroke.color, brush.put()));
                for (size_t index = 1; index < stroke.points.size(); ++index) {
                    d2d_context->DrawLine(stroke.points[index - 1], stroke.points[index], brush.get(), stroke.width);
                }
                brush.reset();
            }
            if (state->edit.drawing_stroke) {
                const ImgViewerEditStroke& stroke = state->edit.current_stroke;
                RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(stroke.color, brush.put()));
                for (size_t index = 1; index < stroke.points.size(); ++index) {
                    d2d_context->DrawLine(stroke.points[index - 1], stroke.points[index], brush.get(), stroke.width);
                }
                brush.reset();
            }

            for (const ImgViewerEditShape& shape : state->edit.shapes) {
                RETURN_IF_FAILED(DrawEditShapeObject(d2d_context, shape));
            }
            if (state->edit.drawing_shape) {
                RETURN_IF_FAILED(DrawEditShapeObject(d2d_context, state->edit.current_shape));
            }

            if (state->edit.tool == ImgViewerEditTool::Crop ||
                state->edit.drawing_crop ||
                state->edit.has_pending_crop ||
                state->edit.has_crop) {
                RETURN_IF_FAILED(DrawCropOverlay(
                    d2d_context,
                    context.draw.body_text_format,
                    state->edit,
                    image_rect,
                    image_scale));
            }

            for (const ImgViewerEditMosaic& mosaic : state->edit.mosaics) {
                const float block = static_cast<float>((std::max)(1U, mosaic.block_size));
                for (float y = mosaic.rect.top; y < mosaic.rect.bottom; y += block) {
                    for (float x = mosaic.rect.left; x < mosaic.rect.right; x += block) {
                        const D2D1_RECT_F block_rect = D2D1::RectF(
                            x,
                            y,
                            (std::min)(x + block, mosaic.rect.right),
                            (std::min)(y + block, mosaic.rect.bottom));
                        const float sample_x = std::clamp(
                            (block_rect.left + block_rect.right) * 0.5f,
                            0.0f,
                            image_width - 1.0f);
                        const float sample_y = std::clamp(
                            (block_rect.top + block_rect.bottom) * 0.5f,
                            0.0f,
                            image_height - 1.0f);
                        const D2D1_RECT_F sample_rect = D2D1::RectF(sample_x, sample_y, sample_x + 1.0f, sample_y + 1.0f);
                        d2d_context->DrawBitmap(
                            state->image.bitmap,
                            block_rect,
                            1.0f,
                            D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR,
                            sample_rect);
                    }
                }
            }

            const D2D1_RECT_F pixel_selection_rect = state->edit.drawing_pixel_selection
                ? state->edit.current_pixel_selection_rect
                : state->edit.pixel_selection_rect;
            if ((state->edit.drawing_pixel_selection || state->edit.has_pixel_selection) &&
                pixel_selection_rect.right > pixel_selection_rect.left &&
                pixel_selection_rect.bottom > pixel_selection_rect.top) {
                RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 0.16f), brush.put()));
                d2d_context->FillRectangle(pixel_selection_rect, brush.get());
                brush.reset();

                D2D1_STROKE_STYLE_PROPERTIES stroke_properties = {};
                stroke_properties.dashStyle = D2D1_DASH_STYLE_DASH;
                stroke_properties.lineJoin = D2D1_LINE_JOIN_MITER;
                wil::com_ptr<ID2D1StrokeStyle> dashed_stroke;
                RETURN_IF_FAILED(context.d2d_factory->CreateStrokeStyle(&stroke_properties, nullptr, 0, dashed_stroke.put()));
                RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 0.95f), brush.put()));
                d2d_context->DrawRectangle(
                    pixel_selection_rect,
                    brush.get(),
                    1.5f / (std::max)(0.01f, image_scale),
                    dashed_stroke.get());
                brush.reset();
            }

            for (size_t text_index = 0; text_index < state->edit.texts.size(); ++text_index) {
                const ImgViewerEditText& text = state->edit.texts[text_index];
                const bool editing = state->edit.editing_text && state->edit.editing_text_index == text_index;
                RETURN_IF_FAILED(DrawEditTextObject(
                    d2d_context,
                    context.draw.dwrite_factory,
                    text,
                    editing,
                    editing ? &state->edit.editing_text_state : nullptr,
                    image_scale));
            }

            D2D1_RECT_F selected_rect = {};
            if (SelectedObjectRect(context.draw.dwrite_factory, state->edit, &selected_rect)) {
                RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(ui_theme::color::kAccent, brush.put()));
                d2d_context->DrawRectangle(
                    selected_rect,
                    brush.get(),
                    1.5f / (std::max)(0.01f, image_scale));
                brush.reset();
            }

            RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 0.55f), brush.put()));
            d2d_context->DrawRectangle(image_rect, brush.get(), 1.0f / (std::max)(0.01f, image_scale));
            return S_OK;
        },
        &state);
}
