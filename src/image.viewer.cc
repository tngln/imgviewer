#include "image.viewer.hpp"

#include <algorithm>
#include <cmath>

#include <d2d1helper.h>
#include <wil/result_macros.h>

namespace {

constexpr float kMinImageZoomMultiplier = 0.05f;
constexpr float kMaxImageZoomMultiplier = 64.0f;
constexpr float kWheelZoomStep = 1.12f;
constexpr float kRadiansToDegrees = 57.2957795f;

float AngleFromCenter(D2D1_POINT_2F point, D2D1_POINT_2F center)
{
    return std::atan2(point.y - center.y, point.x - center.x);
}

D2D1_POINT_2F TransformVector(D2D1_MATRIX_3X2_F matrix, D2D1_POINT_2F vector)
{
    return D2D1::Point2F(
        vector.x * matrix._11 + vector.y * matrix._21,
        vector.x * matrix._12 + vector.y * matrix._22);
}

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
        const float angle = AngleFromCenter(point, viewport_center);
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
            TransformVector(D2D1::Matrix3x2F::Rotation(-image_rotation_degrees_), screen_delta);
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
        const D2D1_POINT_2F viewport_center = D2D1::Point2F(
            static_cast<float>(viewport_size.width) * 0.5f,
            static_cast<float>(viewport_size.height) * 0.5f);
        image_last_rotation_angle_ = AngleFromCenter(point, viewport_center);
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
        image_last_rotation_angle_ = AngleFromCenter(
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
    if (!current_image_.bitmap || delta == 0 || viewport_size.width == 0 || viewport_size.height == 0) {
        return false;
    }

    const float image_width = static_cast<float>(current_image_.pixel_size.width);
    const float image_height = static_cast<float>(current_image_.pixel_size.height);
    if (image_width <= 0.0f || image_height <= 0.0f) {
        return false;
    }

    const float viewport_width = static_cast<float>(viewport_size.width);
    const float viewport_height = static_cast<float>(viewport_size.height);
    const float fit_scale = CurrentImageScale(viewport_size) / image_zoom_multiplier_;
    const float old_scale = fit_scale * image_zoom_multiplier_;
    const float wheel_steps = static_cast<float>(delta) / static_cast<float>(WHEEL_DELTA);
    const float new_zoom = std::clamp(
        image_zoom_multiplier_ * std::pow(kWheelZoomStep, wheel_steps),
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

bool ImageViewerController::OnKeyDown(UINT virtual_key)
{
    if (virtual_key == 'R') {
        r_key_is_down_ = true;
        return true;
    }

    return false;
}

bool ImageViewerController::OnKeyUp(UINT virtual_key)
{
    if (virtual_key == 'R') {
        const bool was_rotating = image_is_rotating_;
        r_key_is_down_ = false;
        image_is_rotating_ = false;
        return was_rotating;
    }

    return false;
}

float ImageViewerController::CurrentImageScale(D2D1_SIZE_U viewport_size) const
{
    if (!current_image_.bitmap || current_image_.pixel_size.width == 0 || current_image_.pixel_size.height == 0) {
        return 0.0f;
    }

    const float image_width = static_cast<float>(current_image_.pixel_size.width);
    const float image_height = static_cast<float>(current_image_.pixel_size.height);
    const float available_width = (std::max)(1.0f, static_cast<float>(viewport_size.width));
    const float available_height = (std::max)(1.0f, static_cast<float>(viewport_size.height));
    return (std::min)(available_width / image_width, available_height / image_height) * image_zoom_multiplier_;
}
