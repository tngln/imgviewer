#include "main.hpp"
#include "coordinates.hpp"
#include "renderer.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include <wil/resource.h>
#include <wil/result_macros.h>

namespace {

constexpr wchar_t kWindowClassName[] = L"ImgViewerWindow";
constexpr wchar_t kWindowTitle[] = L"ImgViewer";

D2D1_POINT_2F GetPointerPoint(HWND hwnd, LPARAM lparam)
{
    const POINT point{
        GET_X_LPARAM(lparam),
        GET_Y_LPARAM(lparam),
    };
    return CoordinateSpace::FromWindow(hwnd).PhysicalToRender(point);
}

Renderer* GetRenderer(HWND hwnd)
{
    return reinterpret_cast<Renderer*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

void RenderIfNeeded(Renderer* renderer, UiEventResult result)
{
    if (renderer == nullptr) {
        return;
    }

    if (result.released_capture) {
        ReleaseCapture();
    }

    if (result.needs_render) {
        renderer->Render();
    }
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

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_NCCREATE: {
        const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_CREATE: {
        Renderer* renderer = GetRenderer(hwnd);
        if (renderer == nullptr || FAILED(renderer->Initialize(hwnd))) {
            return -1;
        }

        return 0;
    }

    case WM_SIZE: {
        Renderer* renderer = GetRenderer(hwnd);
        if (renderer != nullptr && FAILED(renderer->Resize())) {
            return -1;
        }

        return 0;
    }

    case WM_MOUSEMOVE: {
        Renderer* renderer = GetRenderer(hwnd);
        const D2D1_POINT_2F point = GetPointerPoint(hwnd, lparam);
        TrackMouseLeave(hwnd);
        RenderIfNeeded(renderer, renderer != nullptr ? renderer->OnPointerMove(point.x, point.y) : UiEventResult{});
        return 0;
    }

    case WM_LBUTTONDOWN: {
        Renderer* renderer = GetRenderer(hwnd);
        const D2D1_POINT_2F point = GetPointerPoint(hwnd, lparam);
        const UiEventResult result = renderer != nullptr ? renderer->OnPointerDown(point.x, point.y) : UiEventResult{};
        if (result.captured) {
            SetCapture(hwnd);
        }
        RenderIfNeeded(renderer, result);
        return result.handled ? 0 : DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_LBUTTONUP: {
        Renderer* renderer = GetRenderer(hwnd);
        const D2D1_POINT_2F point = GetPointerPoint(hwnd, lparam);
        const UiEventResult result = renderer != nullptr ? renderer->OnPointerUp(point.x, point.y) : UiEventResult{};
        RenderIfNeeded(renderer, result);
        return result.handled ? 0 : DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_MOUSELEAVE: {
        Renderer* renderer = GetRenderer(hwnd);
        RenderIfNeeded(renderer, renderer != nullptr ? renderer->OnPointerLeave() : UiEventResult{});
        return 0;
    }

    case WM_GETOBJECT: {
        if (lparam == UiaRootObjectId) {
            Renderer* renderer = GetRenderer(hwnd);
            if (renderer != nullptr) {
                return UiaReturnRawElementProvider(hwnd, wparam, lparam, renderer->GetAccessibilityProvider());
            }
        }

        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

HRESULT RegisterMainWindowClass(HINSTANCE instance)
{
    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH));
    window_class.lpszClassName = kWindowClassName;

    const ATOM window_class_atom = RegisterClassExW(&window_class);
    RETURN_LAST_ERROR_IF(window_class_atom == 0);

    return S_OK;
}

HRESULT RunApplicationAsHresult()
{
    RETURN_IF_FAILED(InitializeDpiAwareness());

    HINSTANCE instance = GetModuleHandleW(nullptr);
    RETURN_LAST_ERROR_IF_NULL(instance);

    RETURN_IF_FAILED(RegisterMainWindowClass(instance));

    Renderer renderer;
    wil::unique_hwnd window{CreateWindowExW(
        0,
        kWindowClassName,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        960,
        640,
        nullptr,
        nullptr,
        instance,
        &renderer)};
    RETURN_LAST_ERROR_IF_NULL(window.get());

    ShowWindow(window.get(), SW_SHOWDEFAULT);
    RETURN_IF_WIN32_BOOL_FALSE(UpdateWindow(window.get()));

    MSG message = {};
    while (true) {
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

    return S_OK;
}

} // namespace

int RunApplication()
{
    const HRESULT hr = RunApplicationAsHresult();
    return SUCCEEDED(hr) ? 0 : 1;
}

int main()
{
    return RunApplication();
}
