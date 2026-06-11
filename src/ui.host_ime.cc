#include "ui.host_ime.hpp"

#include <cmath>
#include <imm.h>

std::wstring ReadImeCompositionString(HWND hwnd, LPARAM lparam, DWORD string_type)
{
    if ((lparam & string_type) == 0) {
        return {};
    }

    HIMC ime = ImmGetContext(hwnd);
    if (ime == nullptr) {
        return {};
    }

    const LONG bytes = ImmGetCompositionStringW(ime, string_type, nullptr, 0);
    std::wstring text(bytes > 0 ? static_cast<size_t>(bytes) / sizeof(wchar_t) : 0, L'\0');
    if (!text.empty()) {
        ImmGetCompositionStringW(ime, string_type, text.data(), bytes);
    }
    ImmReleaseContext(hwnd, ime);
    return text;
}

void SetImeCompositionWindowClientPoint(HWND hwnd, POINT point)
{
    HIMC ime = ImmGetContext(hwnd);
    if (ime == nullptr) {
        return;
    }

    COMPOSITIONFORM form = {};
    form.dwStyle = CFS_POINT;
    form.ptCurrentPos = point;
    ImmSetCompositionWindow(ime, &form);
    ImmReleaseContext(hwnd, ime);
}

void SetImeCompositionWindowClientPoint(HWND hwnd, D2D1_POINT_2F point)
{
    SetImeCompositionWindowClientPoint(
        hwnd,
        POINT{
            static_cast<LONG>(std::floor(point.x)),
            static_cast<LONG>(std::floor(point.y)),
        });
}
