#pragma once

#include <windows.h>

namespace win32 {

enum class NativeWindowFrame {
    MainWindow,
    BorderlessMainWindow,
    Dialog,
};

struct WindowMessageResult final {
    bool handled = false;
    LRESULT value = 0;

    static constexpr WindowMessageResult Handled(LRESULT result = 0)
    {
        return WindowMessageResult{.handled = true, .value = result};
    }

    static constexpr WindowMessageResult Unhandled()
    {
        return WindowMessageResult{};
    }
};

class NativeWindow;

class NativeWindowDelegate {
public:
    virtual ~NativeWindowDelegate() = default;

    virtual WindowMessageResult OnWindowMessage(
        NativeWindow& window,
        UINT message,
        WPARAM wparam,
        LPARAM lparam) = 0;
};

struct NativeWindowOptions final {
    HINSTANCE instance = nullptr;
    HCURSOR cursor = nullptr;
    const wchar_t* title = L"";
    NativeWindowFrame frame = NativeWindowFrame::MainWindow;
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    int width = CW_USEDEFAULT;
    int height = CW_USEDEFAULT;
    HWND owner = nullptr;
    void* user_data = nullptr;
};

class NativeWindow final {
public:
    NativeWindow() = default;
    NativeWindow(const NativeWindow&) = delete;
    NativeWindow& operator=(const NativeWindow&) = delete;
    ~NativeWindow();

    HRESULT Create(const NativeWindowOptions& options, NativeWindowDelegate* delegate);
    void Destroy();
    void Show(int show_command);
    HWND Hwnd() const;
    HINSTANCE Instance() const;
    void* UserData() const;
    static void* UserData(HWND hwnd);

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);
    static HRESULT RegisterWindowClass(const NativeWindowOptions& options);

    HWND hwnd_ = nullptr;
    NativeWindowDelegate* delegate_ = nullptr;
    HINSTANCE instance_ = nullptr;
    void* user_data_ = nullptr;
};

} // namespace win32
