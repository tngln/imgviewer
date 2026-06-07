#include "imgviewer.renderer.hpp"

#include <algorithm>
#include <string>

#include <d2d1helper.h>
#include <wil/com.h>
#include <wil/result_macros.h>

#include "math.hpp"
#include "ui.draw.hpp"
#include "ui.theme.hpp"

namespace {

constexpr float kCheckerboardCellSize = 8.0f;

struct ImageLayerRenderState final {
    ImgViewerSnapshot image;
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

} // namespace

HRESULT ImgViewerRenderer::Initialize(HWND hwnd)
{
    RETURN_IF_FAILED(ui_renderer_.Initialize(hwnd));
    RETURN_IF_FAILED(ui_renderer_.RegisterSurface(
        UiSurfaceDescriptor{
            .name = L"image",
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
    RETURN_IF_FAILED(RenderImageLayer(image));
    RETURN_IF_FAILED(RenderEditLayer(image, edit.Snapshot()));
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

HRESULT ImgViewerRenderer::RenderImageLayer(const ImgViewerSnapshot& image)
{
    ImageLayerRenderState state{
        .image = image,
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

            draw.Clear(ui_theme::color::kWindowBackground);
            const float width = context.draw.viewport_size.width;
            const float height = context.draw.viewport_size.height;
            d2d_context->SetTransform(context.root_transform);
            if (state->checkerboard_background) {
                RETURN_IF_FAILED(DrawCheckerboard(
                    d2d_context, context.draw.viewport_size, kCheckerboardCellSize * state->dpi_scale));
            }

            if (image->bitmap != nullptr) {
                const float image_width = static_cast<float>(image->pixel_size.width);
                const float image_height = static_cast<float>(image->pixel_size.height);
                const float image_scale =
                    math::FitScale(image->pixel_size, context.viewport_pixel_size) * image->zoom_multiplier;
                const float draw_width = image_width * image_scale;
                const float draw_height = image_height * image_scale;
                const D2D1_POINT_2F viewport_center = D2D1::Point2F(width * 0.5f, height * 0.5f);
                const D2D1_RECT_F destination = D2D1::RectF(
                    viewport_center.x - image->view_center.x * image_scale,
                    viewport_center.y - image->view_center.y * image_scale,
                    viewport_center.x - image->view_center.x * image_scale + draw_width,
                    viewport_center.y - image->view_center.y * image_scale + draw_height);
                const float flip_x = image->flipped_horizontal ? -1.0f : 1.0f;
                const float flip_y = image->flipped_vertical ? -1.0f : 1.0f;
                d2d_context->SetTransform(
                    D2D1::Matrix3x2F::Scale(flip_x, flip_y, viewport_center) *
                        D2D1::Matrix3x2F::Rotation(image->rotation_degrees, viewport_center) *
                        context.root_transform);
                d2d_context->DrawBitmap(
                    image->bitmap,
                    destination,
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
            const float image_scale =
                math::FitScale(state->image.pixel_size, context.viewport_pixel_size) * state->image.zoom_multiplier;
            const D2D1_POINT_2F viewport_center = D2D1::Point2F(width * 0.5f, height * 0.5f);
            const D2D1_RECT_F image_rect = D2D1::RectF(0.0f, 0.0f, image_width, image_height);
            const D2D1_MATRIX_3X2_F document_transform =
                D2D1::Matrix3x2F::Translation(-state->image.view_center.x, -state->image.view_center.y) *
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

            const D2D1_RECT_F crop_rect = state->edit.drawing_crop ? state->edit.current_crop_rect : state->edit.crop_rect;
            if (crop_rect.right > crop_rect.left && crop_rect.bottom > crop_rect.top) {
                RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Cyan, 0.88f), brush.put()));
                d2d_context->DrawRectangle(crop_rect, brush.get(), 2.0f / (std::max)(0.01f, image_scale));
                brush.reset();
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
                const float marker_width = (std::max)(48.0f, static_cast<float>(text.text.size()) * 8.0f + 12.0f);
                const D2D1_RECT_F marker = D2D1::RectF(
                    text.origin.x,
                    text.origin.y,
                    text.origin.x + marker_width,
                    text.origin.y + 26.0f);
                RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Yellow, editing ? 0.82f : 0.55f), brush.put()));
                d2d_context->FillRectangle(marker, brush.get());
                brush.reset();

                RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(editing ? ui_theme::color::kAccent : D2D1::ColorF(D2D1::ColorF::White, 0.7f), brush.put()));
                d2d_context->DrawRectangle(marker, brush.get(), 1.0f / (std::max)(0.01f, image_scale));
                brush.reset();

                RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black, 0.95f), brush.put()));
                const std::wstring text_to_draw = text.text.empty() && editing ? L" " : text.text;
                d2d_context->DrawTextW(
                    text_to_draw.c_str(),
                    static_cast<UINT32>(text_to_draw.size()),
                    context.draw.body_text_format,
                    D2D1::RectF(marker.left + 6.0f, marker.top + 2.0f, marker.right - 6.0f, marker.bottom),
                    brush.get(),
                    D2D1_DRAW_TEXT_OPTIONS_CLIP);
                brush.reset();

                if (editing) {
                    const float caret_x = marker.left + 6.0f + (std::max)(0.0f, static_cast<float>(text.text.size()) * 8.0f);
                    RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), brush.put()));
                    d2d_context->DrawLine(
                        D2D1::Point2F(caret_x, marker.top + 4.0f),
                        D2D1::Point2F(caret_x, marker.bottom - 4.0f),
                        brush.get(),
                        1.0f / (std::max)(0.01f, image_scale));
                    brush.reset();
                }
            }

            RETURN_IF_FAILED(d2d_context->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White, 0.55f), brush.put()));
            d2d_context->DrawRectangle(image_rect, brush.get(), 1.0f / (std::max)(0.01f, image_scale));
            return S_OK;
        },
        &state);
}
