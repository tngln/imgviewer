#pragma once

#include <windows.h>
#include <wincodec.h>

#include <string>

#include <wil/com.h>

namespace win32 {

struct ClipboardContent final {
    std::wstring path;
    wil::com_ptr<IWICBitmapSource> bitmap_source;
};

bool IsClipboardTextAvailable();
bool CopyTextToClipboard(HWND hwnd, const wchar_t* text);
HRESULT CopyBitmapSourceToClipboard(HWND hwnd, IWICImagingFactory2* wic_factory, IWICBitmapSource* source);
bool ReadClipboardText(HWND hwnd, std::wstring* text);
HRESULT ReadClipboardContent(HWND hwnd, IWICImagingFactory2* wic_factory, ClipboardContent* content);

} // namespace win32
