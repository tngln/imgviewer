#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <d2d1_1.h>
#include <dcomp.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#include <wil/com.h>

class Renderer final {
public:
    HRESULT Initialize(HWND hwnd);
    HRESULT Resize();

private:
    HRESULT CreateCompositionSurface();
    HRESULT RenderTestContent(UINT surface_width, UINT surface_height);

    HWND hwnd_ = nullptr;
    wil::com_ptr<ID2D1Factory1> d2d_factory_;
    wil::com_ptr<ID3D11Device> d3d_device_;
    wil::com_ptr<ID3D11DeviceContext> d3d_context_;
    wil::com_ptr<IDXGIDevice> dxgi_device_;
    wil::com_ptr<ID2D1Device> d2d_device_;
    wil::com_ptr<ID2D1DeviceContext> d2d_context_;
    wil::com_ptr<IDCompositionDevice> dcomp_device_;
    wil::com_ptr<IDCompositionTarget> dcomp_target_;
    wil::com_ptr<IDCompositionVisual> root_visual_;
    wil::com_ptr<IDCompositionSurface> surface_;
};
