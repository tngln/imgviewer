#pragma once

#include <algorithm>
#include <cmath>

#include <windows.h>

#include <d2d1_1.h>
#include <d2d1helper.h>

namespace math {

class CoordinateSpace final {
public:
    static CoordinateSpace FromWindow(HWND hwnd)
    {
        return CoordinateSpace(GetDpiForWindow(hwnd));
    }

    explicit constexpr CoordinateSpace(UINT dpi) : dpi_(dpi), scale_(static_cast<float>(dpi) / 96.0f) {}

    constexpr UINT dpi() const { return dpi_; }
    constexpr float scale() const { return scale_; }

private:
    UINT dpi_ = 96;
    float scale_ = 1.0f;
};

inline float RectWidth(D2D1_RECT_F rect)
{
    return rect.right - rect.left;
}

inline float RectHeight(D2D1_RECT_F rect)
{
    return rect.bottom - rect.top;
}

inline D2D1_RECT_F Inset(D2D1_RECT_F rect, float left, float top, float right, float bottom);

inline D2D1_RECT_F Inset(D2D1_RECT_F rect, float all)
{
    return Inset(rect, all, all, all, all);
}

inline D2D1_RECT_F Inset(D2D1_RECT_F rect, float left, float top, float right, float bottom)
{
    return D2D1::RectF(rect.left + left, rect.top + top, rect.right - right, rect.bottom - bottom);
}

inline D2D1_RECT_F StableRect(float left, float top, float right, float bottom)
{
    return D2D1::RectF(left, top, (std::max)(left + 1.0f, right), (std::max)(top + 1.0f, bottom));
}

inline bool Contains(D2D1_RECT_F rect, D2D1_POINT_2F point)
{
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}

inline float FitScale(D2D1_SIZE_U source_size, D2D1_SIZE_U viewport_size)
{
    if (source_size.width == 0 || source_size.height == 0) {
        return 0.0f;
    }

    const float source_width = static_cast<float>(source_size.width);
    const float source_height = static_cast<float>(source_size.height);
    const float available_width = (std::max)(1.0f, static_cast<float>(viewport_size.width));
    const float available_height = (std::max)(1.0f, static_cast<float>(viewport_size.height));
    return (std::min)(available_width / source_width, available_height / source_height);
}

inline float AngleFromCenter(D2D1_POINT_2F point, D2D1_POINT_2F center)
{
    return std::atan2(point.y - center.y, point.x - center.x);
}

inline D2D1_POINT_2F TransformVector(D2D1_MATRIX_3X2_F matrix, D2D1_POINT_2F vector)
{
    return D2D1::Point2F(
        vector.x * matrix._11 + vector.y * matrix._21,
        vector.x * matrix._12 + vector.y * matrix._22);
}

} // namespace math
