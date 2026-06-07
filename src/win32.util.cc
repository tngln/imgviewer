#include "win32.util.hpp"

#include <dwmapi.h>
#include <imm.h>
#include <shlobj_core.h>

#include <string>

#include <wil/result_macros.h>
#include <wil/resource.h>

namespace util {

bool IsKeyDown(int virtual_key)
{
    return (GetKeyState(virtual_key) & 0x8000) != 0;
}

bool IsWindowTopMost(HWND hwnd)
{
    return (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
}

int ResizeBorderThicknessForDpi(UINT dpi)
{
    return GetSystemMetricsForDpi(SM_CXFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
}

std::wstring FileNameFromPath(const wchar_t* path, const wchar_t* fallback)
{
    if (path == nullptr) {
        return fallback;
    }

    std::wstring value(path);
    const size_t separator = value.find_last_of(L"\\/");
    if (separator != std::wstring::npos) {
        value.erase(0, separator + 1);
    }

    return value.empty() ? std::wstring(fallback) : value;
}

HRESULT ApplyDwmFrame(HWND hwnd, bool borderless)
{
    DWMNCRENDERINGPOLICY policy = borderless ? DWMNCRP_DISABLED : DWMNCRP_ENABLED;
    RETURN_IF_FAILED(DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy)));

    const MARGINS margins = borderless ? MARGINS{0, 0, 0, 0} : MARGINS{0, 0, 0, 1};
    RETURN_IF_FAILED(DwmExtendFrameIntoClientArea(hwnd, &margins));
    return S_OK;
}

HRESULT ShowFileInExplorer(const wchar_t* path)
{
    RETURN_HR_IF(E_INVALIDARG, path == nullptr || path[0] == L'\0');

    PIDLIST_ABSOLUTE item = nullptr;
    RETURN_IF_FAILED(SHParseDisplayName(path, nullptr, &item, 0, nullptr));
    auto free_item = wil::scope_exit([&] {
        CoTaskMemFree(item);
    });

    RETURN_IF_FAILED(SHOpenFolderAndSelectItems(item, 0, nullptr, 0));
    return S_OK;
}

void DisableIme(HWND hwnd)
{
    ImmAssociateContext(hwnd, nullptr);
}

void TrackMouseLeave(HWND hwnd)
{
    TRACKMOUSEEVENT track_event = {};
    track_event.cbSize = sizeof(track_event);
    track_event.dwFlags = TME_LEAVE;
    track_event.hwndTrack = hwnd;
    TrackMouseEvent(&track_event);
}

HRESULT InitializeDpiAwareness()
{
    if (SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        return S_OK;
    }

    const DWORD error = GetLastError();
    if (error == ERROR_ACCESS_DENIED) {
        return S_OK;
    }

    RETURN_IF_FAILED(HRESULT_FROM_WIN32(error));
    return S_OK;
}

} // namespace util

bool util::CaptureWindowSize(HWND hwnd, int* width, int* height)
{
    if (hwnd == nullptr || width == nullptr || height == nullptr || IsIconic(hwnd) || IsZoomed(hwnd)) {
        return false;
    }

    RECT window_rect = {};
    if (!GetWindowRect(hwnd, &window_rect)) {
        return false;
    }

    *width = static_cast<int>(window_rect.right - window_rect.left);
    *height = static_cast<int>(window_rect.bottom - window_rect.top);
    return true;
}

void util::ApplyMinTrackSize(HWND hwnd, LPARAM lparam, int min_client_width, int min_client_height)
{
    auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
    if (info == nullptr) {
        return;
    }

    RECT min_rect{0, 0, min_client_width, min_client_height};
    const DWORD style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_STYLE));
    const DWORD ex_style = static_cast<DWORD>(GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
    AdjustWindowRectEx(&min_rect, style, FALSE, ex_style);
    info->ptMinTrackSize.x = min_rect.right - min_rect.left;
    info->ptMinTrackSize.y = min_rect.bottom - min_rect.top;
}
