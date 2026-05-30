#include "renderer.hpp"

#include <algorithm>
#include <cmath>

#include <d2d1helper.h>
#include <wil/result_macros.h>

#include "app.messages.hpp"
#include "coordinates.hpp"
#include "ui.draw.hpp"
#include "ui.theme.hpp"

namespace {

constexpr float kMinImageZoomMultiplier = 0.05f;
constexpr float kMaxImageZoomMultiplier = 64.0f;
constexpr float kWheelZoomStep = 1.12f;
constexpr float kRadiansToDegrees = 57.2957795f;
constexpr wchar_t kBodyText[] = L"D2D + DirectWrite text rendering";
constexpr wchar_t kIconText[] = L"\xE921  \xE922  \xE8BB";

D2D1_RECT_F StableRect(float left, float top, float right, float bottom)
{
    return D2D1::RectF(left, top, (std::max)(left + 1.0f, right), (std::max)(top + 1.0f, bottom));
}

float AngleFromCenter(D2D1_POINT_2F point, D2D1_POINT_2F center)
{
    return std::atan2(point.y - center.y, point.x - center.x);
}

D2D1_POINT_2F TransformVector(D2D1_MATRIX_3X2_F matrix, D2D1_POINT_2F vector)
{
    return D2D1::Point2F(
        vector.x * matrix._11 + vector.y * matrix._21,
        vector.x * matrix._12 + vector.y * matrix._22);
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
    if (image_is_rotating_) {
        const D2D1_POINT_2F point = D2D1::Point2F(x, y);
        const D2D1_POINT_2F viewport_center = D2D1::Point2F(
            static_cast<float>(surfaces_.Width()) * 0.5f,
            static_cast<float>(surfaces_.Height()) * 0.5f);
        const float angle = AngleFromCenter(point, viewport_center);
        image_rotation_degrees_ += (angle - image_last_rotation_angle_) * kRadiansToDegrees;
        image_last_rotation_angle_ = angle;
        Render();
        return UiEventResult{
            .handled = true,
            .needs_render = false,
        };
    }

    if (image_is_panning_) {
        const float image_scale = CurrentImageScale(surfaces_.Width(), surfaces_.Height());
        if (image_scale <= 0.0f) {
            return {};
        }

        const D2D1_POINT_2F point = D2D1::Point2F(x, y);
        const D2D1_POINT_2F screen_delta = D2D1::Point2F(
            point.x - image_last_pan_point_.x,
            point.y - image_last_pan_point_.y);
        const D2D1_POINT_2F image_delta =
            TransformVector(D2D1::Matrix3x2F::Rotation(-image_rotation_degrees_), screen_delta);
        image_view_center_.x -= image_delta.x / image_scale;
        image_view_center_.y -= image_delta.y / image_scale;
        image_last_pan_point_ = point;
        Render();
        return UiEventResult{
            .handled = true,
            .needs_render = false,
        };
    }

    return ui_.OnPointerMove(D2D1::Point2F(x, y));
}

UiEventResult Renderer::OnPointerDown(float x, float y)
{
    const D2D1_POINT_2F point = D2D1::Point2F(x, y);
    UiEventResult ui_result = ui_.OnPointerDown(point);
    if (ui_result.handled || !current_image_.bitmap) {
        return ui_result;
    }

    if (r_key_is_down_) {
        image_is_rotating_ = true;
        const D2D1_POINT_2F viewport_center = D2D1::Point2F(
            static_cast<float>(surfaces_.Width()) * 0.5f,
            static_cast<float>(surfaces_.Height()) * 0.5f);
        image_last_rotation_angle_ = AngleFromCenter(point, viewport_center);
        return UiEventResult{
            .handled = true,
            .needs_render = false,
            .captured = true,
        };
    }

    image_is_panning_ = true;
    image_last_pan_point_ = point;
    return UiEventResult{
        .handled = true,
        .needs_render = false,
        .captured = true,
    };
}

UiEventResult Renderer::OnPointerUp(float x, float y)
{
    if (image_is_rotating_) {
        image_is_rotating_ = false;
        image_last_rotation_angle_ = AngleFromCenter(
            D2D1::Point2F(x, y),
            D2D1::Point2F(static_cast<float>(surfaces_.Width()) * 0.5f, static_cast<float>(surfaces_.Height()) * 0.5f));
        return UiEventResult{
            .handled = true,
            .needs_render = false,
            .released_capture = true,
        };
    }

    if (image_is_panning_) {
        image_is_panning_ = false;
        image_last_pan_point_ = D2D1::Point2F(x, y);
        return UiEventResult{
            .handled = true,
            .needs_render = false,
            .released_capture = true,
        };
    }

    return ui_.OnPointerUp(D2D1::Point2F(x, y));
}

UiEventResult Renderer::OnPointerLeave()
{
    return ui_.OnPointerLeave();
}

bool Renderer::OnMouseWheel(float x, float y, int delta)
{
    if (!current_image_.bitmap || delta == 0 || surfaces_.Width() == 0 || surfaces_.Height() == 0) {
        return false;
    }

    const float image_width = static_cast<float>(current_image_.pixel_size.width);
    const float image_height = static_cast<float>(current_image_.pixel_size.height);
    if (image_width <= 0.0f || image_height <= 0.0f) {
        return false;
    }

    const float viewport_width = static_cast<float>(surfaces_.Width());
    const float viewport_height = static_cast<float>(surfaces_.Height());
    const float fit_scale = CurrentImageScale(surfaces_.Width(), surfaces_.Height()) / image_zoom_multiplier_;
    const float old_scale = fit_scale * image_zoom_multiplier_;
    const float wheel_steps = static_cast<float>(delta) / static_cast<float>(WHEEL_DELTA);
    const float new_zoom = std::clamp(
        image_zoom_multiplier_ * std::pow(kWheelZoomStep, wheel_steps),
        kMinImageZoomMultiplier,
        kMaxImageZoomMultiplier);
    if (new_zoom == image_zoom_multiplier_) {
        return false;
    }

    const D2D1_POINT_2F viewport_center = D2D1::Point2F(viewport_width * 0.5f, viewport_height * 0.5f);
    const D2D1_POINT_2F pointer = D2D1::Point2F(x, y);
    const D2D1_POINT_2F image_point_under_pointer = D2D1::Point2F(
        image_view_center_.x + (pointer.x - viewport_center.x) / old_scale,
        image_view_center_.y + (pointer.y - viewport_center.y) / old_scale);

    const float new_scale = fit_scale * new_zoom;
    image_view_center_ = D2D1::Point2F(
        image_point_under_pointer.x - (pointer.x - viewport_center.x) / new_scale,
        image_point_under_pointer.y - (pointer.y - viewport_center.y) / new_scale);
    image_zoom_multiplier_ = new_zoom;

    Render();
    return true;
}

bool Renderer::OnKeyDown(UINT virtual_key)
{
    if (virtual_key == 'R') {
        r_key_is_down_ = true;
        return true;
    }

    return false;
}

bool Renderer::OnKeyUp(UINT virtual_key)
{
    if (virtual_key == 'R') {
        const bool was_rotating = image_is_rotating_;
        r_key_is_down_ = false;
        image_is_rotating_ = false;
        return was_rotating;
    }

    return false;
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

void Renderer::InvokeUiCommandFromAccessibility(UiCommand command)
{
    PostMessageW(hwnd_, kImgViewerUiCommandMessage, static_cast<WPARAM>(command), 0);
}

HRESULT Renderer::LoadImageFile(const wchar_t* path)
{
    DecodedImage image;
    RETURN_IF_FAILED(image_decoder_.DecodeFirstFrame(path, d2d_context_.get(), &image));

    current_image_ = std::move(image);
    image_view_center_ = D2D1::Point2F(
        static_cast<float>(current_image_.pixel_size.width) * 0.5f,
        static_cast<float>(current_image_.pixel_size.height) * 0.5f);
    image_zoom_multiplier_ = 1.0f;
    image_rotation_degrees_ = 0.0f;
    RETURN_IF_FAILED(Render());
    return S_OK;
}

D2D1_SIZE_U Renderer::CurrentImagePixelSize() const
{
    return current_image_.pixel_size;
}

void Renderer::SetTitleText(const wchar_t* title)
{
    ui_.SetTitleText(title);
    Render();
}

void Renderer::SetWindowState(bool top_most, bool maximized)
{
    ui_.SetWindowState(top_most, maximized);
    Render();
}

bool Renderer::IsPointInCaptionDragArea(float x, float y) const
{
    return ui_.IsPointInCaptionDragArea(D2D1::Point2F(x, y));
}

size_t Renderer::UiElementCount() const
{
    return ui_.ElementCount();
}

const UiElementMetadata* Renderer::UiElementMetadataAt(size_t index) const
{
    return ui_.ElementMetadataAt(index);
}

const UiElementMetadata* Renderer::UiElementMetadata(UiElementId id) const
{
    return ui_.ElementMetadata(id);
}

D2D1_RECT_F Renderer::UiElementRect(UiElementId id) const
{
    return ui_.ElementRect(id);
}

D2D1_RECT_F Renderer::TestButtonRect() const
{
    return ui_.ElementRect(UiElementId::Test);
}

D2D1_RECT_F Renderer::OpenButtonRect() const
{
    return ui_.ElementRect(UiElementId::OpenImage);
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

float Renderer::CurrentImageScale(UINT viewport_width, UINT viewport_height) const
{
    if (!current_image_.bitmap || current_image_.pixel_size.width == 0 || current_image_.pixel_size.height == 0) {
        return 0.0f;
    }

    const float image_width = static_cast<float>(current_image_.pixel_size.width);
    const float image_height = static_cast<float>(current_image_.pixel_size.height);
    const float available_width = (std::max)(1.0f, static_cast<float>(viewport_width));
    const float available_height = (std::max)(1.0f, static_cast<float>(viewport_height));
    return (std::min)(available_width / image_width, available_height / image_height) * image_zoom_multiplier_;
}

HRESULT Renderer::RenderImageLayer()
{
    const UiDrawContext draw_context{
        .d2d_context = d2d_context_.get(),
    };
    const UiDraw draw(draw_context);
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

    d2d_context_->SetTarget(target_bitmap.get());
    d2d_context_->BeginDraw();
    draw.Clear(D2D1::ColorF(ui_theme::color::kWindowBackground));

    const CoordinateSpace coordinates = CoordinateSpace::FromWindow(hwnd_);
    const D2D1_SIZE_F size = coordinates.PhysicalToRender(surfaces_.Width(), surfaces_.Height());
    const float width = size.width;
    const float height = size.height;
    const D2D1_POINT_2F offset_render = coordinates.PhysicalToRender(offset);
    const D2D1_MATRIX_3X2_F root_transform = D2D1::Matrix3x2F::Translation(offset_render.x, offset_render.y);
    d2d_context_->SetTransform(root_transform);

    if (current_image_.bitmap) {
        const float image_width = static_cast<float>(current_image_.pixel_size.width);
        const float image_height = static_cast<float>(current_image_.pixel_size.height);
        const float image_scale = CurrentImageScale(surfaces_.Width(), surfaces_.Height());
        const float draw_width = image_width * image_scale;
        const float draw_height = image_height * image_scale;
        const D2D1_POINT_2F viewport_center = D2D1::Point2F(width * 0.5f, height * 0.5f);
        const D2D1_RECT_F destination = D2D1::RectF(
            viewport_center.x - image_view_center_.x * image_scale,
            viewport_center.y - image_view_center_.y * image_scale,
            viewport_center.x - image_view_center_.x * image_scale + draw_width,
            viewport_center.y - image_view_center_.y * image_scale + draw_height);
        d2d_context_->SetTransform(
            D2D1::Matrix3x2F::Rotation(image_rotation_degrees_, viewport_center) * root_transform);
        d2d_context_->DrawBitmap(
            current_image_.bitmap.get(),
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
            D2D1::ColorF(ui_theme::color::kAccent),
            ui_theme::metrics::kPathIconStrokeWidth / scale);
    }

    RETURN_IF_FAILED(d2d_context_->EndDraw());
    d2d_context_->SetTarget(nullptr);

    RETURN_IF_FAILED(surfaces_.Surface(SurfaceLayerId::Image)->EndDraw());

    return S_OK;
}

HRESULT Renderer::RenderUiOverlayLayer()
{
    const UiDrawContext draw_context{
        .d2d_context = d2d_context_.get(),
        .body_text_format = body_text_format_.get(),
        .icon_text_format = icon_text_format_.get(),
    };
    const UiDraw draw(draw_context);
    wil::com_ptr<ID2D1Bitmap1> target_bitmap;
    POINT offset = {};
    RETURN_IF_FAILED(BeginDrawLayer(
        SurfaceLayerId::UiOverlay,
        DXGI_ALPHA_MODE_PREMULTIPLIED,
        target_bitmap.put(),
        &offset));

    d2d_context_->SetTarget(target_bitmap.get());
    d2d_context_->BeginDraw();
    draw.Clear(D2D1::ColorF(D2D1::ColorF::Black, 0.0f));

    const CoordinateSpace coordinates = CoordinateSpace::FromWindow(hwnd_);
    const D2D1_SIZE_F size = coordinates.PhysicalToRender(surfaces_.Width(), surfaces_.Height());
    const float width = size.width;
    const D2D1_POINT_2F offset_render = coordinates.PhysicalToRender(offset);
    const D2D1_MATRIX_3X2_F root_transform = D2D1::Matrix3x2F::Translation(offset_render.x, offset_render.y);
    d2d_context_->SetTransform(root_transform);
    draw.DrawBodyText(
        kBodyText,
        ARRAYSIZE(kBodyText) - 1,
        StableRect(
            ui_theme::metrics::kPanelPadding,
            ui_theme::metrics::kBodyTextTop,
            width - ui_theme::metrics::kPanelPadding,
            ui_theme::metrics::kBodyTextBottom),
        D2D1::ColorF(ui_theme::color::kMutedText));
    draw.DrawIconText(
        kIconText,
        ARRAYSIZE(kIconText) - 1,
        StableRect(
            ui_theme::metrics::kPanelPadding,
            ui_theme::metrics::kIconTextTop,
            width - ui_theme::metrics::kPanelPadding,
            ui_theme::metrics::kIconTextBottom),
        D2D1::ColorF(ui_theme::color::kAccent));
    ui_.Draw(d2d_context_.get(), size, body_text_format_.get(), icon_text_format_.get());

    RETURN_IF_FAILED(d2d_context_->EndDraw());
    d2d_context_->SetTarget(nullptr);

    RETURN_IF_FAILED(surfaces_.Surface(SurfaceLayerId::UiOverlay)->EndDraw());

    return S_OK;
}
