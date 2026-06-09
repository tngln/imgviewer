#pragma once

#include <wincodec.h>

namespace win32 {

HRESULT SelectCaptureRegion(HWND owner, RECT* region);
HRESULT CaptureScreenRect(IWICImagingFactory2* wic_factory, const RECT& region, IWICBitmapSource** source);

} // namespace win32
