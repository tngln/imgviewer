#include "ui.renderer.hpp"

#include <algorithm>

#include <d2d1helper.h>
#include <wil/result_macros.h>

#include "math.hpp"
#include "ui.draw.hpp"

HRESULT UiRenderer::Initialize(HWND hwnd, GraphicsDevice* graphics)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, hwnd);
    RETURN_HR_IF_NULL(E_INVALIDARG, graphics);
    hwnd_ = hwnd;
    graphics_ = graphics;
    d2d_factory_ = graphics_->D2DFactory();
    d2d_context_ = graphics_->D2DContext();
    dwrite_factory_ = graphics_->DWriteFactory();
    dcomp_device_ = graphics_->DCompDevice();

    RETURN_IF_FAILED(dwrite_factory_->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        14.0f,
        L"",
        body_text_format_.put()));
    RETURN_IF_FAILED(dwrite_factory_->CreateTextFormat(
        L"Segoe MDL2 Assets",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        12.0f,
        L"",
        icon_text_format_.put()));
    RETURN_IF_FAILED(body_text_format_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
    RETURN_IF_FAILED(icon_text_format_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));

    RETURN_IF_FAILED(graphics_->CreateCompositionTarget(hwnd_, dcomp_target_.put(), root_visual_.put()));
    RETURN_IF_FAILED(surfaces_.Initialize(dcomp_device_.get(), root_visual_.get()));
    RETURN_IF_FAILED(ResizeSurfacesToClient());
    return Commit();
}

HRESULT UiRenderer::Resize()
{
    if (!dcomp_device_ || !root_visual_) {
        return S_OK;
    }
    return ResizeSurfacesToClient();
}

HRESULT UiRenderer::RegisterSurface(const UiSurfaceDescriptor& descriptor, UiSurfaceId* id)
{
    return surfaces_.RegisterSurface(descriptor, id);
}

HRESULT UiRenderer::DrawSurface(UiSurfaceId id, UiSurfaceDrawCallback callback, void* user_data)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, callback);

    wil::com_ptr<ID2D1Bitmap1> target_bitmap;
    POINT offset = {};
    RETURN_IF_FAILED(BeginDrawSurface(id, target_bitmap.put(), &offset));

    const D2D1_POINT_2F offset_render = D2D1::Point2F(static_cast<float>(offset.x), static_cast<float>(offset.y));
    const UiSurfaceDrawContext draw_context{
        .draw = UiDrawContext{
            .d2d_context = d2d_context_.get(),
            .d2d_factory = d2d_factory_.get(),
            .dwrite_factory = dwrite_factory_.get(),
            .body_text_format = body_text_format_.get(),
            .icon_text_format = icon_text_format_.get(),
            .viewport_size = D2D1::SizeF(
                static_cast<float>(surfaces_.Width()),
                static_cast<float>(surfaces_.Height())),
            .dpi_scale = 1.0f,
        },
        .d2d_factory = d2d_factory_.get(),
        .viewport_pixel_size = ViewportPixelSize(),
        .offset = offset_render,
        .root_transform = D2D1::Matrix3x2F::Translation(offset_render.x, offset_render.y),
    };

    d2d_context_->SetTarget(target_bitmap.get());
    d2d_context_->BeginDraw();
    const HRESULT callback_result = callback(draw_context, user_data);
    const HRESULT end_draw_result = d2d_context_->EndDraw();
    d2d_context_->SetTarget(nullptr);
    const HRESULT end_surface_result = surfaces_.Surface(id)->EndDraw();

    RETURN_IF_FAILED(callback_result);
    RETURN_IF_FAILED(end_draw_result);
    RETURN_IF_FAILED(end_surface_result);
    return S_OK;
}

HRESULT UiRenderer::RenderUiOverlay(UiSurfaceId id, UiController& ui)
{
    const float dpi_scale = math::CoordinateSpace::FromWindow(hwnd_).scale();
    struct UiOverlayState {
        UiController* ui;
        float dpi_scale;
    } state{&ui, dpi_scale};

    return DrawSurface(
        id,
        [](const UiSurfaceDrawContext& context, void* user_data) -> HRESULT {
            const auto* state = static_cast<const UiOverlayState*>(user_data);
            UiController* ui_controller = state->ui;
            const float dpi_scale = state->dpi_scale;
            RETURN_HR_IF_NULL(E_INVALIDARG, ui_controller);

            const auto& root = reinterpret_cast<const D2D1::Matrix3x2F&>(context.root_transform);
            D2D1::Matrix3x2F ui_transform = D2D1::Matrix3x2F::Scale(dpi_scale, dpi_scale) * root;
            UiDrawContext ui_draw = context.draw;
            ui_draw.dpi_scale = dpi_scale;
            ui_draw.viewport_size = D2D1::SizeF(
                context.draw.viewport_size.width / dpi_scale,
                context.draw.viewport_size.height / dpi_scale);

            // One scratch brush recoloured per primitive for the whole overlay.
            wil::com_ptr<ID2D1SolidColorBrush> shared_brush;
            if (SUCCEEDED(context.draw.d2d_context->CreateSolidColorBrush(
                    D2D1::ColorF(D2D1::ColorF::Black), shared_brush.put()))) {
                ui_draw.brush = shared_brush.get();
            }

            const UiDraw draw(ui_draw);
            draw.Clear(D2D1::ColorF(D2D1::ColorF::Black, 0.0f));
            context.draw.d2d_context->SetTransform(&ui_transform);
            ui_controller->Render(ui_draw);
            return S_OK;
        },
        &state);
}

HRESULT UiRenderer::SetSurfaceVisible(UiSurfaceId id, bool visible)
{
    RETURN_IF_FAILED(surfaces_.SetVisible(id, visible));
    return Commit();
}

HRESULT UiRenderer::SetSurfaceFormat(UiSurfaceId id, DXGI_FORMAT format)
{
    return surfaces_.SetFormat(id, format);
}

HRESULT UiRenderer::Commit()
{
    RETURN_HR_IF_NULL(E_UNEXPECTED, dcomp_device_);
    return dcomp_device_->Commit();
}

D2D1_SIZE_U UiRenderer::ViewportPixelSize() const
{
    return D2D1::SizeU(surfaces_.Width(), surfaces_.Height());
}

IDWriteTextFormat* UiRenderer::BodyTextFormat() const
{
    return body_text_format_.get();
}

IDWriteTextFormat* UiRenderer::IconTextFormat() const
{
    return icon_text_format_.get();
}

float UiRenderer::DpiScale() const
{
    return math::CoordinateSpace::FromWindow(hwnd_).scale();
}

HRESULT UiRenderer::ResizeSurfacesToClient()
{
    RECT client_rect = {};
    RETURN_IF_WIN32_BOOL_FALSE(GetClientRect(hwnd_, &client_rect));

    const UINT width = static_cast<UINT>(std::max<LONG>(1, client_rect.right - client_rect.left));
    const UINT height = static_cast<UINT>(std::max<LONG>(1, client_rect.bottom - client_rect.top));
    return surfaces_.ResizeAutoSurfaces(width, height);
}

HRESULT UiRenderer::BeginDrawSurface(UiSurfaceId id, ID2D1Bitmap1** target, POINT* offset)
{
    RETURN_HR_IF_NULL(E_POINTER, target);
    RETURN_HR_IF_NULL(E_POINTER, offset);

    IDCompositionSurface* surface = surfaces_.Surface(id);
    RETURN_HR_IF_NULL(E_UNEXPECTED, surface);

    const UiSurfaceSize surface_size = surfaces_.Size(id);
    wil::com_ptr<IDXGISurface> dxgi_surface;
    const RECT update_rect = {
        0,
        0,
        static_cast<LONG>(surface_size.width),
        static_cast<LONG>(surface_size.height),
    };
    RETURN_IF_FAILED(surface->BeginDraw(
        &update_rect,
        __uuidof(IDXGISurface),
        reinterpret_cast<void**>(dxgi_surface.put()),
        offset));

    RETURN_HR_IF_NULL(E_UNEXPECTED, graphics_);
    return graphics_->CreateTargetBitmapFromDxgiSurface(
        dxgi_surface.get(),
        surfaces_.Format(id),
        surfaces_.AlphaMode(id),
        target);
}
