#include "imgviewer.viewer.hpp"

#include <algorithm>
#include <cmath>

#include <d2d1helper.h>
#include <wil/result_macros.h>

#include "math.hpp"

namespace {

constexpr float kMinImageZoomMultiplier = 0.05f;
constexpr float kMaxImageZoomMultiplier = 64.0f;
constexpr float kWheelZoomStep = 1.12f;
constexpr float kRadiansToDegrees = 57.2957795f;

} // namespace

HRESULT ImgViewerController::Initialize()
{
    RETURN_IF_FAILED(image_decoder_.Initialize());
    RETURN_IF_FAILED(image_encoder_.Initialize(image_decoder_.WicFactory()));
    return S_OK;
}

HRESULT ImgViewerController::LoadImageFile(const wchar_t* path, ID2D1DeviceContext* d2d_context)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, path);
    RETURN_HR_IF_NULL(E_INVALIDARG, d2d_context);

    DecodedImageSet image_set;
    RETURN_IF_FAILED(image_decoder_.DecodeImageFile(path, d2d_context, &image_set));

    SetCurrentImageSet(std::move(image_set));
    return S_OK;
}

HRESULT ImgViewerController::LoadBitmapSource(IWICBitmapSource* source, ID2D1DeviceContext* d2d_context)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, source);
    RETURN_HR_IF_NULL(E_INVALIDARG, d2d_context);

    DecodedImage image;
    RETURN_IF_FAILED(image_decoder_.DecodeBitmapSource(source, d2d_context, &image));

    SetCurrentImage(std::move(image));
    return S_OK;
}

HRESULT ImgViewerController::SaveCurrentImagePng(const wchar_t* path)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, path);
    RETURN_HR_IF_NULL(E_UNEXPECTED, current_image_.pixel_source);
    RETURN_IF_FAILED(image_encoder_.SavePngFile(current_image_.pixel_source.get(), path));
    return S_OK;
}

IWICImagingFactory2* ImgViewerController::WicFactory() const
{
    return image_decoder_.WicFactory();
}

IWICBitmapSource* ImgViewerController::CurrentPixelSource() const
{
    return current_image_.pixel_source.get();
}

bool ImgViewerController::HasCurrentImage() const
{
    return current_image_.bitmap != nullptr && current_image_.pixel_source != nullptr;
}

void ImgViewerController::SetCurrentImage(DecodedImage image)
{
    current_image_ = std::move(image);
    animation_frames_.clear();
    current_animation_frame_ = 0;
    animation_elapsed_ms_ = 0;
    animation_playing_ = false;
    animation_loop_ = true;
    image_view_center_ = D2D1::Point2F(
        static_cast<float>(current_image_.pixel_size.width) * 0.5f,
        static_cast<float>(current_image_.pixel_size.height) * 0.5f);
    image_zoom_multiplier_ = 1.0f;
    image_rotation_degrees_ = 0.0f;
    image_flipped_horizontal_ = false;
    image_flipped_vertical_ = false;
    image_is_panning_ = false;
    image_is_rotating_ = false;
    r_key_is_down_ = false;
    r_key_started_rotation_ = false;
}

void ImgViewerController::SetCurrentImageSet(DecodedImageSet image_set)
{
    current_image_ = std::move(image_set.image);
    animation_frames_ = std::move(image_set.animation_frames);
    current_animation_frame_ = 0;
    animation_elapsed_ms_ = 0;
    animation_playing_ = animation_frames_.size() > 1;
    animation_loop_ = image_set.animation_loop;
    if (!animation_frames_.empty()) {
        current_image_ = DecodedImage{
            .bitmap = animation_frames_.front().image.bitmap,
            .pixel_source = animation_frames_.front().image.pixel_source,
            .pixel_size = animation_frames_.front().image.pixel_size,
            .metadata = std::move(current_image_.metadata),
        };
    }
    image_view_center_ = D2D1::Point2F(
        static_cast<float>(current_image_.pixel_size.width) * 0.5f,
        static_cast<float>(current_image_.pixel_size.height) * 0.5f);
    image_zoom_multiplier_ = 1.0f;
    image_rotation_degrees_ = 0.0f;
    image_flipped_horizontal_ = false;
    image_flipped_vertical_ = false;
    image_is_panning_ = false;
    image_is_rotating_ = false;
    r_key_is_down_ = false;
    r_key_started_rotation_ = false;
}

void ImgViewerController::SetAnimationFrame(size_t index)
{
    if (animation_frames_.empty()) {
        return;
    }

    current_animation_frame_ = (std::min)(index, animation_frames_.size() - 1);
    ImageMetadata metadata = std::move(current_image_.metadata);
    current_image_ = DecodedImage{
        .bitmap = animation_frames_[current_animation_frame_].image.bitmap,
        .pixel_source = animation_frames_[current_animation_frame_].image.pixel_source,
        .pixel_size = animation_frames_[current_animation_frame_].image.pixel_size,
        .metadata = std::move(metadata),
    };
}

D2D1_SIZE_U ImgViewerController::CurrentImagePixelSize() const
{
    return current_image_.pixel_size;
}

const ImageMetadata& ImgViewerController::CurrentImageMetadata() const
{
    return current_image_.metadata;
}

ImgViewerSnapshot ImgViewerController::Snapshot() const
{
    return ImgViewerSnapshot{
        .bitmap = current_image_.bitmap.get(),
        .pixel_size = current_image_.pixel_size,
        .view_center = image_view_center_,
        .zoom_multiplier = image_zoom_multiplier_,
        .rotation_degrees = image_rotation_degrees_,
        .flipped_horizontal = image_flipped_horizontal_,
        .flipped_vertical = image_flipped_vertical_,
        .pixelated_sampling = pixelated_sampling_,
    };
}

bool ImgViewerController::HasTransientCapture() const
{
    return image_is_panning_ || image_is_rotating_;
}

ImgViewerAnimationState ImgViewerController::AnimationState() const
{
    return ImgViewerAnimationState{
        .available = HasAnimation(),
        .playing = animation_playing_,
        .loop = animation_loop_,
        .current_frame = current_animation_frame_ + (HasAnimation() ? 1 : 0),
        .total_frames = animation_frames_.size(),
    };
}

bool ImgViewerController::HasAnimation() const
{
    return animation_frames_.size() > 1;
}

bool ImgViewerController::ToggleAnimationPlayback()
{
    if (!HasAnimation()) {
        return false;
    }

    animation_playing_ = !animation_playing_;
    animation_elapsed_ms_ = 0;
    return true;
}

bool ImgViewerController::ToggleAnimationLoop()
{
    if (!HasAnimation()) {
        return false;
    }

    animation_loop_ = !animation_loop_;
    return true;
}

bool ImgViewerController::StepAnimationFrame(int direction)
{
    if (!HasAnimation() || direction == 0) {
        return false;
    }

    animation_playing_ = false;
    animation_elapsed_ms_ = 0;
    const size_t total = animation_frames_.size();
    size_t next = current_animation_frame_;
    if (direction > 0) {
        next = current_animation_frame_ + 1 < total ? current_animation_frame_ + 1 : 0;
    } else {
        next = current_animation_frame_ == 0 ? total - 1 : current_animation_frame_ - 1;
    }
    SetAnimationFrame(next);
    return true;
}

bool ImgViewerController::AdvanceAnimation(UINT elapsed_ms)
{
    if (!HasAnimation() || !animation_playing_ || elapsed_ms == 0) {
        return false;
    }

    animation_elapsed_ms_ += elapsed_ms;
    bool changed = false;
    while (animation_elapsed_ms_ >= animation_frames_[current_animation_frame_].duration_ms) {
        animation_elapsed_ms_ -= animation_frames_[current_animation_frame_].duration_ms;
        if (current_animation_frame_ + 1 >= animation_frames_.size()) {
            if (!animation_loop_) {
                const bool was_playing = animation_playing_;
                animation_playing_ = false;
                animation_elapsed_ms_ = 0;
                SetAnimationFrame(animation_frames_.size() - 1);
                return changed || was_playing;
            }
            SetAnimationFrame(0);
        } else {
            SetAnimationFrame(current_animation_frame_ + 1);
        }
        changed = true;
    }

    return changed;
}

ImgViewerEventResult ImgViewerController::OnPointerMove(float x, float y, D2D1_SIZE_U viewport_size)
{
    if (image_is_rotating_) {
        const D2D1_POINT_2F point = D2D1::Point2F(x, y);
        const D2D1_POINT_2F viewport_center = D2D1::Point2F(
            static_cast<float>(viewport_size.width) * 0.5f,
            static_cast<float>(viewport_size.height) * 0.5f);
        const float angle = math::AngleFromCenter(point, viewport_center);
        image_rotation_degrees_ += (angle - image_last_rotation_angle_) * kRadiansToDegrees;
        image_last_rotation_angle_ = angle;
        return ImgViewerEventResult{
            .handled = true,
            .needs_render = true,
        };
    }

    if (image_is_panning_) {
        const float image_scale = CurrentImageScale(viewport_size);
        if (image_scale <= 0.0f) {
            return {};
        }

        const D2D1_POINT_2F point = D2D1::Point2F(x, y);
        const D2D1_POINT_2F screen_delta = D2D1::Point2F(
            point.x - image_last_pan_point_.x,
            point.y - image_last_pan_point_.y);
        const float flip_x = image_flipped_horizontal_ ? -1.0f : 1.0f;
        const float flip_y = image_flipped_vertical_ ? -1.0f : 1.0f;
        const D2D1_POINT_2F image_delta = math::TransformVector(
            D2D1::Matrix3x2F::Rotation(-image_rotation_degrees_) *
                D2D1::Matrix3x2F::Scale(flip_x, flip_y),
            screen_delta);
        image_view_center_.x -= image_delta.x / image_scale;
        image_view_center_.y -= image_delta.y / image_scale;
        image_last_pan_point_ = point;
        return ImgViewerEventResult{
            .handled = true,
            .needs_render = true,
        };
    }

    return {};
}

ImgViewerEventResult ImgViewerController::OnPointerDown(float x, float y, D2D1_SIZE_U viewport_size)
{
    if (!current_image_.bitmap) {
        return {};
    }

    const D2D1_POINT_2F point = D2D1::Point2F(x, y);
    if (r_key_is_down_) {
        image_is_rotating_ = true;
        r_key_started_rotation_ = true;
        const D2D1_POINT_2F viewport_center = D2D1::Point2F(
            static_cast<float>(viewport_size.width) * 0.5f,
            static_cast<float>(viewport_size.height) * 0.5f);
        image_last_rotation_angle_ = math::AngleFromCenter(point, viewport_center);
        return ImgViewerEventResult{
            .handled = true,
            .captured = true,
        };
    }

    image_is_panning_ = true;
    image_last_pan_point_ = point;
    return ImgViewerEventResult{
        .handled = true,
        .captured = true,
    };
}

ImgViewerEventResult ImgViewerController::OnPointerUp(float x, float y, D2D1_SIZE_U viewport_size)
{
    if (image_is_rotating_) {
        image_is_rotating_ = false;
        image_last_rotation_angle_ = math::AngleFromCenter(
            D2D1::Point2F(x, y),
            D2D1::Point2F(
                static_cast<float>(viewport_size.width) * 0.5f,
                static_cast<float>(viewport_size.height) * 0.5f));
        return ImgViewerEventResult{
            .handled = true,
            .released_capture = true,
        };
    }

    if (image_is_panning_) {
        image_is_panning_ = false;
        image_last_pan_point_ = D2D1::Point2F(x, y);
        return ImgViewerEventResult{
            .handled = true,
            .released_capture = true,
        };
    }

    return {};
}

bool ImgViewerController::OnMouseWheel(float x, float y, int delta, D2D1_SIZE_U viewport_size)
{
    if (delta == 0) {
        return false;
    }

    return ZoomAtPoint(x, y, static_cast<float>(delta) / static_cast<float>(WHEEL_DELTA), viewport_size);
}

bool ImgViewerController::ZoomAtPoint(float x, float y, float steps, D2D1_SIZE_U viewport_size)
{
    if (!current_image_.bitmap || steps == 0.0f || viewport_size.width == 0 || viewport_size.height == 0) {
        return false;
    }

    const float fit_scale = math::FitScale(current_image_.pixel_size, viewport_size);
    if (fit_scale <= 0.0f) {
        return false;
    }

    const float viewport_width = static_cast<float>(viewport_size.width);
    const float viewport_height = static_cast<float>(viewport_size.height);
    const float old_scale = fit_scale * image_zoom_multiplier_;
    const float new_zoom = std::clamp(
        image_zoom_multiplier_ * std::pow(kWheelZoomStep, steps),
        kMinImageZoomMultiplier,
        kMaxImageZoomMultiplier);
    if (new_zoom == image_zoom_multiplier_) {
        return false;
    }

    const D2D1_POINT_2F viewport_center = D2D1::Point2F(viewport_width * 0.5f, viewport_height * 0.5f);
    const D2D1_POINT_2F pointer = D2D1::Point2F(x, y);
    const D2D1_POINT_2F image_point_under_pointer = D2D1::Point2F(
        image_view_center_.x + (pointer.x - viewport_center.x) / old_scale,
        image_view_center_.y + (pointer.y - viewport_center.y) / old_scale);

    const float new_scale = fit_scale * new_zoom;
    image_view_center_ = D2D1::Point2F(
        image_point_under_pointer.x - (pointer.x - viewport_center.x) / new_scale,
        image_point_under_pointer.y - (pointer.y - viewport_center.y) / new_scale);
    image_zoom_multiplier_ = new_zoom;

    return true;
}

bool ImgViewerController::ZoomByStep(int steps, D2D1_SIZE_U viewport_size)
{
    if (steps == 0) {
        return false;
    }

    return ZoomAtPoint(
        static_cast<float>(viewport_size.width) * 0.5f,
        static_cast<float>(viewport_size.height) * 0.5f,
        static_cast<float>(steps),
        viewport_size);
}

bool ImgViewerController::FitWindow()
{
    if (!current_image_.bitmap) {
        return false;
    }

    image_view_center_ = D2D1::Point2F(
        static_cast<float>(current_image_.pixel_size.width) * 0.5f,
        static_cast<float>(current_image_.pixel_size.height) * 0.5f);
    image_zoom_multiplier_ = 1.0f;
    image_is_panning_ = false;
    image_is_rotating_ = false;
    r_key_is_down_ = false;
    r_key_started_rotation_ = false;
    return true;
}

bool ImgViewerController::ActualSize(D2D1_SIZE_U viewport_size)
{
    if (!current_image_.bitmap || viewport_size.width == 0 || viewport_size.height == 0) {
        return false;
    }

    const float fit_scale = math::FitScale(current_image_.pixel_size, viewport_size);
    if (fit_scale <= 0.0f) {
        return false;
    }

    image_view_center_ = D2D1::Point2F(
        static_cast<float>(current_image_.pixel_size.width) * 0.5f,
        static_cast<float>(current_image_.pixel_size.height) * 0.5f);
    image_zoom_multiplier_ = 1.0f / fit_scale;
    image_is_panning_ = false;
    image_is_rotating_ = false;
    r_key_is_down_ = false;
    r_key_started_rotation_ = false;
    return true;
}

bool ImgViewerController::CenterOnImagePoint(D2D1_POINT_2F point)
{
    if (!current_image_.bitmap) {
        return false;
    }

    image_view_center_ = D2D1::Point2F(
        std::clamp(point.x, 0.0f, static_cast<float>(current_image_.pixel_size.width)),
        std::clamp(point.y, 0.0f, static_cast<float>(current_image_.pixel_size.height)));
    image_is_panning_ = false;
    image_is_rotating_ = false;
    r_key_is_down_ = false;
    r_key_started_rotation_ = false;
    return true;
}

bool ImgViewerController::RotateClockwise()
{
    if (!current_image_.bitmap) {
        return false;
    }

    image_rotation_degrees_ += 90.0f;
    return true;
}

bool ImgViewerController::FlipHorizontal()
{
    if (!current_image_.bitmap) {
        return false;
    }

    image_flipped_horizontal_ = !image_flipped_horizontal_;
    return true;
}

bool ImgViewerController::FlipVertical()
{
    if (!current_image_.bitmap) {
        return false;
    }

    image_flipped_vertical_ = !image_flipped_vertical_;
    return true;
}

bool ImgViewerController::ResetView()
{
    if (!current_image_.bitmap) {
        return false;
    }

    image_view_center_ = D2D1::Point2F(
        static_cast<float>(current_image_.pixel_size.width) * 0.5f,
        static_cast<float>(current_image_.pixel_size.height) * 0.5f);
    image_zoom_multiplier_ = 1.0f;
    image_rotation_degrees_ = 0.0f;
    image_flipped_horizontal_ = false;
    image_flipped_vertical_ = false;
    image_is_panning_ = false;
    image_is_rotating_ = false;
    r_key_is_down_ = false;
    r_key_started_rotation_ = false;
    return true;
}

void ImgViewerController::SetPixelatedSampling(bool enabled)
{
    pixelated_sampling_ = enabled;
}

void ImgViewerController::CancelTransientViewGesture()
{
    image_is_panning_ = false;
    image_is_rotating_ = false;
    r_key_is_down_ = false;
    r_key_started_rotation_ = false;
    image_last_pan_point_ = D2D1_POINT_2F{};
    image_last_rotation_angle_ = 0.0f;
}

bool ImgViewerController::SampleColorAt(float x, float y, D2D1_SIZE_U viewport_size, ImgViewerColorSample* color) const
{
    if (color == nullptr || current_image_.pixel_source == nullptr) {
        return false;
    }

    D2D1_POINT_2F image_point = {};
    if (!ImagePointFromViewportPoint(x, y, viewport_size, &image_point)) {
        return false;
    }

    const int pixel_x = static_cast<int>(std::floor(image_point.x));
    const int pixel_y = static_cast<int>(std::floor(image_point.y));
    if (pixel_x < 0 ||
        pixel_y < 0 ||
        pixel_x >= static_cast<int>(current_image_.pixel_size.width) ||
        pixel_y >= static_cast<int>(current_image_.pixel_size.height)) {
        return false;
    }

    BYTE bgra[4] = {};
    const WICRect rect{
        pixel_x,
        pixel_y,
        1,
        1,
    };
    if (FAILED(current_image_.pixel_source->CopyPixels(&rect, sizeof(bgra), sizeof(bgra), bgra))) {
        return false;
    }

    *color = ImgViewerColorSample{
        .red = bgra[2],
        .green = bgra[1],
        .blue = bgra[0],
        .alpha = bgra[3],
    };
    return true;
}

HRESULT ImgViewerController::AnalyzeCurrentImage(ImagePixelAnalysis* analysis) const
{
    RETURN_HR_IF_NULL(E_POINTER, analysis);
    RETURN_HR_IF_NULL(E_UNEXPECTED, current_image_.pixel_source);
    RETURN_IF_FAILED(AnalyzeImagePixels(current_image_.pixel_source.get(), analysis));
    return S_OK;
}

bool ImgViewerController::OnActionDown(ImgViewerAction action)
{
    if (action == ImgViewerAction::RotateClockwise) {
        r_key_is_down_ = true;
        return true;
    }

    return false;
}

ImgViewerEventResult ImgViewerController::OnActionUp(ImgViewerAction action)
{
    if (action == ImgViewerAction::RotateClockwise) {
        const bool should_rotate_clockwise = r_key_is_down_ && !r_key_started_rotation_;
        const bool should_release_capture = image_is_rotating_;
        r_key_is_down_ = false;
        image_is_rotating_ = false;
        r_key_started_rotation_ = false;
        if (should_rotate_clockwise && RotateClockwise()) {
            return ImgViewerEventResult{
                .handled = true,
                .needs_render = true,
            };
        }

        return ImgViewerEventResult{
            .handled = should_release_capture || should_rotate_clockwise,
            .released_capture = should_release_capture,
        };
    }

    return {};
}

float ImgViewerController::CurrentImageScale(D2D1_SIZE_U viewport_size) const
{
    if (!current_image_.bitmap) {
        return 0.0f;
    }

    return math::FitScale(current_image_.pixel_size, viewport_size) * image_zoom_multiplier_;
}

bool ImgViewerController::ImagePointFromViewportPoint(
    float x,
    float y,
    D2D1_SIZE_U viewport_size,
    D2D1_POINT_2F* image_point) const
{
    if (image_point == nullptr || !current_image_.bitmap) {
        return false;
    }

    const float image_scale = CurrentImageScale(viewport_size);
    if (image_scale <= 0.0f) {
        return false;
    }

    const D2D1_POINT_2F viewport_center = D2D1::Point2F(
        static_cast<float>(viewport_size.width) * 0.5f,
        static_cast<float>(viewport_size.height) * 0.5f);
    const D2D1_POINT_2F screen_delta = D2D1::Point2F(x - viewport_center.x, y - viewport_center.y);
    const float flip_x = image_flipped_horizontal_ ? -1.0f : 1.0f;
    const float flip_y = image_flipped_vertical_ ? -1.0f : 1.0f;
    const D2D1_POINT_2F image_delta = math::TransformVector(
        D2D1::Matrix3x2F::Rotation(-image_rotation_degrees_) *
            D2D1::Matrix3x2F::Scale(flip_x, flip_y),
        screen_delta);
    *image_point = D2D1::Point2F(
        image_view_center_.x + image_delta.x / image_scale,
        image_view_center_.y + image_delta.y / image_scale);
    return true;
}
