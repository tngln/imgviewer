#include "win32.window.hpp"

#include <wil/result_macros.h>

namespace win32 {

namespace {

constexpr wchar_t kNativeWindowClassName[] = L"ImgViewerNativeWindow";

DWORD NativeWindowStyle(NativeWindowFrame frame)
{
    switch (frame) {
    case NativeWindowFrame::BorderlessMainWindow:
        return WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    case NativeWindowFrame::Dialog:
        return WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
    case NativeWindowFrame::MainWindow:
    default:
        return WS_OVERLAPPEDWINDOW;
    }
}

DWORD NativeWindowExStyle(const NativeWindowOptions& options)
{
    DWORD ex_style = 0;
    if (options.frame == NativeWindowFrame::Dialog) {
        ex_style |= WS_EX_DLGMODALFRAME;
    }
    return ex_style;
}

} // namespace

NativeWindow::~NativeWindow()
{
    Destroy();
}

HRESULT NativeWindow::Create(const NativeWindowOptions& options, NativeWindowDelegate* delegate)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, options.instance);
    RETURN_HR_IF_NULL(E_INVALIDARG, delegate);
    RETURN_HR_IF(E_UNEXPECTED, hwnd_ != nullptr);

    RETURN_IF_FAILED(RegisterWindowClass(options));

    delegate_ = delegate;
    instance_ = options.instance;
    user_data_ = options.user_data;
    HWND hwnd = CreateWindowExW(
        NativeWindowExStyle(options),
        kNativeWindowClassName,
        options.title,
        NativeWindowStyle(options.frame),
        options.x,
        options.y,
        options.width,
        options.height,
        options.owner,
        nullptr,
        options.instance,
        this);
    if (hwnd == nullptr) {
        delegate_ = nullptr;
        instance_ = nullptr;
        user_data_ = nullptr;
        RETURN_LAST_ERROR();
    }

    return S_OK;
}

void NativeWindow::Destroy()
{
    if (hwnd_ != nullptr) {
        HWND hwnd = hwnd_;
        hwnd_ = nullptr;
        DestroyWindow(hwnd);
    }
}

void NativeWindow::Show(int show_command)
{
    if (hwnd_ != nullptr) {
        ShowWindow(hwnd_, show_command);
        UpdateWindow(hwnd_);
    }
}

HWND NativeWindow::Hwnd() const
{
    return hwnd_;
}

HINSTANCE NativeWindow::Instance() const
{
    return instance_;
}

void* NativeWindow::UserData() const
{
    return user_data_;
}

void* NativeWindow::UserData(HWND hwnd)
{
    NativeWindow* window = reinterpret_cast<NativeWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return window != nullptr ? window->UserData() : nullptr;
}

HRESULT NativeWindow::RegisterWindowClass(const NativeWindowOptions& options)
{
    WNDCLASSEXW window_class = {};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_DBLCLKS;
    window_class.lpfnWndProc = NativeWindow::WindowProc;
    window_class.hInstance = options.instance;
    window_class.hCursor = options.cursor != nullptr ? options.cursor : LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = kNativeWindowClassName;

    const ATOM atom = RegisterClassExW(&window_class);
    if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        RETURN_LAST_ERROR();
    }
    return S_OK;
}

LRESULT CALLBACK NativeWindow::WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    NativeWindow* window = reinterpret_cast<NativeWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        window = static_cast<NativeWindow*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        if (window != nullptr) {
            window->hwnd_ = hwnd;
        }
    }

    if (window != nullptr && window->delegate_ != nullptr) {
        const WindowMessageResult result = window->delegate_->OnWindowMessage(*window, message, wparam, lparam);
        if (result.handled) {
            if (message == WM_NCDESTROY) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
                window->hwnd_ = nullptr;
            }
            return result.value;
        }
    }

    const LRESULT result = DefWindowProcW(hwnd, message, wparam, lparam);
    if (message == WM_NCDESTROY && window != nullptr) {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        window->hwnd_ = nullptr;
    }
    return result;
}

} // namespace win32
