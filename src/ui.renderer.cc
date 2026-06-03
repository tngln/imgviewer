#include "ui.renderer.hpp"

#include <algorithm>

#include <d2d1helper.h>
#include <wil/result_macros.h>

#include "math.hpp"
#include "ui.draw.hpp"
#include "ui.theme.hpp"

HRESULT UiRenderer::Initialize(HWND hwnd)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, hwnd);
    hwnd_ = hwnd;

    D2D1_FACTORY_OPTIONS factory_options = {};
#if defined(_DEBUG)
    factory_options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

    RETURN_IF_FAILED(D2D1CreateFactory(
        D2D1_FACTORY_TYPE_SINGLE_THREADED,
        factory_options,
        d2d_factory_.put()));

    constexpr UINT device_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    constexpr D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    RETURN_IF_FAILED(D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        device_flags,
        feature_levels,
        ARRAYSIZE(feature_levels),
        D3D11_SDK_VERSION,
        d3d_device_.put(),
        nullptr,
        d3d_context_.put()));

    RETURN_IF_FAILED(d3d_device_->QueryInterface(IID_PPV_ARGS(dxgi_device_.put())));
    RETURN_IF_FAILED(d2d_factory_->CreateDevice(dxgi_device_.get(), d2d_device_.put()));
    RETURN_IF_FAILED(d2d_device_->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, d2d_context_.put()));
    RETURN_IF_FAILED(DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(dwrite_factory_.put())));
    RETURN_IF_FAILED(dwrite_factory_->CreateTextFormat(
        L"Segoe UI",
        nullptr,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        28.0f,
        L"",
        body_text_format_.put()));
    RETURN_IF_FAILED(dwrite_factory_->CreateTextFormat(
        L"Segoe MDL2 Assets",
        nullptr,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        24.0f,
        L"",
        icon_text_format_.put()));
    RETURN_IF_FAILED(body_text_format_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
    RETURN_IF_FAILED(icon_text_format_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));

    RETURN_IF_FAILED(DCompositionCreateDevice(
        dxgi_device_.get(),
        __uuidof(IDCompositionDevice),
        reinterpret_cast<void**>(dcomp_device_.put())));

    RETURN_IF_FAILED(dcomp_device_->CreateTargetForHwnd(hwnd_, TRUE, dcomp_target_.put()));
    RETURN_IF_FAILED(dcomp_device_->CreateVisual(root_visual_.put()));
    RETURN_IF_FAILED(dcomp_target_->SetRoot(root_visual_.get()));
    RETURN_IF_FAILED(surfaces_.Initialize(dcomp_device_.get(), root_visual_.get()));
    RETURN_IF_FAILED(surfaces_.RegisterSurface(
        UiSurfaceDescriptor{
            .name = L"image",
            .alpha_mode = DXGI_ALPHA_MODE_IGNORE,
            .z_order = 0,
            .auto_resize = true,
            .initially_visible = true,
        },
        &image_surface_));
    RETURN_IF_FAILED(surfaces_.RegisterSurface(
        UiSurfaceDescriptor{
            .name = L"ui-overlay",
            .alpha_mode = DXGI_ALPHA_MODE_PREMULTIPLIED,
            .z_order = 100,
            .auto_resize = true,
            .initially_visible = true,
        },
        &ui_overlay_surface_));

    RETURN_IF_FAILED(ResizeSurfacesToClient());
    RETURN_IF_FAILED(dcomp_device_->Commit());

    return S_OK;
}

HRESULT UiRenderer::Resize()
{
    if (!dcomp_device_ || !root_visual_) {
        return S_OK;
    }

    RETURN_IF_FAILED(ResizeSurfacesToClient());

    return S_OK;
}

HRESULT UiRenderer::ResizeSurfacesToClient()
{
    RECT client_rect = {};
    RETURN_IF_WIN32_BOOL_FALSE(GetClientRect(hwnd_, &client_rect));

    const UINT width = static_cast<UINT>(std::max<LONG>(1, client_rect.right - client_rect.left));
    const UINT height = static_cast<UINT>(std::max<LONG>(1, client_rect.bottom - client_rect.top));

    RETURN_IF_FAILED(surfaces_.ResizeAutoSurfaces(width, height));

    return S_OK;
}

HRESULT UiRenderer::Render(const ImgViewerController& viewer, UiController& ui)
{
    if (!surfaces_.Surface(image_surface_) || !surfaces_.Surface(ui_overlay_surface_)) {
        return S_OK;
    }

    RETURN_IF_FAILED(RenderImageLayer(viewer.Snapshot()));
    RETURN_IF_FAILED(RenderUiOverlayLayer(ui));
    RETURN_IF_FAILED(dcomp_device_->Commit());

    return S_OK;
}

D2D1_SIZE_U UiRenderer::ViewportPixelSize() const
{
    return D2D1::SizeU(surfaces_.Width(), surfaces_.Height());
}

ID2D1DeviceContext* UiRenderer::BitmapDeviceContext() const
{
    return d2d_context_.get();
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

    wil::com_ptr<ID2D1Bitmap1> target_bitmap;
    const D2D1_BITMAP_PROPERTIES1 bitmap_properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, static_cast<D2D1_ALPHA_MODE>(surfaces_.AlphaMode(id))),
        96.0f,
        96.0f);
    RETURN_IF_FAILED(d2d_context_->CreateBitmapFromDxgiSurface(
        dxgi_surface.get(),
        bitmap_properties,
        target_bitmap.put()));

    *target = target_bitmap.detach();
    return S_OK;
}

HRESULT UiRenderer::RenderImageLayer(const ImgViewerSnapshot& image)
{
    const UiDrawContext draw_context{
        .d2d_context = d2d_context_.get(),
        .dwrite_factory = dwrite_factory_.get(),
    };
    const UiDraw draw(draw_context);
    wil::com_ptr<ID2D1Bitmap1> target_bitmap;
    POINT offset = {};
    RETURN_IF_FAILED(BeginDrawSurface(image_surface_, target_bitmap.put(), &offset));

    wil::com_ptr<ID2D1PathGeometry> icon_geometry;
    RETURN_IF_FAILED(CreatePathGeometryFromIcon(
        d2d_factory_.get(),
        icons::kImageIconPath,
        ARRAYSIZE(icons::kImageIconPath),
        icon_geometry.put()));

    d2d_context_->SetTarget(target_bitmap.get());
    d2d_context_->BeginDraw();
    draw.Clear(ui_theme::color::kWindowBackground);

    const math::CoordinateSpace coordinates = math::CoordinateSpace::FromWindow(hwnd_);
    const D2D1_SIZE_F size = coordinates.PhysicalToRender(surfaces_.Width(), surfaces_.Height());
    const float width = size.width;
    const float height = size.height;
    const D2D1_POINT_2F offset_render = coordinates.PhysicalToRender(offset);
    const D2D1_MATRIX_3X2_F root_transform = D2D1::Matrix3x2F::Translation(offset_render.x, offset_render.y);
    d2d_context_->SetTransform(root_transform);

    if (image.bitmap != nullptr) {
        const float image_width = static_cast<float>(image.pixel_size.width);
        const float image_height = static_cast<float>(image.pixel_size.height);
        const float image_scale =
            math::FitScale(image.pixel_size, D2D1::SizeU(surfaces_.Width(), surfaces_.Height())) *
            image.zoom_multiplier;
        const float draw_width = image_width * image_scale;
        const float draw_height = image_height * image_scale;
        const D2D1_POINT_2F viewport_center = D2D1::Point2F(width * 0.5f, height * 0.5f);
        const D2D1_RECT_F destination = D2D1::RectF(
            viewport_center.x - image.view_center.x * image_scale,
            viewport_center.y - image.view_center.y * image_scale,
            viewport_center.x - image.view_center.x * image_scale + draw_width,
            viewport_center.y - image.view_center.y * image_scale + draw_height);
        const float flip_x = image.flipped_horizontal ? -1.0f : 1.0f;
        const float flip_y = image.flipped_vertical ? -1.0f : 1.0f;
        d2d_context_->SetTransform(
            D2D1::Matrix3x2F::Scale(flip_x, flip_y, viewport_center) *
                D2D1::Matrix3x2F::Rotation(image.rotation_degrees, viewport_center) *
                root_transform);
        d2d_context_->DrawBitmap(
            image.bitmap,
            destination,
            1.0f,
            D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
    } else {
        const float icon_size = (std::max)(
            ui_theme::metrics::kIconPlaceholderMinimumSize,
            (std::min)(
                ui_theme::metrics::kIconPlaceholderSize,
                (std::min)(width, height) - ui_theme::metrics::kIconPlaceholderPadding));
        const float scale = icon_size / icons::kImageIconViewport;
        const float left = (width - icon_size) * 0.5f;
        const float top = (height - icon_size) * 0.5f;

        const D2D1_MATRIX_3X2_F transform =
            D2D1::Matrix3x2F::Scale(scale, scale) *
            D2D1::Matrix3x2F::Translation(left, top) *
            root_transform;
        d2d_context_->SetTransform(transform);
        draw.DrawGeometry(
            icon_geometry.get(),
            ui_theme::color::kAccent,
            ui_theme::metrics::kPathIconStrokeWidth / scale);
    }

    RETURN_IF_FAILED(d2d_context_->EndDraw());
    d2d_context_->SetTarget(nullptr);

    RETURN_IF_FAILED(surfaces_.Surface(image_surface_)->EndDraw());

    return S_OK;
}

HRESULT UiRenderer::RenderUiOverlayLayer(UiController& ui)
{
    const UiDrawContext draw_context{
        .d2d_context = d2d_context_.get(),
        .dwrite_factory = dwrite_factory_.get(),
        .body_text_format = body_text_format_.get(),
        .icon_text_format = icon_text_format_.get(),
    };
    const UiDraw draw(draw_context);
    wil::com_ptr<ID2D1Bitmap1> target_bitmap;
    POINT offset = {};
    RETURN_IF_FAILED(BeginDrawSurface(ui_overlay_surface_, target_bitmap.put(), &offset));

    d2d_context_->SetTarget(target_bitmap.get());
    d2d_context_->BeginDraw();
    draw.Clear(D2D1::ColorF(D2D1::ColorF::Black, 0.0f));

    const math::CoordinateSpace coordinates = math::CoordinateSpace::FromWindow(hwnd_);
    const D2D1_SIZE_F size = coordinates.PhysicalToRender(surfaces_.Width(), surfaces_.Height());
    const D2D1_POINT_2F offset_render = coordinates.PhysicalToRender(offset);
    const D2D1_MATRIX_3X2_F root_transform = D2D1::Matrix3x2F::Translation(offset_render.x, offset_render.y);
    d2d_context_->SetTransform(root_transform);
    ui.Draw(d2d_context_.get(), size, dwrite_factory_.get(), body_text_format_.get(), icon_text_format_.get());

    RETURN_IF_FAILED(d2d_context_->EndDraw());
    d2d_context_->SetTarget(nullptr);

    RETURN_IF_FAILED(surfaces_.Surface(ui_overlay_surface_)->EndDraw());

    return S_OK;
}
