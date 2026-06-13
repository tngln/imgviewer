#pragma once

#include <windows.h>

#include <d2d1_1.h>

#include <vector>

#include "imgviewer.action.hpp"
#include "image.analysis.hpp"
#include "image.decoder.hpp"
#include "image.encoder.hpp"

struct ImgViewerEventResult final {
    bool handled = false;
    bool captured = false;
    bool released_capture = false;
};

struct ImgViewerSnapshot final {
    ID2D1Bitmap* bitmap = nullptr;
    D2D1_SIZE_U pixel_size = {};
    DXGI_FORMAT display_format = DXGI_FORMAT_B8G8R8A8_UNORM;
    D2D1_POINT_2F view_center = {};
    float zoom_multiplier = 1.0f;
    float rotation_degrees = 0.0f;
    bool flipped_horizontal = false;
    bool flipped_vertical = false;
    bool pixelated_sampling = false;
};

struct ImgViewerColorSample final {
    BYTE red = 0;
    BYTE green = 0;
    BYTE blue = 0;
    BYTE alpha = 0;
};

struct ImgViewerAnimationState final {
    bool available = false;
    bool playing = false;
    bool loop = true;
    size_t current_frame = 0;
    size_t total_frames = 0;
};

class ImgViewerController final {
public:
    HRESULT Initialize();
    HRESULT LoadImageFile(const wchar_t* path, ID2D1DeviceContext* d2d_context);
    HRESULT LoadBitmapSource(IWICBitmapSource* source, ID2D1DeviceContext* d2d_context);
    HRESULT SaveCurrentImagePng(const wchar_t* path);
    IWICImagingFactory2* WicFactory() const;
    IWICBitmapSource* CurrentPixelSource() const;
    bool HasCurrentImage() const;
    D2D1_SIZE_U CurrentImagePixelSize() const;
    const ImageMetadata& CurrentImageMetadata() const;
    ImgViewerSnapshot Snapshot() const;
    bool HasTransientCapture() const;
    ImgViewerAnimationState AnimationState() const;
    bool HasAnimation() const;
    bool ToggleAnimationPlayback();
    bool ToggleAnimationLoop();
    bool StepAnimationFrame(int direction);
    bool AdvanceAnimation(UINT elapsed_ms);

    ImgViewerEventResult OnPointerMove(float x, float y, D2D1_SIZE_U viewport_size);
    ImgViewerEventResult OnPointerDown(float x, float y, D2D1_SIZE_U viewport_size);
    ImgViewerEventResult OnPointerUp(float x, float y, D2D1_SIZE_U viewport_size);
    bool OnMouseWheel(float x, float y, int delta, D2D1_SIZE_U viewport_size);
    bool OnActionDown(ImgViewerAction action);
    ImgViewerEventResult OnActionUp(ImgViewerAction action);
    bool ZoomByStep(int steps, D2D1_SIZE_U viewport_size);
    bool FitWindow();
    bool ActualSize(D2D1_SIZE_U viewport_size);
    bool CenterOnImagePoint(D2D1_POINT_2F point);
    bool RotateClockwise();
    bool FlipHorizontal();
    bool FlipVertical();
    bool ResetView();
    void SetPixelatedSampling(bool enabled);
    void CancelTransientViewGesture();
    bool SampleColorAt(float x, float y, D2D1_SIZE_U viewport_size, ImgViewerColorSample* color) const;
    HRESULT AnalyzeCurrentImage(ImagePixelAnalysis* analysis) const;

private:
    void SetCurrentImage(DecodedImage image);
    void SetCurrentImageSet(DecodedImageSet image_set);
    void SetAnimationFrame(size_t index);
    float CurrentImageScale(D2D1_SIZE_U viewport_size) const;
    bool ImagePointFromViewportPoint(float x, float y, D2D1_SIZE_U viewport_size, D2D1_POINT_2F* image_point) const;
    bool ZoomAtPoint(float x, float y, float steps, D2D1_SIZE_U viewport_size);

    ImageDecoder image_decoder_;
    ImageEncoder image_encoder_;
    DecodedImage current_image_;
    std::vector<DecodedAnimationFrame> animation_frames_;
    size_t current_animation_frame_ = 0;
    UINT animation_elapsed_ms_ = 0;
    bool animation_playing_ = false;
    bool animation_loop_ = true;
    D2D1_POINT_2F image_view_center_ = {};
    float image_zoom_multiplier_ = 1.0f;
    float image_rotation_degrees_ = 0.0f;
    bool image_flipped_horizontal_ = false;
    bool image_flipped_vertical_ = false;
    bool pixelated_sampling_ = false;
    bool image_is_panning_ = false;
    bool image_is_rotating_ = false;
    bool r_key_is_down_ = false;
    bool r_key_started_rotation_ = false;
    D2D1_POINT_2F image_last_pan_point_ = {};
    float image_last_rotation_angle_ = 0.0f;
};
