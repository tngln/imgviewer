#pragma once

#include <windows.h>

#include <dcomp.h>
#include <dxgi.h>

#include <wil/com.h>

enum class SurfaceLayerId {
    Image,
    UiOverlay,
};

class SurfaceManager final {
public:
    HRESULT Initialize(IDCompositionDevice* device, IDCompositionVisual* root_visual);
    HRESULT Resize(UINT width, UINT height);

    IDCompositionSurface* Surface(SurfaceLayerId id) const;
    UINT Width() const { return width_; }
    UINT Height() const { return height_; }

private:
    struct Layer final {
        wil::com_ptr<IDCompositionVisual> visual;
        wil::com_ptr<IDCompositionSurface> surface;
        DXGI_ALPHA_MODE alpha_mode = DXGI_ALPHA_MODE_IGNORE;
        UINT allocated_width = 0;
        UINT allocated_height = 0;
    };

    Layer& LayerFor(SurfaceLayerId id);
    const Layer& LayerFor(SurfaceLayerId id) const;
    HRESULT CreateLayerSurface(Layer& layer, UINT width, UINT height);

    wil::com_ptr<IDCompositionDevice> device_;
    wil::com_ptr<IDCompositionVisual> root_visual_;
    Layer image_layer_;
    Layer ui_overlay_layer_;
    UINT width_ = 1;
    UINT height_ = 1;
};
