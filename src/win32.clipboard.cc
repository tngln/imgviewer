#include "win32.clipboard.hpp"

#include <algorithm>
#include <cwctype>
#include <limits>
#include <string_view>
#include <vector>

#include <shellapi.h>
#include <wil/resource.h>
#include <wil/result_macros.h>

namespace win32 {
namespace {

constexpr DWORD kBiAlphaBitfields = 6;

bool IsRegularFilePath(const std::wstring& path)
{
    if (path.empty()) {
        return false;
    }

    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

void TrimWhitespace(std::wstring* text)
{
    if (text == nullptr) {
        return;
    }

    const auto first = std::find_if_not(text->begin(), text->end(), [](wchar_t ch) {
        return std::iswspace(ch) != 0;
    });
    const auto last = std::find_if_not(text->rbegin(), text->rend(), [](wchar_t ch) {
        return std::iswspace(ch) != 0;
    }).base();

    if (first >= last) {
        text->clear();
        return;
    }

    *text = std::wstring(first, last);
}

void StripMatchingQuotes(std::wstring* text)
{
    if (text == nullptr || text->size() < 2) {
        return;
    }

    if ((text->front() == L'"' && text->back() == L'"') || (text->front() == L'\'' && text->back() == L'\'')) {
        *text = text->substr(1, text->size() - 2);
    }
}

std::wstring FirstPathFromText(std::wstring_view text)
{
    size_t start = 0;
    while (start <= text.size()) {
        const size_t line_end = text.find_first_of(L"\r\n", start);
        std::wstring line(text.substr(start, line_end == std::wstring_view::npos ? text.size() - start : line_end - start));
        TrimWhitespace(&line);
        StripMatchingQuotes(&line);
        TrimWhitespace(&line);
        if (IsRegularFilePath(line)) {
            return line;
        }

        if (line_end == std::wstring_view::npos) {
            break;
        }
        start = line_end + 1;
        while (start < text.size() && (text[start] == L'\r' || text[start] == L'\n')) {
            ++start;
        }
    }

    return {};
}

bool ReadClipboardDropPath(std::wstring* path)
{
    if (path == nullptr || !IsClipboardFormatAvailable(CF_HDROP)) {
        return false;
    }

    const auto drop = static_cast<HDROP>(GetClipboardData(CF_HDROP));
    if (drop == nullptr) {
        return false;
    }

    const UINT length = DragQueryFileW(drop, 0, nullptr, 0);
    if (length == 0) {
        return false;
    }

    std::vector<wchar_t> buffer(static_cast<size_t>(length) + 1);
    DragQueryFileW(drop, 0, buffer.data(), static_cast<UINT>(buffer.size()));
    std::wstring candidate(buffer.data());
    if (!IsRegularFilePath(candidate)) {
        return false;
    }

    *path = std::move(candidate);
    return true;
}

bool ReadClipboardTextPath(std::wstring* path)
{
    if (path == nullptr || !IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        return false;
    }

    HGLOBAL memory = GetClipboardData(CF_UNICODETEXT);
    if (memory == nullptr) {
        return false;
    }

    const wchar_t* text = static_cast<const wchar_t*>(GlobalLock(memory));
    if (text == nullptr) {
        return false;
    }
    auto unlock = wil::scope_exit([memory] { GlobalUnlock(memory); });

    const std::wstring candidate = FirstPathFromText(text);
    if (candidate.empty()) {
        return false;
    }

    *path = candidate;
    return true;
}

size_t PaletteEntryCount(const BITMAPINFOHEADER& header)
{
    if (header.biClrUsed != 0) {
        return header.biClrUsed;
    }

    if (header.biBitCount <= 8) {
        return size_t{1} << header.biBitCount;
    }

    return 0;
}

size_t DibBitsOffset(const BITMAPINFOHEADER& header)
{
    size_t offset = header.biSize;
    if (header.biCompression == BI_BITFIELDS && header.biSize == sizeof(BITMAPINFOHEADER)) {
        offset += 3 * sizeof(DWORD);
    } else if (header.biCompression == kBiAlphaBitfields && header.biSize == sizeof(BITMAPINFOHEADER)) {
        offset += 4 * sizeof(DWORD);
    }
    offset += PaletteEntryCount(header) * sizeof(RGBQUAD);
    return offset;
}

bool CopyDibPixelsToBgra(const BYTE* dib_data, size_t dib_size, UINT* width, UINT* height, std::vector<BYTE>* bgra)
{
    if (dib_data == nullptr || dib_size < sizeof(BITMAPINFOHEADER) || width == nullptr || height == nullptr || bgra == nullptr) {
        return false;
    }

    const auto* header = reinterpret_cast<const BITMAPINFOHEADER*>(dib_data);
    if (header->biSize < sizeof(BITMAPINFOHEADER) || header->biSize > dib_size || header->biWidth <= 0 || header->biHeight == 0) {
        return false;
    }

    if (header->biCompression != BI_RGB &&
        header->biCompression != BI_BITFIELDS &&
        header->biCompression != kBiAlphaBitfields) {
        return false;
    }

    const bool supported_bpp = header->biBitCount == 24 || header->biBitCount == 32;
    if (!supported_bpp) {
        return false;
    }

    const size_t bits_offset = DibBitsOffset(*header);
    if (bits_offset >= dib_size) {
        return false;
    }

    const UINT dib_width = static_cast<UINT>(header->biWidth);
    const UINT dib_height = static_cast<UINT>(header->biHeight < 0 ? -header->biHeight : header->biHeight);
    const size_t source_stride = (((static_cast<size_t>(dib_width) * header->biBitCount) + 31) / 32) * 4;
    const size_t output_stride = static_cast<size_t>(dib_width) * 4;
    const size_t needed_size = source_stride * static_cast<size_t>(dib_height);
    if (needed_size > dib_size - bits_offset || output_stride > (std::numeric_limits<size_t>::max)() / dib_height) {
        return false;
    }

    std::vector<BYTE> pixels(output_stride * static_cast<size_t>(dib_height));
    const BYTE* source_bits = dib_data + bits_offset;
    const bool top_down = header->biHeight < 0;
    for (UINT y = 0; y < dib_height; ++y) {
        const UINT source_y = top_down ? y : dib_height - y - 1;
        const BYTE* source_row = source_bits + source_stride * source_y;
        BYTE* target_row = pixels.data() + output_stride * y;
        for (UINT x = 0; x < dib_width; ++x) {
            const BYTE* source_pixel = source_row + static_cast<size_t>(x) * header->biBitCount / 8;
            BYTE* target_pixel = target_row + static_cast<size_t>(x) * 4;
            target_pixel[0] = source_pixel[0];
            target_pixel[1] = source_pixel[1];
            target_pixel[2] = source_pixel[2];
            target_pixel[3] = header->biBitCount == 32 && header->biCompression != BI_RGB ? source_pixel[3] : 0xFF;
        }
    }

    *width = dib_width;
    *height = dib_height;
    *bgra = std::move(pixels);
    return true;
}

HRESULT CreateBitmapSourceFromBgra(
    IWICImagingFactory2* wic_factory,
    UINT width,
    UINT height,
    const std::vector<BYTE>& bgra,
    IWICBitmapSource** source)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, wic_factory);
    RETURN_HR_IF_NULL(E_POINTER, source);
    RETURN_HR_IF(E_INVALIDARG, width == 0 || height == 0 || bgra.empty());

    wil::com_ptr<IWICBitmap> memory_bitmap;
    RETURN_IF_FAILED(wic_factory->CreateBitmapFromMemory(
        width,
        height,
        GUID_WICPixelFormat32bppBGRA,
        width * 4,
        static_cast<UINT>(bgra.size()),
        const_cast<BYTE*>(bgra.data()),
        memory_bitmap.put()));

    wil::com_ptr<IWICBitmap> cached_bitmap;
    RETURN_IF_FAILED(wic_factory->CreateBitmapFromSource(memory_bitmap.get(), WICBitmapCacheOnLoad, cached_bitmap.put()));

    *source = cached_bitmap.detach();
    return S_OK;
}

HRESULT ReadClipboardDib(IWICImagingFactory2* wic_factory, UINT format, IWICBitmapSource** source)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, wic_factory);
    RETURN_HR_IF_NULL(E_POINTER, source);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_NOT_FOUND), !IsClipboardFormatAvailable(format));

    HGLOBAL memory = GetClipboardData(format);
    RETURN_LAST_ERROR_IF_NULL(memory);

    const BYTE* dib_data = static_cast<const BYTE*>(GlobalLock(memory));
    RETURN_LAST_ERROR_IF_NULL(dib_data);
    auto unlock = wil::scope_exit([memory] { GlobalUnlock(memory); });

    UINT width = 0;
    UINT height = 0;
    std::vector<BYTE> bgra;
    RETURN_HR_IF(E_FAIL, !CopyDibPixelsToBgra(dib_data, GlobalSize(memory), &width, &height, &bgra));
    RETURN_IF_FAILED(CreateBitmapSourceFromBgra(wic_factory, width, height, bgra, source));
    return S_OK;
}

HRESULT ReadClipboardHbitmap(IWICImagingFactory2* wic_factory, IWICBitmapSource** source)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, wic_factory);
    RETURN_HR_IF_NULL(E_POINTER, source);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_NOT_FOUND), !IsClipboardFormatAvailable(CF_BITMAP));

    HBITMAP bitmap = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
    RETURN_LAST_ERROR_IF_NULL(bitmap);

    wil::com_ptr<IWICBitmap> wic_bitmap;
    RETURN_IF_FAILED(wic_factory->CreateBitmapFromHBITMAP(
        bitmap,
        nullptr,
        WICBitmapUsePremultipliedAlpha,
        wic_bitmap.put()));

    *source = wic_bitmap.detach();
    return S_OK;
}

HRESULT ReadClipboardBitmap(IWICImagingFactory2* wic_factory, IWICBitmapSource** source)
{
    if (SUCCEEDED(ReadClipboardDib(wic_factory, CF_DIBV5, source))) {
        return S_OK;
    }
    if (SUCCEEDED(ReadClipboardDib(wic_factory, CF_DIB, source))) {
        return S_OK;
    }
    RETURN_IF_FAILED(ReadClipboardHbitmap(wic_factory, source));
    return S_OK;
}

} // namespace

HRESULT ReadClipboardContent(HWND hwnd, IWICImagingFactory2* wic_factory, ClipboardContent* content)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, content);
    *content = {};

    RETURN_IF_WIN32_BOOL_FALSE(OpenClipboard(hwnd));
    auto close_clipboard = wil::scope_exit([] { CloseClipboard(); });

    if (ReadClipboardDropPath(&content->path) || ReadClipboardTextPath(&content->path)) {
        return S_OK;
    }

    if (SUCCEEDED(ReadClipboardBitmap(wic_factory, content->bitmap_source.put()))) {
        return S_OK;
    }

    return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}

} // namespace win32
