#include "main.hpp"
#include "app.messages.hpp"
#include "coordinates.hpp"
#include "image.viewer.hpp"
#include "renderer.hpp"
#include "ui.a11y.hpp"
#include "ui.hpp"

#include <windows.h>
#include <windowsx.h>

#include <dwmapi.h>
#include <cwchar>
#include <imm.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <string>
#include <vector>

#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result_macros.h>

namespace {

constexpr wchar_t kWindowClassName[] = L"ImgViewerWindow";
constexpr wchar_t kWindowTitle[] = L"ImgViewer";

struct AppContext final {
    Renderer renderer;
    UiController ui;
    ImageViewerController viewer;
    wil::com_ptr<IRawElementProviderSimple> accessibility_provider;
};

D2D1_POINT_2F GetPointerPoint(HWND hwnd, LPARAM lparam)
{
    const POINT point{
        GET_X_LPARAM(lparam),
        GET_Y_LPARAM(lparam),
    };
    return CoordinateSpace::FromWindow(hwnd).PhysicalToRender(point);
}

D2D1_POINT_2F GetScreenPointerPoint(HWND hwnd, LPARAM lparam)
{
    POINT point{
        GET_X_LPARAM(lparam),
        GET_Y_LPARAM(lparam),
    };
    ScreenToClient(hwnd, &point);
    return CoordinateSpace::FromWindow(hwnd).PhysicalToRender(point);
}

AppContext* GetAppContext(HWND hwnd)
{
    return reinterpret_cast<AppContext*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
}

bool IsWindowTopMost(HWND hwnd)
{
    return (GetWindowLongPtrW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
}

int ResizeBorderThicknessForDpi(UINT dpi)
{
    return GetSystemMetricsForDpi(SM_CXFRAME, dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
}

HRESULT RenderApplication(AppContext* context)
{
    if (context == nullptr) {
        return S_OK;
    }

    RETURN_IF_FAILED(context->renderer.Render(context->viewer, context->ui));
    return S_OK;
}

void SyncWindowState(HWND hwnd, UiController* ui)
{
    if (ui != nullptr) {
        ui->SetWindowState(IsWindowTopMost(hwnd), IsZoomed(hwnd));
    }
}

HRESULT ApplyDwmFrame(HWND hwnd)
{
    DWMNCRENDERINGPOLICY policy = DWMNCRP_ENABLED;
    RETURN_IF_FAILED(DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy)));

    const MARGINS margins = {0, 0, 0, 1};
    RETURN_IF_FAILED(DwmExtendFrameIntoClientArea(hwnd, &margins));
    return S_OK;
}

void DisableIme(HWND hwnd)
{
    ImmAssociateContext(hwnd, nullptr);
}

std::wstring FileNameFromPath(const wchar_t* path)
{
    if (path == nullptr) {
        return kWindowTitle;
    }

    std::wstring value(path);
    const size_t separator = value.find_last_of(L"\\/");
    if (separator != std::wstring::npos) {
        value.erase(0, separator + 1);
    }

    return value.empty() ? std::wstring(kWindowTitle) : value;
}

void ExecuteUiCommand(HWND hwnd, AppContext* context, UiCommand command)
{
    switch (command) {
    case UiCommand::OpenImage:
        break;
    case UiCommand::ToggleTopMost: {
        const bool top_most = !IsWindowTopMost(hwnd);
        SetWindowPos(
            hwnd,
            top_most ? HWND_TOPMOST : HWND_NOTOPMOST,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        if (context != nullptr) {
            SyncWindowState(hwnd, &context->ui);
            RenderApplication(context);
        }
        break;
    }
    case UiCommand::Minimize:
        ShowWindow(hwnd, SW_MINIMIZE);
        break;
    case UiCommand::ToggleMaximize:
        ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
        if (context != nullptr) {
            SyncWindowState(hwnd, &context->ui);
            RenderApplication(context);
        }
        break;
    case UiCommand::Close:
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        break;
    default:
        break;
    }
}

void LoadImageFile(HWND hwnd, AppContext* context, const wchar_t* path)
{
    if (context == nullptr || path == nullptr || path[0] == L'\0') {
        return;
    }

    const HRESULT hr = context->viewer.LoadImageFile(path, context->renderer.BitmapDeviceContext());
    if (FAILED(hr)) {
        MessageBoxW(hwnd, L"Could not open the selected image.", kWindowTitle, MB_OK | MB_ICONERROR);
        return;
    }

    const std::wstring file_name = FileNameFromPath(path);
    const D2D1_SIZE_U image_size = context->viewer.CurrentImagePixelSize();
    wchar_t resolution_text[64] = {};
    swprintf_s(resolution_text, L"  %ux%u", image_size.width, image_size.height);
    const std::wstring title_text = file_name + resolution_text;
    context->ui.SetTitleText(title_text.c_str());
    SetWindowTextW(hwnd, title_text.c_str());
    RenderApplication(context);
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

void HandleOpenImageCommand(HWND hwnd, AppContext* context)
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

    LoadImageFile(hwnd, context, path.c_str());
}

void RenderIfNeeded(HWND hwnd, AppContext* context, UiEventResult result)
{
    if (context == nullptr) {
        return;
    }

    if (result.released_capture) {
        ReleaseCapture();
    }

    if (result.needs_render) {
        RenderApplication(context);
    }

    if (result.command == UiCommand::OpenImage) {
        HandleOpenImageCommand(hwnd, context);
    } else if (result.command != UiCommand::None) {
        ExecuteUiCommand(hwnd, context, result.command);
    }
}

void RenderIfNeeded(AppContext* context, ImageViewerEventResult result)
{
    if (context == nullptr) {
        return;
    }

    if (result.released_capture) {
        ReleaseCapture();
    }

    if (result.needs_render) {
        RenderApplication(context);
    }
}

LRESULT HitTestFrame(HWND hwnd, LPARAM lparam)
{
    POINT screen_point{
        GET_X_LPARAM(lparam),
        GET_Y_LPARAM(lparam),
    };

    RECT window_rect = {};
    GetWindowRect(hwnd, &window_rect);
    const UINT dpi = GetDpiForWindow(hwnd);
    const int resize_border = ResizeBorderThicknessForDpi(dpi);

    if (!IsZoomed(hwnd)) {
        const bool left = screen_point.x >= window_rect.left && screen_point.x < window_rect.left + resize_border;
        const bool right = screen_point.x < window_rect.right && screen_point.x >= window_rect.right - resize_border;
        const bool top = screen_point.y >= window_rect.top && screen_point.y < window_rect.top + resize_border;
        const bool bottom = screen_point.y < window_rect.bottom && screen_point.y >= window_rect.bottom - resize_border;

        if (top && left) {
            return HTTOPLEFT;
        }
        if (top && right) {
            return HTTOPRIGHT;
        }
        if (bottom && left) {
            return HTBOTTOMLEFT;
        }
        if (bottom && right) {
            return HTBOTTOMRIGHT;
        }
        if (left) {
            return HTLEFT;
        }
        if (right) {
            return HTRIGHT;
        }
        if (top) {
            return HTTOP;
        }
        if (bottom) {
            return HTBOTTOM;
        }
    }

    POINT client_point = screen_point;
    ScreenToClient(hwnd, &client_point);
    const D2D1_POINT_2F render_point = CoordinateSpace::FromWindow(hwnd).PhysicalToRender(client_point);
    AppContext* context = GetAppContext(hwnd);
    if (context != nullptr && context->ui.IsPointInCaptionDragArea(render_point)) {
        return HTCAPTION;
    }

    return HTCLIENT;
}

LRESULT CalculateClientArea(HWND hwnd, WPARAM wparam, LPARAM lparam)
{
    if (wparam != TRUE) {
        return DefWindowProcW(hwnd, WM_NCCALCSIZE, wparam, lparam);
    }

    auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam);
    if (IsZoomed(hwnd)) {
        const int resize_border = ResizeBorderThicknessForDpi(GetDpiForWindow(hwnd));
        params->rgrc[0].left += resize_border;
        params->rgrc[0].top += resize_border;
        params->rgrc[0].right -= resize_border;
        params->rgrc[0].bottom -= resize_border;
    }

    return 0;
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
    case kImgViewerUiCommandMessage: {
        ExecuteUiCommand(hwnd, GetAppContext(hwnd), static_cast<UiCommand>(wparam));
        return 0;
    }

    case kImgViewerOpenImageMessage: {
        HandleOpenImageCommand(hwnd, GetAppContext(hwnd));
        return 0;
    }

    case WM_NCCALCSIZE:
        return CalculateClientArea(hwnd, wparam, lparam);

    case WM_NCCREATE: {
        const auto* create_struct = reinterpret_cast<CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_CREATE: {
        AppContext* context = GetAppContext(hwnd);
        if (context == nullptr ||
            FAILED(context->renderer.Initialize(hwnd)) ||
            FAILED(context->viewer.Initialize()) ||
            FAILED(CreateUiAccessibilityProvider(hwnd, &context->ui, context->accessibility_provider.put())) ||
            FAILED(RenderApplication(context))) {
            return -1;
        }

        return 0;
    }

    case WM_SIZE: {
        AppContext* context = GetAppContext(hwnd);
        if (context != nullptr && FAILED(context->renderer.Resize())) {
            return -1;
        }
        if (context != nullptr) {
            SyncWindowState(hwnd, &context->ui);
            if (FAILED(RenderApplication(context))) {
                return -1;
            }
        }

        return 0;
    }

    case WM_NCHITTEST:
        return HitTestFrame(hwnd, lparam);

    case WM_NCLBUTTONDBLCLK:
        if (wparam == HTCAPTION) {
            ExecuteUiCommand(hwnd, GetAppContext(hwnd), UiCommand::ToggleMaximize);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);

    case WM_ERASEBKGND:
        return 1;

    case WM_MOUSEMOVE: {
        AppContext* context = GetAppContext(hwnd);
        const D2D1_POINT_2F point = GetPointerPoint(hwnd, lparam);
        TrackMouseLeave(hwnd);
        ImageViewerEventResult viewer_result = {};
        if (context != nullptr) {
            viewer_result = context->viewer.OnPointerMove(point.x, point.y, context->renderer.ViewportPixelSize());
        }
        RenderIfNeeded(context, viewer_result);
        if (context != nullptr && !viewer_result.handled) {
            RenderIfNeeded(hwnd, context, context->ui.OnPointerMove(point));
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        AppContext* context = GetAppContext(hwnd);
        const D2D1_POINT_2F point = GetPointerPoint(hwnd, lparam);
        UiEventResult ui_result = context != nullptr ? context->ui.OnPointerDown(point) : UiEventResult{};
        ImageViewerEventResult viewer_result = {};
        if (context != nullptr && !ui_result.handled) {
            viewer_result = context->viewer.OnPointerDown(point.x, point.y, context->renderer.ViewportPixelSize());
        }
        if (ui_result.captured || viewer_result.captured) {
            SetCapture(hwnd);
        }
        RenderIfNeeded(hwnd, context, ui_result);
        RenderIfNeeded(context, viewer_result);
        return (ui_result.handled || viewer_result.handled) ? 0 : DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_LBUTTONUP: {
        AppContext* context = GetAppContext(hwnd);
        const D2D1_POINT_2F point = GetPointerPoint(hwnd, lparam);
        ImageViewerEventResult viewer_result = {};
        if (context != nullptr) {
            viewer_result = context->viewer.OnPointerUp(point.x, point.y, context->renderer.ViewportPixelSize());
        }
        RenderIfNeeded(context, viewer_result);
        UiEventResult ui_result = {};
        if (context != nullptr && !viewer_result.handled) {
            ui_result = context->ui.OnPointerUp(point);
            RenderIfNeeded(hwnd, context, ui_result);
        }
        return (ui_result.handled || viewer_result.handled) ? 0 : DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_MOUSELEAVE: {
        AppContext* context = GetAppContext(hwnd);
        RenderIfNeeded(hwnd, context, context != nullptr ? context->ui.OnPointerLeave() : UiEventResult{});
        return 0;
    }

    case WM_MOUSEWHEEL: {
        AppContext* context = GetAppContext(hwnd);
        const D2D1_POINT_2F point = GetScreenPointerPoint(hwnd, lparam);
        if (context != nullptr &&
            context->viewer.OnMouseWheel(
                point.x,
                point.y,
                GET_WHEEL_DELTA_WPARAM(wparam),
                context->renderer.ViewportPixelSize())) {
            RenderApplication(context);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        AppContext* context = GetAppContext(hwnd);
        if (context != nullptr && context->viewer.OnKeyDown(static_cast<UINT>(wparam))) {
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_KEYUP:
    case WM_SYSKEYUP: {
        AppContext* context = GetAppContext(hwnd);
        if (context != nullptr && context->viewer.OnKeyUp(static_cast<UINT>(wparam))) {
            ReleaseCapture();
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    case WM_DROPFILES: {
        AppContext* context = GetAppContext(hwnd);
        const HDROP drop = reinterpret_cast<HDROP>(wparam);
        const UINT length = DragQueryFileW(drop, 0, nullptr, 0);
        if (length > 0) {
            std::vector<wchar_t> path(static_cast<size_t>(length) + 1);
            DragQueryFileW(drop, 0, path.data(), static_cast<UINT>(path.size()));
            LoadImageFile(hwnd, context, path.data());
        }
        DragFinish(drop);
        return 0;
    }

    case WM_GETOBJECT: {
        if (lparam == UiaRootObjectId) {
            AppContext* context = GetAppContext(hwnd);
            if (context != nullptr) {
                return UiaReturnRawElementProvider(
                    hwnd,
                    wparam,
                    lparam,
                    context->accessibility_provider.get());
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
    window_class.style = CS_DROPSHADOW;
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

    AppContext context;
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
        &context)};
    RETURN_LAST_ERROR_IF_NULL(window.get());
    DragAcceptFiles(window.get(), TRUE);
    DisableIme(window.get());
    RETURN_IF_FAILED(ApplyDwmFrame(window.get()));
    RETURN_IF_WIN32_BOOL_FALSE(SetWindowPos(
        window.get(),
        nullptr,
        0,
        0,
        0,
        0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));

    ShowWindow(window.get(), SW_SHOWDEFAULT);
    RETURN_IF_WIN32_BOOL_FALSE(UpdateWindow(window.get()));

    int argc = 0;
    wil::unique_hlocal command_line_args{reinterpret_cast<HLOCAL>(CommandLineToArgvW(GetCommandLineW(), &argc))};
    RETURN_LAST_ERROR_IF_NULL(command_line_args.get());
    auto** argv = reinterpret_cast<wchar_t**>(command_line_args.get());
    if (argc > 1) {
        LoadImageFile(window.get(), &context, argv[1]);
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
