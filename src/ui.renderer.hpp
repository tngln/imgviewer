#pragma once

#include <windows.h>

#include <d2d1_1.h>
#include <dcomp.h>
#include <d3d11.h>
#include <dwrite.h>
#include <dxgi1_2.h>

#include <wil/com.h>

#include "imgviewer.viewer.hpp"
#include "ui.surface.hpp"
#include "ui.hpp"

class UiRenderer final {
public:
    HRESULT Initialize(HWND hwnd);
    HRESULT Resize();
    HRESULT Render(const ImgViewerController& viewer, UiController& ui);
    D2D1_SIZE_U ViewportPixelSize() const;
    ID2D1DeviceContext* BitmapDeviceContext() const;

private:
    HRESULT ResizeSurfacesToClient();
    HRESULT BeginDrawSurface(UiSurfaceId id, ID2D1Bitmap1** target, POINT* offset);
    HRESULT RenderImageLayer(const ImgViewerSnapshot& image);
    HRESULT RenderUiOverlayLayer(UiController& ui);

    HWND hwnd_ = nullptr;
    wil::com_ptr<ID2D1Factory1> d2d_factory_;
    wil::com_ptr<ID3D11Device> d3d_device_;
    wil::com_ptr<ID3D11DeviceContext> d3d_context_;
    wil::com_ptr<IDXGIDevice> dxgi_device_;
    wil::com_ptr<ID2D1Device> d2d_device_;
    wil::com_ptr<ID2D1DeviceContext> d2d_context_;
    wil::com_ptr<IDWriteFactory> dwrite_factory_;
    wil::com_ptr<IDWriteTextFormat> body_text_format_;
    wil::com_ptr<IDWriteTextFormat> icon_text_format_;
    wil::com_ptr<IDCompositionDevice> dcomp_device_;
    wil::com_ptr<IDCompositionTarget> dcomp_target_;
    wil::com_ptr<IDCompositionVisual> root_visual_;
    UiSurfaceManager surfaces_;
    UiSurfaceId image_surface_;
    UiSurfaceId ui_overlay_surface_;
    UiSurfaceId popup_surface_;
};
