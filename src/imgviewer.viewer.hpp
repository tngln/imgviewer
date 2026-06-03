#pragma once

#include <windows.h>

#include <d2d1_1.h>

#include "imgviewer.action.hpp"
#include "image.decoder.hpp"

struct ImgViewerEventResult final {
    bool handled = false;
    bool needs_render = false;
    bool captured = false;
    bool released_capture = false;
};

struct ImgViewerSnapshot final {
    ID2D1Bitmap* bitmap = nullptr;
    D2D1_SIZE_U pixel_size = {};
    D2D1_POINT_2F view_center = {};
    float zoom_multiplier = 1.0f;
    float rotation_degrees = 0.0f;
    bool flipped_horizontal = false;
    bool flipped_vertical = false;
};

class ImgViewerController final {
public:
    HRESULT Initialize();
    HRESULT LoadImageFile(const wchar_t* path, ID2D1DeviceContext* d2d_context);
    D2D1_SIZE_U CurrentImagePixelSize() const;
    ImgViewerSnapshot Snapshot() const;

    ImgViewerEventResult OnPointerMove(float x, float y, D2D1_SIZE_U viewport_size);
    ImgViewerEventResult OnPointerDown(float x, float y, D2D1_SIZE_U viewport_size);
    ImgViewerEventResult OnPointerUp(float x, float y, D2D1_SIZE_U viewport_size);
    bool OnMouseWheel(float x, float y, int delta, D2D1_SIZE_U viewport_size);
    bool OnActionDown(ImgViewerAction action);
    ImgViewerEventResult OnActionUp(ImgViewerAction action);
    bool ZoomByStep(int steps, D2D1_SIZE_U viewport_size);
    bool RotateClockwise();
    bool FlipHorizontal();
    bool FlipVertical();
    bool ResetView();

private:
    float CurrentImageScale(D2D1_SIZE_U viewport_size) const;
    bool ZoomAtPoint(float x, float y, float steps, D2D1_SIZE_U viewport_size);

    ImageDecoder image_decoder_;
    DecodedImage current_image_;
    D2D1_POINT_2F image_view_center_ = {};
    float image_zoom_multiplier_ = 1.0f;
    float image_rotation_degrees_ = 0.0f;
    bool image_flipped_horizontal_ = false;
    bool image_flipped_vertical_ = false;
    bool image_is_panning_ = false;
    bool image_is_rotating_ = false;
    bool r_key_is_down_ = false;
    bool r_key_started_rotation_ = false;
    D2D1_POINT_2F image_last_pan_point_ = {};
    float image_last_rotation_angle_ = 0.0f;
};
