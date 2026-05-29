#include "renderer.hpp"

#include <algorithm>

#include <d2d1helper.h>
#include <wil/result_macros.h>

#include "icons.inc"

namespace {

constexpr float kTestIconSize = 96.0f;
constexpr float kTestIconPadding = 24.0f;

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

} // namespace

HRESULT Renderer::Initialize(HWND hwnd)
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

    RETURN_IF_FAILED(DCompositionCreateDevice(
        dxgi_device_.get(),
        __uuidof(IDCompositionDevice),
        reinterpret_cast<void**>(dcomp_device_.put())));

    RETURN_IF_FAILED(dcomp_device_->CreateTargetForHwnd(hwnd_, TRUE, dcomp_target_.put()));
    RETURN_IF_FAILED(dcomp_device_->CreateVisual(root_visual_.put()));
    RETURN_IF_FAILED(dcomp_target_->SetRoot(root_visual_.get()));

    RETURN_IF_FAILED(CreateCompositionSurface());
    RETURN_IF_FAILED(dcomp_device_->Commit());

    return S_OK;
}

HRESULT Renderer::Resize()
{
    if (!dcomp_device_ || !root_visual_) {
        return S_OK;
    }

    RETURN_IF_FAILED(CreateCompositionSurface());
    RETURN_IF_FAILED(dcomp_device_->Commit());

    return S_OK;
}

HRESULT Renderer::CreateCompositionSurface()
{
    RECT client_rect = {};
    RETURN_IF_WIN32_BOOL_FALSE(GetClientRect(hwnd_, &client_rect));

    const UINT width = static_cast<UINT>(std::max<LONG>(1, client_rect.right - client_rect.left));
    const UINT height = static_cast<UINT>(std::max<LONG>(1, client_rect.bottom - client_rect.top));

    surface_.reset();
    RETURN_IF_FAILED(dcomp_device_->CreateSurface(
        width,
        height,
        DXGI_FORMAT_B8G8R8A8_UNORM,
        DXGI_ALPHA_MODE_IGNORE,
        surface_.put()));

    RETURN_IF_FAILED(root_visual_->SetContent(surface_.get()));
    RETURN_IF_FAILED(RenderTestContent(width, height));

    return S_OK;
}

HRESULT Renderer::RenderTestContent(UINT surface_width, UINT surface_height)
{
    wil::com_ptr<IDXGISurface> dxgi_surface;
    POINT offset = {};
    RETURN_IF_FAILED(surface_->BeginDraw(
        nullptr,
        __uuidof(IDXGISurface),
        reinterpret_cast<void**>(dxgi_surface.put()),
        &offset));

    wil::com_ptr<ID2D1Bitmap1> target_bitmap;
    const D2D1_BITMAP_PROPERTIES1 bitmap_properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE),
        96.0f,
        96.0f);
    RETURN_IF_FAILED(d2d_context_->CreateBitmapFromDxgiSurface(
        dxgi_surface.get(),
        bitmap_properties,
        target_bitmap.put()));

    wil::com_ptr<ID2D1PathGeometry> icon_geometry;
    RETURN_IF_FAILED(CreatePathGeometryFromIcon(
        d2d_factory_.get(),
        icons::kImageIconPath,
        ARRAYSIZE(icons::kImageIconPath),
        icon_geometry.put()));

    wil::com_ptr<ID2D1SolidColorBrush> icon_brush;
    RETURN_IF_FAILED(d2d_context_->CreateSolidColorBrush(
        D2D1::ColorF(0x2f6fed),
        icon_brush.put()));

    d2d_context_->SetTarget(target_bitmap.get());
    d2d_context_->BeginDraw();
    d2d_context_->Clear(D2D1::ColorF(0xf7f9fc));

    const float width = static_cast<float>(surface_width);
    const float height = static_cast<float>(surface_height);
    const float icon_size = (std::max)(16.0f, (std::min)(kTestIconSize, (std::min)(width, height) - kTestIconPadding));
    const float scale = icon_size / icons::kImageIconViewport;
    const float left = (width - icon_size) * 0.5f - static_cast<float>(offset.x);
    const float top = (height - icon_size) * 0.5f - static_cast<float>(offset.y);

    const D2D1_MATRIX_3X2_F transform =
        D2D1::Matrix3x2F::Scale(scale, scale) *
        D2D1::Matrix3x2F::Translation(left, top);
    d2d_context_->SetTransform(transform);
    d2d_context_->DrawGeometry(icon_geometry.get(), icon_brush.get(), 1.75f / scale);
    d2d_context_->SetTransform(D2D1::Matrix3x2F::Identity());

    RETURN_IF_FAILED(d2d_context_->EndDraw());
    d2d_context_->SetTarget(nullptr);

    RETURN_IF_FAILED(surface_->EndDraw());

    return S_OK;
}
