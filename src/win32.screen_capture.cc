#include "win32.screen_capture.hpp"

#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result_macros.h>

#include "image.bitmap.hpp"

namespace win32 {
namespace {

constexpr wchar_t kRegionSelectorClassName[] = L"ImgViewerRegionSelectorWindow";

struct RegionSelectionContext final {
    RECT virtual_rect = {};
    RECT selected_rect = {};
    POINT drag_start = {};
    POINT drag_current = {};
    bool dragging = false;
    bool completed = false;
};

RECT NormalizeRect(POINT first, POINT second)
{
    return RECT{
        (std::min)(first.x, second.x),
        (std::min)(first.y, second.y),
        (std::max)(first.x, second.x),
        (std::max)(first.y, second.y),
    };
}

bool IsEmptyRegion(const RECT& rect)
{
    return rect.right <= rect.left || rect.bottom <= rect.top;
}

void DrawSelectionOverlay(HWND hwnd, HDC hdc, const RegionSelectionContext* context)
{
    RECT client_rect = {};
    GetClientRect(hwnd, &client_rect);
    HBRUSH dim_brush = CreateSolidBrush(RGB(0, 0, 0));
    FillRect(hdc, &client_rect, dim_brush);
    DeleteObject(dim_brush);

    if (context == nullptr || !context->dragging) {
        return;
    }

    RECT selection = NormalizeRect(context->drag_start, context->drag_current);
    OffsetRect(&selection, -context->virtual_rect.left, -context->virtual_rect.top);

    HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
    HGDIOBJ previous_pen = SelectObject(hdc, pen);
    HGDIOBJ previous_brush = SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(hdc, selection.left, selection.top, selection.right, selection.bottom);
    SelectObject(hdc, previous_brush);
    SelectObject(hdc, previous_pen);
    DeleteObject(pen);
}

LRESULT CALLBACK RegionSelectorWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto* context = reinterpret_cast<RegionSelectionContext*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message) {
    case WM_NCCREATE: {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
        return TRUE;
    }
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, IDC_CROSS));
        return TRUE;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) {
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        if (context != nullptr) {
            context->dragging = true;
            context->drag_start = POINT{
                GET_X_LPARAM(lparam) + context->virtual_rect.left,
                GET_Y_LPARAM(lparam) + context->virtual_rect.top,
            };
            context->drag_current = context->drag_start;
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (context != nullptr && context->dragging) {
            context->drag_current = POINT{
                GET_X_LPARAM(lparam) + context->virtual_rect.left,
                GET_Y_LPARAM(lparam) + context->virtual_rect.top,
            };
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (context != nullptr && context->dragging) {
            context->dragging = false;
            context->drag_current = POINT{
                GET_X_LPARAM(lparam) + context->virtual_rect.left,
                GET_Y_LPARAM(lparam) + context->virtual_rect.top,
            };
            context->selected_rect = NormalizeRect(context->drag_start, context->drag_current);
            context->completed = !IsEmptyRegion(context->selected_rect);
            ReleaseCapture();
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint = {};
        HDC hdc = BeginPaint(hwnd, &paint);
        DrawSelectionOverlay(hwnd, hdc, context);
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wparam, lparam);
}

HRESULT RegisterRegionSelectorClass(HINSTANCE instance)
{
    WNDCLASSW window_class = {};
    window_class.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    window_class.hInstance = instance;
    window_class.lpfnWndProc = RegionSelectorWindowProc;
    window_class.lpszClassName = kRegionSelectorClassName;

    if (RegisterClassW(&window_class) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
        return S_OK;
    }
    RETURN_LAST_ERROR();
}

} // namespace

HRESULT SelectCaptureRegion(HWND owner, RECT* region)
{
    RETURN_HR_IF_NULL(E_POINTER, region);
    *region = {};

    RegionSelectionContext context;
    context.virtual_rect = RECT{
        GetSystemMetrics(SM_XVIRTUALSCREEN),
        GetSystemMetrics(SM_YVIRTUALSCREEN),
        GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN),
        GetSystemMetrics(SM_YVIRTUALSCREEN) + GetSystemMetrics(SM_CYVIRTUALSCREEN),
    };
    RETURN_HR_IF(E_FAIL, IsEmptyRegion(context.virtual_rect));

    HINSTANCE instance = GetModuleHandleW(nullptr);
    RETURN_LAST_ERROR_IF_NULL(instance);
    RETURN_IF_FAILED(RegisterRegionSelectorClass(instance));

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
        kRegionSelectorClassName,
        L"",
        WS_POPUP,
        context.virtual_rect.left,
        context.virtual_rect.top,
        context.virtual_rect.right - context.virtual_rect.left,
        context.virtual_rect.bottom - context.virtual_rect.top,
        owner,
        nullptr,
        instance,
        &context);
    RETURN_LAST_ERROR_IF_NULL(hwnd);

    SetLayeredWindowAttributes(hwnd, 0, 96, LWA_ALPHA);
    ShowWindow(hwnd, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    MSG message = {};
    while (IsWindow(hwnd)) {
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result == -1) {
            RETURN_LAST_ERROR();
        }
        if (result == 0) {
            break;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_CANCELLED), !context.completed);
    *region = context.selected_rect;
    return S_OK;
}

HRESULT CaptureScreenRect(IWICImagingFactory2* wic_factory, const RECT& region, IWICBitmapSource** source)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, wic_factory);
    RETURN_HR_IF_NULL(E_POINTER, source);
    *source = nullptr;

    const int capture_width = region.right - region.left;
    const int capture_height = region.bottom - region.top;
    RETURN_HR_IF(E_INVALIDARG, capture_width <= 0 || capture_height <= 0);

    const uint64_t pixel_count = static_cast<uint64_t>(capture_width) * static_cast<uint64_t>(capture_height);
    RETURN_HR_IF(E_OUTOFMEMORY, pixel_count > (std::numeric_limits<size_t>::max)() / 4);

    HDC screen_dc = GetDC(nullptr);
    RETURN_LAST_ERROR_IF_NULL(screen_dc);
    auto release_screen_dc = wil::scope_exit([screen_dc] { ReleaseDC(nullptr, screen_dc); });

    wil::unique_hdc memory_dc{CreateCompatibleDC(screen_dc)};
    RETURN_LAST_ERROR_IF_NULL(memory_dc);

    BITMAPINFO bitmap_info = {};
    bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
    bitmap_info.bmiHeader.biWidth = capture_width;
    bitmap_info.bmiHeader.biHeight = -capture_height;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = 32;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    wil::unique_hbitmap bitmap{CreateDIBSection(
        screen_dc,
        &bitmap_info,
        DIB_RGB_COLORS,
        &bits,
        nullptr,
        0)};
    RETURN_LAST_ERROR_IF_NULL(bitmap);
    RETURN_HR_IF_NULL(E_OUTOFMEMORY, bits);

    HGDIOBJ previous_bitmap = SelectObject(memory_dc.get(), bitmap.get());
    RETURN_LAST_ERROR_IF(previous_bitmap == nullptr || previous_bitmap == HGDI_ERROR);
    auto restore_bitmap = wil::scope_exit([dc = memory_dc.get(), previous_bitmap] { SelectObject(dc, previous_bitmap); });

    RETURN_IF_WIN32_BOOL_FALSE(BitBlt(
        memory_dc.get(),
        0,
        0,
        capture_width,
        capture_height,
        screen_dc,
        region.left,
        region.top,
        SRCCOPY | CAPTUREBLT));

    std::vector<BYTE> bgra(static_cast<size_t>(pixel_count) * 4);
    memcpy(bgra.data(), bits, bgra.size());
    for (size_t index = 3; index < bgra.size(); index += 4) {
        bgra[index] = 0xFF;
    }

    RETURN_IF_FAILED(image_bitmap::CreateBitmapSourceFromBgra(
        wic_factory,
        static_cast<UINT>(capture_width),
        static_cast<UINT>(capture_height),
        bgra,
        source));
    return S_OK;
}

} // namespace win32
