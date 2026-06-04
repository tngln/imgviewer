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

HRESULT ReadClipboardContent(HWND hwnd, IWICImagingFactory2* wic_factory, ClipboardContent* content);

} // namespace win32
