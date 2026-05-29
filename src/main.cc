#include "main.hpp"
#include "app.messages.hpp"
#include "coordinates.hpp"
#include "renderer.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include <shellapi.h>
#include <shobjidl.h>
#include <string>
#include <vector>

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

void LoadImageFile(HWND hwnd, Renderer* renderer, const wchar_t* path)
{
    if (renderer == nullptr || path == nullptr || path[0] == L'\0') {
        return;
    }

    const HRESULT hr = renderer->LoadImageFile(path);
    if (FAILED(hr)) {
        MessageBoxW(hwnd, L"Could not open the selected image.", kWindowTitle, MB_OK | MB_ICONERROR);
    }
}

HRESULT PickImageFile(HWND hwnd, std::wstring* path)
{
    RETURN_HR_IF_NULL(E_POINTER, path);

    wil::com_ptr<IFileOpenDialog> dialog;
    RETURN_IF_FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.put())));

    constexpr COMDLG_FILTERSPEC filters[] = {
        {L"Images", L"*.bmp;*.dib;*.gif;*.ico;*.jpg;*.jpeg;*.jpe;*.png;*.tif;*.tiff;*.webp"},
        {L"All files", L"*.*"},
    };
    RETURN_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(filters), filters));
    RETURN_IF_FAILED(dialog->SetFileTypeIndex(1));

    const HRESULT show_result = dialog->Show(hwnd);
    if (show_result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        return show_result;
    }
    RETURN_IF_FAILED(show_result);

    wil::com_ptr<IShellItem> item;
    RETURN_IF_FAILED(dialog->GetResult(item.put()));

    wil::unique_cotaskmem_string file_path;
    RETURN_IF_FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, file_path.put()));
    *path = file_path.get();
    return S_OK;
}

void HandleOpenImageCommand(HWND hwnd, Renderer* renderer)
{
    std::wstring path;
    const HRESULT hr = PickImageFile(hwnd, &path);
    if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        return;
    }

    if (FAILED(hr)) {
        MessageBoxW(hwnd, L"Could not show the image picker.", kWindowTitle, MB_OK | MB_ICONERROR);
        return;
    }

    LoadImageFile(hwnd, renderer, path.c_str());
}

void RenderIfNeeded(HWND hwnd, Renderer* renderer, UiEventResult result)
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

    if (result.command == UiCommand::OpenImage) {
        HandleOpenImageCommand(hwnd, renderer);
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
    case kImgViewerOpenImageMessage: {
        HandleOpenImageCommand(hwnd, GetRenderer(hwnd));
        return 0;
    }

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

    case WM_ERASEBKGND:
        return 1;

    case WM_MOUSEMOVE: {
        Renderer* renderer = GetRenderer(hwnd);
        const D2D1_POINT_2F point = GetPointerPoint(hwnd, lparam);
        TrackMouseLeave(hwnd);
        RenderIfNeeded(hwnd, renderer, renderer != nullptr ? renderer->OnPointerMove(point.x, point.y) : UiEventResult{});
        return 0;
    }

    case WM_LBUTTONDOWN: {
        Renderer* renderer = GetRenderer(hwnd);
        const D2D1_POINT_2F point = GetPointerPoint(hwnd, lparam);
        const UiEventResult result = renderer != nullptr ? renderer->OnPointerDown(point.x, point.y) : UiEventResult{};
        if (result.captured) {
            SetCapture(hwnd);
        }
        RenderIfNeeded(hwnd, renderer, result);
        return result.handled ? 0 : DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_LBUTTONUP: {
        Renderer* renderer = GetRenderer(hwnd);
        const D2D1_POINT_2F point = GetPointerPoint(hwnd, lparam);
        const UiEventResult result = renderer != nullptr ? renderer->OnPointerUp(point.x, point.y) : UiEventResult{};
        RenderIfNeeded(hwnd, renderer, result);
        return result.handled ? 0 : DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_MOUSELEAVE: {
        Renderer* renderer = GetRenderer(hwnd);
        RenderIfNeeded(hwnd, renderer, renderer != nullptr ? renderer->OnPointerLeave() : UiEventResult{});
        return 0;
    }

    case WM_DROPFILES: {
        Renderer* renderer = GetRenderer(hwnd);
        const HDROP drop = reinterpret_cast<HDROP>(wparam);
        const UINT length = DragQueryFileW(drop, 0, nullptr, 0);
        if (length > 0) {
            std::vector<wchar_t> path(static_cast<size_t>(length) + 1);
            DragQueryFileW(drop, 0, path.data(), static_cast<UINT>(path.size()));
            LoadImageFile(hwnd, renderer, path.data());
        }
        DragFinish(drop);
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
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = kWindowClassName;

    const ATOM window_class_atom = RegisterClassExW(&window_class);
    RETURN_LAST_ERROR_IF(window_class_atom == 0);

    return S_OK;
}

HRESULT RunApplicationAsHresult()
{
    RETURN_IF_FAILED(InitializeDpiAwareness());
    const HRESULT co_initialize_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    RETURN_IF_FAILED(co_initialize_result);
    auto co_uninitialize = wil::scope_exit([] { CoUninitialize(); });

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
    DragAcceptFiles(window.get(), TRUE);

    ShowWindow(window.get(), SW_SHOWDEFAULT);
    RETURN_IF_WIN32_BOOL_FALSE(UpdateWindow(window.get()));

    int argc = 0;
    wil::unique_hlocal command_line_args{reinterpret_cast<HLOCAL>(CommandLineToArgvW(GetCommandLineW(), &argc))};
    RETURN_LAST_ERROR_IF_NULL(command_line_args.get());
    auto** argv = reinterpret_cast<wchar_t**>(command_line_args.get());
    if (argc > 1) {
        LoadImageFile(window.get(), &renderer, argv[1]);
    }

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
