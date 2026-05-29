#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <d2d1_1.h>
#include <dcomp.h>
#include <d3d11.h>
#include <dwrite.h>
#include <dxgi1_2.h>

#include <wil/com.h>

#include "ui.a11y.hpp"
#include "ui.hpp"

class Renderer final {
public:
    HRESULT Initialize(HWND hwnd);
    HRESULT Resize();
    HRESULT Render();
    UiEventResult OnPointerMove(float x, float y);
    UiEventResult OnPointerDown(float x, float y);
    UiEventResult OnPointerUp(float x, float y);
    UiEventResult OnPointerLeave();
    IRawElementProviderSimple* GetAccessibilityProvider();
    void InvokeTestButtonFromAccessibility();
    D2D1_RECT_F TestButtonRect() const;

private:
    HRESULT CreateCompositionSurface();
    HRESULT RenderTestContent();

    HWND hwnd_ = nullptr;
    UINT surface_width_ = 1;
    UINT surface_height_ = 1;
    UiController ui_;
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
    wil::com_ptr<IDCompositionSurface> surface_;
    wil::com_ptr<IRawElementProviderSimple> accessibility_provider_;
};
