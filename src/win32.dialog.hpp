#pragma once

#include <windows.h>

#include <span>
#include <string>

namespace win32 {

struct NativeFileDialogFilter {
    const wchar_t* name = nullptr;
    const wchar_t* pattern = nullptr;
};

struct NativeOpenFileDialogOptions {
    std::span<const NativeFileDialogFilter> filters;
    UINT default_filter_index = 1;
};

struct NativeSaveFileDialogOptions {
    std::span<const NativeFileDialogFilter> filters;
    UINT default_filter_index = 1;
    const wchar_t* default_extension = nullptr;
};

HRESULT OpenNativeFileDialog(HWND hwnd, const NativeOpenFileDialogOptions& options, std::wstring* path);
HRESULT OpenNativeSaveFileDialog(HWND hwnd, const NativeSaveFileDialogOptions& options, std::wstring* path);

} // namespace win32
