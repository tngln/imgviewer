#pragma once

#include <algorithm>

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

    constexpr float PhysicalToDip(float value) const { return value / scale_; }
    constexpr float DipToPhysical(float value) const { return value * scale_; }
    constexpr float PhysicalToRender(float value) const { return value; }

    constexpr D2D1_POINT_2F PhysicalToDip(POINT point) const
    {
        return D2D1_POINT_2F{
            PhysicalToDip(static_cast<float>(point.x)),
            PhysicalToDip(static_cast<float>(point.y)),
        };
    }

    constexpr D2D1_POINT_2F PhysicalToRender(POINT point) const
    {
        return D2D1_POINT_2F{
            PhysicalToRender(static_cast<float>(point.x)),
            PhysicalToRender(static_cast<float>(point.y)),
        };
    }

    constexpr D2D1_SIZE_F PhysicalToRender(UINT width, UINT height) const
    {
        return D2D1_SIZE_F{
            PhysicalToRender(static_cast<float>(width)),
            PhysicalToRender(static_cast<float>(height)),
        };
    }

    constexpr D2D1_SIZE_F PhysicalToDip(UINT width, UINT height) const
    {
        return D2D1_SIZE_F{
            PhysicalToDip(static_cast<float>(width)),
            PhysicalToDip(static_cast<float>(height)),
        };
    }

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

} // namespace math
