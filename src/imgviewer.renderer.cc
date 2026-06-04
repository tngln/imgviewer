#include "imgviewer.renderer.hpp"

#include <algorithm>

#include <d2d1helper.h>
#include <wil/com.h>
#include <wil/result_macros.h>

#include "math.hpp"
#include "ui.draw.hpp"
#include "ui.theme.hpp"

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

HRESULT ImgViewerRenderer::Render(const ImgViewerController& viewer, UiController& ui)
{
    RETURN_IF_FAILED(RenderImageLayer(viewer.Snapshot()));
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

D2D1_SIZE_U ImgViewerRenderer::ViewportPixelSize() const
{
    return ui_renderer_.ViewportPixelSize();
}

ID2D1DeviceContext* ImgViewerRenderer::BitmapDeviceContext() const
{
    return ui_renderer_.BitmapDeviceContext();
}

HRESULT ImgViewerRenderer::RenderImageLayer(const ImgViewerSnapshot& image)
{
    return ui_renderer_.DrawSurface(
        image_surface_,
        [](const UiSurfaceDrawContext& context, void* user_data) -> HRESULT {
            const auto* image = static_cast<const ImgViewerSnapshot*>(user_data);
            RETURN_HR_IF_NULL(E_INVALIDARG, image);

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
                ui_theme::metrics::kIconPlaceholderMinimumSize,
                (std::min)(
                    ui_theme::metrics::kIconPlaceholderSize,
                    (std::min)(width, height) - ui_theme::metrics::kIconPlaceholderPadding));
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
        const_cast<ImgViewerSnapshot*>(&image));
}
