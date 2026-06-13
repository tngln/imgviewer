#include "ui.host_input.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>

#include <d2d1helper.h>

#include "math.hpp"

namespace ui_host_input {

D2D1_SIZE_U ClientPixelSize(HWND hwnd)
{
    RECT rect = {};
    GetClientRect(hwnd, &rect);
    return D2D1::SizeU(
        static_cast<UINT32>((std::max)(1L, rect.right - rect.left)),
        static_cast<UINT32>((std::max)(1L, rect.bottom - rect.top)));
}

D2D1_SIZE_F ClientRenderSize(HWND hwnd)
{
    const D2D1_SIZE_U size = ClientPixelSize(hwnd);
    return D2D1::SizeF(static_cast<float>(size.width), static_cast<float>(size.height));
}

float DpiScale(HWND hwnd)
{
    return math::CoordinateSpace::FromWindow(hwnd).scale();
}

D2D1_POINT_2F PhysicalClientPointToUi(HWND hwnd, POINT point)
{
    const float dpi_scale = DpiScale(hwnd);
    return D2D1::Point2F(
        static_cast<float>(point.x) / dpi_scale,
        static_cast<float>(point.y) / dpi_scale);
}

D2D1_POINT_2F PhysicalClientPointToUi(HWND hwnd, LPARAM lparam)
{
    return PhysicalClientPointToUi(hwnd, POINT{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
}

D2D1_POINT_2F ScreenPointToUi(HWND hwnd, POINT point)
{
    ScreenToClient(hwnd, &point);
    return PhysicalClientPointToUi(hwnd, point);
}

POINT UiPointToPhysicalClient(HWND hwnd, D2D1_POINT_2F point)
{
    const float dpi_scale = DpiScale(hwnd);
    return POINT{
        static_cast<LONG>(std::floor(point.x * dpi_scale)),
        static_cast<LONG>(std::floor(point.y * dpi_scale)),
    };
}

} // namespace ui_host_input
