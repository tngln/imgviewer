#include "ui.common_window.hpp"

#include <algorithm>

#include <d2d1helper.h>
#include <wil/result_macros.h>

namespace ui_common_window {

HRESULT RegisterWindowClass(HINSTANCE instance, const wchar_t* class_name, WNDPROC wndproc, UINT style)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, instance);
    RETURN_HR_IF_NULL(E_INVALIDARG, class_name);
    RETURN_HR_IF_NULL(E_INVALIDARG, wndproc);

    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.style = style;
    window_class.lpfnWndProc = wndproc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = class_name;
    const ATOM atom = RegisterClassExW(&window_class);
    if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        RETURN_LAST_ERROR();
    }
    return S_OK;
}

POINT ClientToScreenPoint(HWND hwnd, D2D1_POINT_2F point)
{
    POINT screen_point{static_cast<LONG>(point.x), static_cast<LONG>(point.y)};
    ClientToScreen(hwnd, &screen_point);
    return screen_point;
}

HRESULT EnsureCompositionTarget(
    GraphicsDevice* graphics,
    HWND hwnd,
    wil::com_ptr<IDCompositionTarget>& target,
    wil::com_ptr<IDCompositionVisual>& root_visual)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, graphics);
    RETURN_HR_IF_NULL(E_INVALIDARG, hwnd);

    if (target != nullptr && root_visual != nullptr) {
        return S_OK;
    }
    return graphics->CreateCompositionTarget(hwnd, target.put(), root_visual.put());
}

HRESULT EnsureCompositionSurface(
    IDCompositionDevice* device,
    IDCompositionVisual* visual,
    wil::com_ptr<IDCompositionSurface>& surface,
    UINT width,
    UINT height,
    UINT* allocated_width,
    UINT* allocated_height)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, device);
    RETURN_HR_IF_NULL(E_INVALIDARG, visual);
    RETURN_HR_IF_NULL(E_POINTER, allocated_width);
    RETURN_HR_IF_NULL(E_POINTER, allocated_height);

    width = (std::max)(1U, width);
    height = (std::max)(1U, height);
    if (surface != nullptr && *allocated_width == width && *allocated_height == height) {
        return S_OK;
    }

    surface.reset();
    RETURN_IF_FAILED(device->CreateSurface(width, height, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_ALPHA_MODE_PREMULTIPLIED, surface.put()));
    RETURN_IF_FAILED(visual->SetContent(surface.get()));
    RETURN_IF_FAILED(visual->SetOffsetX(0.0f));
    RETURN_IF_FAILED(visual->SetOffsetY(0.0f));
    *allocated_width = width;
    *allocated_height = height;
    return device->Commit();
}

HRESULT RenderCompositionSurface(
    GraphicsDevice* graphics,
    HWND hwnd,
    wil::com_ptr<IDCompositionTarget>& target,
    wil::com_ptr<IDCompositionVisual>& visual,
    wil::com_ptr<IDCompositionSurface>& surface,
    UINT width,
    UINT height,
    UINT* allocated_width,
    UINT* allocated_height,
    GraphicsCompositionDrawCallback callback,
    void* user_data)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, graphics);
    RETURN_HR_IF_NULL(E_INVALIDARG, hwnd);
    RETURN_HR_IF_NULL(E_INVALIDARG, callback);

    wil::com_ptr<IDCompositionDevice> dcomp_device = graphics->DCompDevice();
    RETURN_HR_IF_NULL(E_UNEXPECTED, dcomp_device);
    RETURN_IF_FAILED(EnsureCompositionTarget(graphics, hwnd, target, visual));
    RETURN_IF_FAILED(EnsureCompositionSurface(
        dcomp_device.get(),
        visual.get(),
        surface,
        width,
        height,
        allocated_width,
        allocated_height));
    RETURN_IF_FAILED(graphics->DrawCompositionSurface(
        surface.get(),
        width,
        height,
        DXGI_FORMAT_B8G8R8A8_UNORM,
        DXGI_ALPHA_MODE_PREMULTIPLIED,
        callback,
        user_data));
    return dcomp_device->Commit();
}

HRESULT RenderUiCompositionSurface(
    GraphicsDevice* graphics,
    HWND hwnd,
    wil::com_ptr<IDCompositionTarget>& target,
    wil::com_ptr<IDCompositionVisual>& visual,
    wil::com_ptr<IDCompositionSurface>& surface,
    UINT width,
    UINT height,
    UINT* allocated_width,
    UINT* allocated_height,
    IDWriteFactory* dwrite_factory,
    IDWriteTextFormat* body_text_format,
    IDWriteTextFormat* icon_text_format,
    D2D1_SIZE_F viewport_size,
    float dpi_scale,
    D2D1_POINT_2F translation,
    D2D1_COLOR_F clear_color,
    UiCompositionSurfaceDrawCallback callback,
    void* user_data)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, dwrite_factory);
    RETURN_HR_IF_NULL(E_INVALIDARG, callback);

    struct RenderState final {
        GraphicsDevice* graphics;
        IDWriteFactory* dwrite_factory;
        IDWriteTextFormat* body_text_format;
        IDWriteTextFormat* icon_text_format;
        D2D1_SIZE_F viewport_size;
        float dpi_scale;
        D2D1_POINT_2F translation;
        D2D1_COLOR_F clear_color;
        UiCompositionSurfaceDrawCallback callback;
        void* user_data;
    } state{
        graphics,
        dwrite_factory,
        body_text_format,
        icon_text_format,
        viewport_size,
        dpi_scale,
        translation,
        clear_color,
        callback,
        user_data,
    };

    return RenderCompositionSurface(
        graphics,
        hwnd,
        target,
        visual,
        surface,
        width,
        height,
        allocated_width,
        allocated_height,
        [](ID2D1DeviceContext* d2d_context, POINT offset, void* user_data) -> HRESULT {
            const auto* state = static_cast<const RenderState*>(user_data);
            RETURN_HR_IF_NULL(E_INVALIDARG, state);
            RETURN_HR_IF_NULL(E_INVALIDARG, state->graphics);
            RETURN_HR_IF_NULL(E_INVALIDARG, state->callback);

            const UiDrawContext draw_context{
                .d2d_context = d2d_context,
                .d2d_factory = state->graphics->D2DFactory(),
                .dwrite_factory = state->dwrite_factory,
                .body_text_format = state->body_text_format,
                .icon_text_format = state->icon_text_format,
                .viewport_size = state->viewport_size,
                .dpi_scale = state->dpi_scale,
            };
            d2d_context->SetTransform(
                D2D1::Matrix3x2F::Scale(state->dpi_scale, state->dpi_scale) *
                D2D1::Matrix3x2F::Translation(
                    static_cast<float>(offset.x) + state->translation.x,
                    static_cast<float>(offset.y) + state->translation.y));
            d2d_context->Clear(state->clear_color);
            return state->callback(draw_context, state->user_data);
        },
        &state);
}

} // namespace ui_common_window
