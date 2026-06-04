#pragma once

#include <windows.h>

#include <d2d1_1.h>

#include "imgviewer.viewer.hpp"
#include "ui.renderer.hpp"

class ImgViewerRenderer final {
public:
    HRESULT Initialize(HWND hwnd);
    HRESULT Resize();
    HRESULT Render(const ImgViewerController& viewer, UiController& ui);
    HRESULT SetUiOverlayVisible(bool visible);
    D2D1_SIZE_U ViewportPixelSize() const;
    ID2D1DeviceContext* BitmapDeviceContext() const;

private:
    HRESULT RenderImageLayer(const ImgViewerSnapshot& image);

    UiRenderer ui_renderer_;
    UiSurfaceId image_surface_ = kInvalidUiSurfaceId;
    UiSurfaceId ui_overlay_surface_ = kInvalidUiSurfaceId;
    bool ui_overlay_visible_ = true;
};
