#pragma once

#include <windows.h>

#include <d2d1_1.h>

#include <optional>

#include "imgviewer.viewer.hpp"
#include "imgviewer.edit.hpp"
#include "script.view.hpp"
#include "ui.graphics_device.hpp"
#include "ui.renderer.hpp"

class ImgViewerRenderer final {
public:
    HRESULT Initialize(HWND hwnd, GraphicsDevice* graphics);
    HRESULT Resize();
    HRESULT Render(const ImgViewerController& viewer, const ImgViewerEditController& edit, ScriptView& ui);
    HRESULT SetUiOverlayVisible(bool visible);
    void SetCheckerboardBackground(bool enabled);
    D2D1_SIZE_U ViewportPixelSize() const;

private:
    // Inputs that determine the image layer's pixels. The image layer is the
    // expensive one (large bitmap blit), so it is only re-rendered when this key
    // changes (refactor.md per-layer invalidation). Edit/UI layers always render.
    struct ImageLayerKey final {
        const void* bitmap = nullptr;
        DXGI_FORMAT display_format = DXGI_FORMAT_UNKNOWN;
        UINT pixel_width = 0;
        UINT pixel_height = 0;
        float zoom_multiplier = 0.0f;
        float view_center_x = 0.0f;
        float view_center_y = 0.0f;
        float rotation_degrees = 0.0f;
        bool flipped_horizontal = false;
        bool flipped_vertical = false;
        bool pixelated_sampling = false;
        bool edit_active = false;
        int edit_rotation_quadrants = 0;
        bool checkerboard_background = false;
        float dpi_scale = 0.0f;
        UINT viewport_width = 0;
        UINT viewport_height = 0;

        bool operator==(const ImageLayerKey&) const = default;
    };

    ImageLayerKey ComputeImageLayerKey(const ImgViewerSnapshot& image, const ImgViewerEditSnapshot& edit) const;
    HRESULT RenderImageLayer(const ImgViewerSnapshot& image, const ImgViewerEditSnapshot& edit);
    HRESULT RenderEditLayer(const ImgViewerSnapshot& image, const ImgViewerEditSnapshot& edit);

    UiRenderer ui_renderer_;
    UiSurfaceId image_surface_ = kInvalidUiSurfaceId;
    UiSurfaceId edit_surface_ = kInvalidUiSurfaceId;
    UiSurfaceId ui_overlay_surface_ = kInvalidUiSurfaceId;
    bool ui_overlay_visible_ = true;
    bool checkerboard_background_ = false;
    std::optional<ImageLayerKey> last_image_key_;
    // Edit overlay is empty unless editing; once it has settled into the
    // inactive (cleared) state, stop repainting it. While editing it always
    // renders (stroke/selection content changes per frame).
    bool last_edit_active_ = true;
};
