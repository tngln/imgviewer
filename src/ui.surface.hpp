#pragma once

#include <windows.h>

#include <dcomp.h>
#include <dxgi.h>

#include <cstdint>
#include <string>
#include <vector>

#include <d2d1.h>
#include <wil/com.h>

struct UiSurfaceId final {
    uint32_t value = 0;
};

constexpr bool operator==(UiSurfaceId left, UiSurfaceId right)
{
    return left.value == right.value;
}

constexpr bool operator!=(UiSurfaceId left, UiSurfaceId right)
{
    return !(left == right);
}

constexpr UiSurfaceId kInvalidUiSurfaceId{};

struct UiSurfaceDescriptor final {
    const wchar_t* name = L"";
    DXGI_ALPHA_MODE alpha_mode = DXGI_ALPHA_MODE_PREMULTIPLIED;
    int z_order = 0;
    bool auto_resize = true;
    bool initially_visible = true;
};

struct UiSurfaceSize final {
    UINT width = 0;
    UINT height = 0;
};

class UiSurfaceManager final {
public:
    HRESULT Initialize(IDCompositionDevice* device, IDCompositionVisual* root_visual);
    HRESULT RegisterSurface(const UiSurfaceDescriptor& descriptor, UiSurfaceId* id);
    void UnregisterSurface(UiSurfaceId id);
    HRESULT EnsureSurface(UiSurfaceId id, UINT width, UINT height);
    HRESULT ResizeAutoSurfaces(UINT width, UINT height);
    HRESULT SetVisible(UiSurfaceId id, bool visible);
    HRESULT SetZOrder(UiSurfaceId id, int z_order);

    IDCompositionSurface* Surface(UiSurfaceId id) const;
    DXGI_ALPHA_MODE AlphaMode(UiSurfaceId id) const;
    UiSurfaceSize Size(UiSurfaceId id) const;
    UINT Width() const { return width_; }
    UINT Height() const { return height_; }

private:
    struct Layer final {
        UiSurfaceId id;
        std::wstring name;
        DXGI_ALPHA_MODE alpha_mode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        int z_order = 0;
        bool auto_resize = true;
        bool visible = true;
        wil::com_ptr<IDCompositionVisual> visual;
        wil::com_ptr<IDCompositionSurface> surface;
        UINT allocated_width = 0;
        UINT allocated_height = 0;
    };

    Layer* FindLayer(UiSurfaceId id);
    const Layer* FindLayer(UiSurfaceId id) const;
    HRESULT RebuildVisualOrder();
    HRESULT CreateLayerSurface(Layer& layer, UINT width, UINT height);

    wil::com_ptr<IDCompositionDevice> device_;
    wil::com_ptr<IDCompositionVisual> root_visual_;
    std::vector<Layer> layers_;
    UINT width_ = 1;
    UINT height_ = 1;
    uint32_t next_id_ = 1;
};

