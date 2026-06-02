#pragma once

#include <windows.h>

#include <dcomp.h>
#include <dxgi.h>

#include <array>
#include <cstddef>

#include <wil/com.h>

enum class UiSurfaceLayerId {
    Image,
    UiOverlay,
    Count,
};

constexpr size_t kUiSurfaceLayerCount = static_cast<size_t>(UiSurfaceLayerId::Count);

class UiSurfaceManager final {
public:
    HRESULT Initialize(IDCompositionDevice* device, IDCompositionVisual* root_visual);
    HRESULT Resize(UINT width, UINT height);

    IDCompositionSurface* Surface(UiSurfaceLayerId id) const;
    DXGI_ALPHA_MODE AlphaMode(UiSurfaceLayerId id) const;
    UINT Width() const { return width_; }
    UINT Height() const { return height_; }

private:
    struct Layer final {
        wil::com_ptr<IDCompositionVisual> visual;
        wil::com_ptr<IDCompositionSurface> surface;
        UINT allocated_width = 0;
        UINT allocated_height = 0;
    };

    Layer& LayerFor(UiSurfaceLayerId id);
    const Layer& LayerFor(UiSurfaceLayerId id) const;
    HRESULT CreateLayerSurface(UiSurfaceLayerId id, UINT width, UINT height);

    wil::com_ptr<IDCompositionDevice> device_;
    wil::com_ptr<IDCompositionVisual> root_visual_;
    std::array<Layer, kUiSurfaceLayerCount> layers_;
    UINT width_ = 1;
    UINT height_ = 1;
};
