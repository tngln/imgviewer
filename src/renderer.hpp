#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <d2d1_1.h>
#include <dcomp.h>
#include <d3d11.h>
#include <dwrite.h>
#include <dxgi1_2.h>

#include <wil/com.h>

#include "image.decoder.hpp"
#include "surface.manager.hpp"
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
    bool OnMouseWheel(float x, float y, int delta);
    bool OnKeyDown(UINT virtual_key);
    bool OnKeyUp(UINT virtual_key);
    IRawElementProviderSimple* GetAccessibilityProvider();
    void InvokeTestButtonFromAccessibility();
    void InvokeOpenImageFromAccessibility();
    void InvokeUiCommandFromAccessibility(UiCommand command);
    HRESULT LoadImageFile(const wchar_t* path);
    D2D1_SIZE_U CurrentImagePixelSize() const;
    void SetTitleText(const wchar_t* title);
    void SetWindowState(bool top_most, bool maximized);
    bool IsPointInCaptionDragArea(float x, float y) const;
    D2D1_RECT_F UiElementRect(UiElementId id) const;
    D2D1_RECT_F TestButtonRect() const;
    D2D1_RECT_F OpenButtonRect() const;

private:
    HRESULT ResizeSurfacesToClient();
    HRESULT BeginDrawLayer(
        SurfaceLayerId id,
        DXGI_ALPHA_MODE alpha_mode,
        ID2D1Bitmap1** target,
        POINT* offset);
    float CurrentImageScale(UINT viewport_width, UINT viewport_height) const;
    HRESULT RenderImageLayer();
    HRESULT RenderUiOverlayLayer();

    HWND hwnd_ = nullptr;
    UiController ui_;
    ImageDecoder image_decoder_;
    DecodedImage current_image_;
    D2D1_POINT_2F image_view_center_ = {};
    float image_zoom_multiplier_ = 1.0f;
    float image_rotation_degrees_ = 0.0f;
    bool image_is_panning_ = false;
    bool image_is_rotating_ = false;
    bool r_key_is_down_ = false;
    D2D1_POINT_2F image_last_pan_point_ = {};
    float image_last_rotation_angle_ = 0.0f;
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
    SurfaceManager surfaces_;
    wil::com_ptr<IRawElementProviderSimple> accessibility_provider_;
};
