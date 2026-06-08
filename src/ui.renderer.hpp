#pragma once

#include <windows.h>

#include <d2d1_1.h>
#include <dcomp.h>
#include <d3d11.h>
#include <dwrite.h>
#include <dxgi1_2.h>

#include <wil/com.h>

#include "ui.draw.hpp"
#include "ui.surface.hpp"
#include "ui.hpp"

struct UiSurfaceDrawContext final {
    UiDrawContext draw = {};
    ID2D1Factory1* d2d_factory = nullptr;
    D2D1_SIZE_U viewport_pixel_size = {};
    D2D1_POINT_2F offset = {};
    D2D1_MATRIX_3X2_F root_transform = {};
};

using UiSurfaceDrawCallback = HRESULT (*)(const UiSurfaceDrawContext& context, void* user_data);

class UiRenderer final {
public:
    HRESULT Initialize(HWND hwnd);
    HRESULT Resize();
    HRESULT RegisterSurface(const UiSurfaceDescriptor& descriptor, UiSurfaceId* id);
    HRESULT DrawSurface(UiSurfaceId id, UiSurfaceDrawCallback callback, void* user_data);
    HRESULT RenderUiOverlay(UiSurfaceId id, UiController& ui);
    HRESULT SetSurfaceVisible(UiSurfaceId id, bool visible);
    HRESULT SetSurfaceFormat(UiSurfaceId id, DXGI_FORMAT format);
    HRESULT Commit();

    D2D1_SIZE_U ViewportPixelSize() const;
    ID2D1Factory1* D2DFactory() const;
    IDWriteFactory* DWriteFactory() const;
    IDWriteTextFormat* BodyTextFormat() const;
    IDWriteTextFormat* IconTextFormat() const;
    ID2D1DeviceContext* BitmapDeviceContext() const;
    float DpiScale() const;

private:
    HRESULT ResizeSurfacesToClient();
    HRESULT BeginDrawSurface(UiSurfaceId id, ID2D1Bitmap1** target, POINT* offset);

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
};
