#pragma once

#include <windows.h>

#include <d2d1_1.h>
#include <dcomp.h>
#include <dxgi1_2.h>

#include <wil/com.h>

#include "ui.draw.hpp"
#include "ui.graphics_device.hpp"

namespace ui_common_window {

HRESULT RegisterWindowClass(HINSTANCE instance, const wchar_t* class_name, WNDPROC wndproc, UINT style = CS_DBLCLKS);
template <typename T>
T* WindowUserData(HWND hwnd)
{
    return reinterpret_cast<T*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}
template <typename T>
void SetWindowUserData(HWND hwnd, T* user_data)
{
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(user_data));
}
POINT ClientToScreenPoint(HWND hwnd, D2D1_POINT_2F point);
HRESULT EnsureCompositionTarget(
    GraphicsDevice* graphics,
    HWND hwnd,
    wil::com_ptr<IDCompositionTarget>& target,
    wil::com_ptr<IDCompositionVisual>& root_visual);
HRESULT EnsureCompositionSurface(
    IDCompositionDevice* device,
    IDCompositionVisual* visual,
    wil::com_ptr<IDCompositionSurface>& surface,
    UINT width,
    UINT height,
    UINT* allocated_width,
    UINT* allocated_height);
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
    void* user_data);
using UiCompositionSurfaceDrawCallback = HRESULT (*)(const UiDrawContext& draw_context, void* user_data);
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
    D2D1_SIZE_F viewport_size,
    float dpi_scale,
    D2D1_POINT_2F translation,
    D2D1_COLOR_F clear_color,
    UiCompositionSurfaceDrawCallback callback,
    void* user_data);

} // namespace ui_common_window
