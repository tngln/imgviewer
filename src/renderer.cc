#include "renderer.hpp"

#include <algorithm>

#include <d2d1helper.h>
#include <wil/result_macros.h>

#include "app.messages.hpp"
#include "coordinates.hpp"
#include "icons.inc"

namespace {

constexpr float kTestIconSize = 96.0f;
constexpr float kTestIconPadding = 24.0f;
constexpr wchar_t kTitleText[] = L"ImgViewer";
constexpr wchar_t kBodyText[] = L"D2D + DirectWrite text rendering";
constexpr wchar_t kIconText[] = L"\xE921  \xE922  \xE8BB";

D2D1_RECT_F StableRect(float left, float top, float right, float bottom)
{
    return D2D1::RectF(left, top, (std::max)(left + 1.0f, right), (std::max)(top + 1.0f, bottom));
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
    RETURN_IF_FAILED(image_decoder_.Initialize());
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
    RETURN_IF_FAILED(CreateUiAccessibilityProvider(hwnd_, this, accessibility_provider_.put()));

    RETURN_IF_FAILED(ResizeSurfacesToClient());
    RETURN_IF_FAILED(dcomp_device_->Commit());

    return S_OK;
}

HRESULT Renderer::Resize()
{
    if (!dcomp_device_ || !root_visual_) {
        return S_OK;
    }

    RETURN_IF_FAILED(ResizeSurfacesToClient());

    return S_OK;
}

HRESULT Renderer::ResizeSurfacesToClient()
{
    RECT client_rect = {};
    RETURN_IF_WIN32_BOOL_FALSE(GetClientRect(hwnd_, &client_rect));

    const UINT width = static_cast<UINT>(std::max<LONG>(1, client_rect.right - client_rect.left));
    const UINT height = static_cast<UINT>(std::max<LONG>(1, client_rect.bottom - client_rect.top));

    RETURN_IF_FAILED(surfaces_.Resize(width, height));
    RETURN_IF_FAILED(Render());

    return S_OK;
}

HRESULT Renderer::Render()
{
    if (!surfaces_.Surface(SurfaceLayerId::Image) || !surfaces_.Surface(SurfaceLayerId::UiOverlay)) {
        return S_OK;
    }

    RETURN_IF_FAILED(RenderImageLayer());
    RETURN_IF_FAILED(RenderUiOverlayLayer());
    RETURN_IF_FAILED(dcomp_device_->Commit());

    return S_OK;
}

UiEventResult Renderer::OnPointerMove(float x, float y)
{
    return ui_.OnPointerMove(D2D1::Point2F(x, y));
}

UiEventResult Renderer::OnPointerDown(float x, float y)
{
    return ui_.OnPointerDown(D2D1::Point2F(x, y));
}

UiEventResult Renderer::OnPointerUp(float x, float y)
{
    return ui_.OnPointerUp(D2D1::Point2F(x, y));
}

UiEventResult Renderer::OnPointerLeave()
{
    return ui_.OnPointerLeave();
}

IRawElementProviderSimple* Renderer::GetAccessibilityProvider()
{
    return accessibility_provider_.get();
}

void Renderer::InvokeTestButtonFromAccessibility()
{
    ui_.InvokeTestButton();
    Render();
}

void Renderer::InvokeOpenImageFromAccessibility()
{
    PostMessageW(hwnd_, kImgViewerOpenImageMessage, 0, 0);
}

HRESULT Renderer::LoadImageFile(const wchar_t* path)
{
    DecodedImage image;
    RETURN_IF_FAILED(image_decoder_.DecodeFirstFrame(path, d2d_context_.get(), &image));

    current_image_ = std::move(image);
    RETURN_IF_FAILED(Render());
    return S_OK;
}

D2D1_RECT_F Renderer::TestButtonRect() const
{
    return ui_.TestButtonRect();
}

D2D1_RECT_F Renderer::OpenButtonRect() const
{
    return ui_.OpenButtonRect();
}

HRESULT Renderer::BeginDrawLayer(
    SurfaceLayerId id,
    DXGI_ALPHA_MODE alpha_mode,
    ID2D1Bitmap1** target,
    POINT* offset)
{
    RETURN_HR_IF_NULL(E_POINTER, target);
    RETURN_HR_IF_NULL(E_POINTER, offset);

    IDCompositionSurface* surface = surfaces_.Surface(id);
    RETURN_HR_IF_NULL(E_UNEXPECTED, surface);

    wil::com_ptr<IDXGISurface> dxgi_surface;
    const RECT update_rect = {
        0,
        0,
        static_cast<LONG>(surfaces_.Width()),
        static_cast<LONG>(surfaces_.Height()),
    };
    RETURN_IF_FAILED(surface->BeginDraw(
        &update_rect,
        __uuidof(IDXGISurface),
        reinterpret_cast<void**>(dxgi_surface.put()),
        offset));

    wil::com_ptr<ID2D1Bitmap1> target_bitmap;
    const D2D1_BITMAP_PROPERTIES1 bitmap_properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, static_cast<D2D1_ALPHA_MODE>(alpha_mode)),
        96.0f,
        96.0f);
    RETURN_IF_FAILED(d2d_context_->CreateBitmapFromDxgiSurface(
        dxgi_surface.get(),
        bitmap_properties,
        target_bitmap.put()));

    *target = target_bitmap.detach();
    return S_OK;
}

HRESULT Renderer::RenderImageLayer()
{
    wil::com_ptr<ID2D1Bitmap1> target_bitmap;
    POINT offset = {};
    RETURN_IF_FAILED(BeginDrawLayer(
        SurfaceLayerId::Image,
        DXGI_ALPHA_MODE_IGNORE,
        target_bitmap.put(),
        &offset));

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

    const CoordinateSpace coordinates = CoordinateSpace::FromWindow(hwnd_);
    const D2D1_SIZE_F size = coordinates.PhysicalToRender(surfaces_.Width(), surfaces_.Height());
    const float width = size.width;
    const float height = size.height;
    const D2D1_POINT_2F offset_render = coordinates.PhysicalToRender(offset);
    const D2D1_MATRIX_3X2_F root_transform = D2D1::Matrix3x2F::Translation(-offset_render.x, -offset_render.y);
    d2d_context_->SetTransform(root_transform);

    if (current_image_.bitmap) {
        const float image_width = static_cast<float>(current_image_.pixel_size.width);
        const float image_height = static_cast<float>(current_image_.pixel_size.height);
        const float available_width = (std::max)(1.0f, width);
        const float available_height = (std::max)(1.0f, height);
        const float image_scale = (std::min)(available_width / image_width, available_height / image_height);
        const float draw_width = image_width * image_scale;
        const float draw_height = image_height * image_scale;
        const D2D1_RECT_F destination = D2D1::RectF(
            (width - draw_width) * 0.5f,
            (height - draw_height) * 0.5f,
            (width + draw_width) * 0.5f,
            (height + draw_height) * 0.5f);
        d2d_context_->DrawBitmap(
            current_image_.bitmap.get(),
            destination,
            1.0f,
            D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
    } else {
        const float icon_size = (std::max)(16.0f, (std::min)(kTestIconSize, (std::min)(width, height) - kTestIconPadding));
        const float scale = icon_size / icons::kImageIconViewport;
        const float left = (width - icon_size) * 0.5f;
        const float top = (height - icon_size) * 0.5f;

        const D2D1_MATRIX_3X2_F transform =
            D2D1::Matrix3x2F::Scale(scale, scale) *
            D2D1::Matrix3x2F::Translation(left, top) *
            root_transform;
        d2d_context_->SetTransform(transform);
        d2d_context_->DrawGeometry(icon_geometry.get(), icon_brush.get(), 1.75f / scale);
    }

    RETURN_IF_FAILED(d2d_context_->EndDraw());
    d2d_context_->SetTarget(nullptr);

    RETURN_IF_FAILED(surfaces_.Surface(SurfaceLayerId::Image)->EndDraw());

    return S_OK;
}

HRESULT Renderer::RenderUiOverlayLayer()
{
    wil::com_ptr<ID2D1Bitmap1> target_bitmap;
    POINT offset = {};
    RETURN_IF_FAILED(BeginDrawLayer(
        SurfaceLayerId::UiOverlay,
        DXGI_ALPHA_MODE_PREMULTIPLIED,
        target_bitmap.put(),
        &offset));

    wil::com_ptr<ID2D1SolidColorBrush> icon_brush;
    RETURN_IF_FAILED(d2d_context_->CreateSolidColorBrush(
        D2D1::ColorF(0x2f6fed),
        icon_brush.put()));
    wil::com_ptr<ID2D1SolidColorBrush> title_brush;
    RETURN_IF_FAILED(d2d_context_->CreateSolidColorBrush(
        D2D1::ColorF(0x172033),
        title_brush.put()));
    wil::com_ptr<ID2D1SolidColorBrush> muted_brush;
    RETURN_IF_FAILED(d2d_context_->CreateSolidColorBrush(
        D2D1::ColorF(0x697386),
        muted_brush.put()));

    d2d_context_->SetTarget(target_bitmap.get());
    d2d_context_->BeginDraw();
    d2d_context_->Clear(D2D1::ColorF(D2D1::ColorF::Black, 0.0f));

    const CoordinateSpace coordinates = CoordinateSpace::FromWindow(hwnd_);
    const D2D1_SIZE_F size = coordinates.PhysicalToRender(surfaces_.Width(), surfaces_.Height());
    const float width = size.width;
    const D2D1_POINT_2F offset_render = coordinates.PhysicalToRender(offset);
    const D2D1_MATRIX_3X2_F root_transform = D2D1::Matrix3x2F::Translation(-offset_render.x, -offset_render.y);
    d2d_context_->SetTransform(root_transform);
    d2d_context_->DrawTextW(
        kTitleText,
        ARRAYSIZE(kTitleText) - 1,
        body_text_format_.get(),
        StableRect(32.0f, 26.0f, width - 32.0f, 66.0f),
        title_brush.get(),
        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE_NATURAL);
    d2d_context_->DrawTextW(
        kBodyText,
        ARRAYSIZE(kBodyText) - 1,
        body_text_format_.get(),
        StableRect(32.0f, 64.0f, width - 32.0f, 104.0f),
        muted_brush.get(),
        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE_NATURAL);
    d2d_context_->DrawTextW(
        kIconText,
        ARRAYSIZE(kIconText) - 1,
        icon_text_format_.get(),
        StableRect(32.0f, 112.0f, width - 32.0f, 152.0f),
        icon_brush.get(),
        D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE_NATURAL);
    ui_.Draw(d2d_context_.get(), body_text_format_.get(), icon_text_format_.get());

    RETURN_IF_FAILED(d2d_context_->EndDraw());
    d2d_context_->SetTarget(nullptr);

    RETURN_IF_FAILED(surfaces_.Surface(SurfaceLayerId::UiOverlay)->EndDraw());

    return S_OK;
}
