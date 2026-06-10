#pragma once

#include <windows.h>

#include <d2d1_1.h>
#include <d3d11.h>
#include <dcomp.h>
#include <dwrite.h>
#include <dxgi1_2.h>
#include <wincodec.h>

#include <wil/com.h>

using GraphicsDeviceDrawCallback = HRESULT (*)(ID2D1DeviceContext* d2d_context, void* user_data);
using GraphicsCompositionDrawCallback = HRESULT (*)(
    ID2D1DeviceContext* d2d_context,
    POINT offset,
    void* user_data);

class GraphicsDevice final {
public:
    HRESULT Initialize();

    ID2D1Factory1* D2DFactory() const;
    ID3D11Device* D3DDevice() const;
    ID3D11DeviceContext* D3DContext() const;
    IDXGIDevice* DxgiDevice() const;
    ID2D1Device* D2DDevice() const;
    ID2D1DeviceContext* D2DContext() const;
    IDWriteFactory* DWriteFactory() const;
    IDCompositionDevice* DCompDevice() const;

    HRESULT CreateCompositionTarget(
        HWND hwnd,
        IDCompositionTarget** target,
        IDCompositionVisual** root_visual) const;
    HRESULT CreateTargetBitmapFromDxgiSurface(
        IDXGISurface* surface,
        DXGI_FORMAT format,
        DXGI_ALPHA_MODE alpha_mode,
        ID2D1Bitmap1** bitmap) const;
    HRESULT DrawCompositionSurface(
        IDCompositionSurface* surface,
        UINT width,
        UINT height,
        DXGI_FORMAT format,
        DXGI_ALPHA_MODE alpha_mode,
        GraphicsCompositionDrawCallback callback,
        void* user_data) const;
    HRESULT RenderTextureToWicBitmap(
        IWICImagingFactory2* wic_factory,
        UINT width,
        UINT height,
        GraphicsDeviceDrawCallback callback,
        void* user_data,
        IWICBitmapSource** source) const;

private:
    wil::com_ptr<ID2D1Factory1> d2d_factory_;
    wil::com_ptr<ID3D11Device> d3d_device_;
    wil::com_ptr<ID3D11DeviceContext> d3d_context_;
    wil::com_ptr<IDXGIDevice> dxgi_device_;
    wil::com_ptr<ID2D1Device> d2d_device_;
    wil::com_ptr<ID2D1DeviceContext> d2d_context_;
    wil::com_ptr<IDWriteFactory> dwrite_factory_;
    wil::com_ptr<IDCompositionDevice> dcomp_device_;
};
