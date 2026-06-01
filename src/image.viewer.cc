#include "image.viewer.hpp"

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

HRESULT ImageViewerController::Initialize()
{
    RETURN_IF_FAILED(image_decoder_.Initialize());
    return S_OK;
}

HRESULT ImageViewerController::LoadImageFile(const wchar_t* path, ID2D1DeviceContext* d2d_context)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, path);
    RETURN_HR_IF_NULL(E_INVALIDARG, d2d_context);

    DecodedImage image;
    RETURN_IF_FAILED(image_decoder_.DecodeFirstFrame(path, d2d_context, &image));

    current_image_ = std::move(image);
    image_view_center_ = D2D1::Point2F(
        static_cast<float>(current_image_.pixel_size.width) * 0.5f,
        static_cast<float>(current_image_.pixel_size.height) * 0.5f);
    image_zoom_multiplier_ = 1.0f;
    image_rotation_degrees_ = 0.0f;
    image_is_panning_ = false;
    image_is_rotating_ = false;
    r_key_is_down_ = false;
    r_key_started_rotation_ = false;
    return S_OK;
}

D2D1_SIZE_U ImageViewerController::CurrentImagePixelSize() const
{
    return current_image_.pixel_size;
}

ImageViewerSnapshot ImageViewerController::Snapshot() const
{
    return ImageViewerSnapshot{
        .bitmap = current_image_.bitmap.get(),
        .pixel_size = current_image_.pixel_size,
        .view_center = image_view_center_,
        .zoom_multiplier = image_zoom_multiplier_,
        .rotation_degrees = image_rotation_degrees_,
    };
}

ImageViewerEventResult ImageViewerController::OnPointerMove(float x, float y, D2D1_SIZE_U viewport_size)
{
    if (image_is_rotating_) {
        const D2D1_POINT_2F point = D2D1::Point2F(x, y);
        const D2D1_POINT_2F viewport_center = D2D1::Point2F(
            static_cast<float>(viewport_size.width) * 0.5f,
            static_cast<float>(viewport_size.height) * 0.5f);
        const float angle = math::AngleFromCenter(point, viewport_center);
        image_rotation_degrees_ += (angle - image_last_rotation_angle_) * kRadiansToDegrees;
        image_last_rotation_angle_ = angle;
        return ImageViewerEventResult{
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
        const D2D1_POINT_2F image_delta =
            math::TransformVector(D2D1::Matrix3x2F::Rotation(-image_rotation_degrees_), screen_delta);
        image_view_center_.x -= image_delta.x / image_scale;
        image_view_center_.y -= image_delta.y / image_scale;
        image_last_pan_point_ = point;
        return ImageViewerEventResult{
            .handled = true,
            .needs_render = true,
        };
    }

    return {};
}

ImageViewerEventResult ImageViewerController::OnPointerDown(float x, float y, D2D1_SIZE_U viewport_size)
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
        return ImageViewerEventResult{
            .handled = true,
            .captured = true,
        };
    }

    image_is_panning_ = true;
    image_last_pan_point_ = point;
    return ImageViewerEventResult{
        .handled = true,
        .captured = true,
    };
}

ImageViewerEventResult ImageViewerController::OnPointerUp(float x, float y, D2D1_SIZE_U viewport_size)
{
    if (image_is_rotating_) {
        image_is_rotating_ = false;
        image_last_rotation_angle_ = math::AngleFromCenter(
            D2D1::Point2F(x, y),
            D2D1::Point2F(
                static_cast<float>(viewport_size.width) * 0.5f,
                static_cast<float>(viewport_size.height) * 0.5f));
        return ImageViewerEventResult{
            .handled = true,
            .released_capture = true,
        };
    }

    if (image_is_panning_) {
        image_is_panning_ = false;
        image_last_pan_point_ = D2D1::Point2F(x, y);
        return ImageViewerEventResult{
            .handled = true,
            .released_capture = true,
        };
    }

    return {};
}

bool ImageViewerController::OnMouseWheel(float x, float y, int delta, D2D1_SIZE_U viewport_size)
{
    if (delta == 0) {
        return false;
    }

    return ZoomAtPoint(x, y, static_cast<float>(delta) / static_cast<float>(WHEEL_DELTA), viewport_size);
}

bool ImageViewerController::ZoomAtPoint(float x, float y, float steps, D2D1_SIZE_U viewport_size)
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

bool ImageViewerController::ZoomByStep(int steps, D2D1_SIZE_U viewport_size)
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

bool ImageViewerController::RotateClockwise()
{
    if (!current_image_.bitmap) {
        return false;
    }

    image_rotation_degrees_ += 90.0f;
    return true;
}

bool ImageViewerController::ResetView()
{
    if (!current_image_.bitmap) {
        return false;
    }

    image_view_center_ = D2D1::Point2F(
        static_cast<float>(current_image_.pixel_size.width) * 0.5f,
        static_cast<float>(current_image_.pixel_size.height) * 0.5f);
    image_zoom_multiplier_ = 1.0f;
    image_rotation_degrees_ = 0.0f;
    image_is_panning_ = false;
    image_is_rotating_ = false;
    r_key_is_down_ = false;
    r_key_started_rotation_ = false;
    return true;
}

bool ImageViewerController::OnKeyDown(UINT virtual_key)
{
    if (virtual_key == 'R') {
        r_key_is_down_ = true;
        return true;
    }

    return false;
}

ImageViewerEventResult ImageViewerController::OnKeyUp(UINT virtual_key)
{
    if (virtual_key == 'R') {
        const bool should_rotate_clockwise = r_key_is_down_ && !r_key_started_rotation_;
        const bool should_release_capture = image_is_rotating_;
        r_key_is_down_ = false;
        image_is_rotating_ = false;
        r_key_started_rotation_ = false;
        if (should_rotate_clockwise && RotateClockwise()) {
            return ImageViewerEventResult{
                .handled = true,
                .needs_render = true,
            };
        }

        return ImageViewerEventResult{
            .handled = should_release_capture || should_rotate_clockwise,
            .released_capture = should_release_capture,
        };
    }

    return {};
}

float ImageViewerController::CurrentImageScale(D2D1_SIZE_U viewport_size) const
{
    if (!current_image_.bitmap) {
        return 0.0f;
    }

    return math::FitScale(current_image_.pixel_size, viewport_size) * image_zoom_multiplier_;
}
