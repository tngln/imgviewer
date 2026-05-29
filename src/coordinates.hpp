#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <d2d1_1.h>

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
