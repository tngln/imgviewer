#include "image.decoder.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cwctype>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

#include <wil/result_macros.h>

#include "imgviewer.strings.hpp"
#include "image.animation_decoder.hpp"
#include "image.stb_decoder.hpp"
#include "image.utils.hpp"

namespace {

bool IsStbFallbackExtension(const wchar_t* path)
{
    const std::wstring ext = image_utils::ToLowerExtension(path);
    return ext == L".tga" || ext == L".psd";
}

void AddColorRow(ImageMetadata* metadata, const wchar_t* label, std::wstring value)
{
    if (metadata == nullptr || value.empty()) {
        return;
    }

    metadata->color_rows.push_back(ImageMetadataRow{.label = label, .value = std::move(value)});
}

std::wstring YesNo(bool value)
{
    return ImgViewerString(value ? ImgViewerStringId::Yes : ImgViewerStringId::No);
}

std::wstring FormatChannelDepth(UINT bits_per_channel, UINT channel_count)
{
    if (bits_per_channel == 0) {
        return {};
    }

    wchar_t text[64] = {};
    if (channel_count > 0) {
        swprintf_s(text, L"%u-bit x %u", bits_per_channel, channel_count);
    } else {
        swprintf_s(text, L"%u-bit", bits_per_channel);
    }
    return text;
}

void RebuildColorRows(ImageMetadata* metadata)
{
    if (metadata == nullptr) {
        return;
    }

    const ImageColorInfo& color = metadata->color_info;
    metadata->color_rows.clear();
    AddColorRow(metadata, ImgViewerString(ImgViewerStringId::Container), color.container);
    AddColorRow(metadata, ImgViewerString(ImgViewerStringId::SourcePixels), FormatChannelDepth(color.bits_per_channel, color.channel_count));
    AddColorRow(metadata, ImgViewerString(ImgViewerStringId::WicFormat), color.pixel_format);
    AddColorRow(metadata, ImgViewerString(ImgViewerStringId::IccProfile), YesNo(color.has_icc_profile));
    AddColorRow(metadata, ImgViewerString(ImgViewerStringId::Primaries), color.dynamic_range.primaries);
    AddColorRow(metadata, ImgViewerString(ImgViewerStringId::Transfer), color.dynamic_range.transfer_function);
    AddColorRow(metadata, ImgViewerString(ImgViewerStringId::HdrMetadata), YesNo(color.dynamic_range.has_hdr_metadata));
    AddColorRow(metadata, ImgViewerString(ImgViewerStringId::HdrSource), YesNo(color.dynamic_range.high_dynamic_range));
    AddColorRow(metadata, ImgViewerString(ImgViewerStringId::DisplayPath), color.display_path);
    AddColorRow(metadata, ImgViewerString(ImgViewerStringId::SourcePreserved), YesNo(color.source_preserved));
}

bool ReadFilePrefix(const wchar_t* path, std::vector<BYTE>* bytes)
{
    if (path == nullptr || bytes == nullptr) {
        return false;
    }

    std::ifstream stream(std::filesystem::path(path), std::ios::binary);
    if (!stream) {
        return false;
    }

    stream.seekg(0, std::ios::end);
    const std::streamoff file_size = stream.tellg();
    if (file_size <= 0) {
        return false;
    }
    stream.seekg(0, std::ios::beg);
    const size_t size = static_cast<size_t>((std::min<std::streamoff>)(file_size, 1024 * 1024));
    bytes->resize(size);
    stream.read(reinterpret_cast<char*>(bytes->data()), static_cast<std::streamsize>(bytes->size()));
    return stream.gcount() == static_cast<std::streamsize>(bytes->size());
}

uint32_t ReadBe32(const std::vector<BYTE>& bytes, size_t offset)
{
    return (static_cast<uint32_t>(bytes[offset]) << 24) |
        (static_cast<uint32_t>(bytes[offset + 1]) << 16) |
        (static_cast<uint32_t>(bytes[offset + 2]) << 8) |
        static_cast<uint32_t>(bytes[offset + 3]);
}

uint16_t ReadBe16(const std::vector<BYTE>& bytes, size_t offset)
{
    return static_cast<uint16_t>((static_cast<uint16_t>(bytes[offset]) << 8) | bytes[offset + 1]);
}

bool IsChunkType(const BYTE* type, const char (&text)[5])
{
    return type[0] == static_cast<BYTE>(text[0]) &&
        type[1] == static_cast<BYTE>(text[1]) &&
        type[2] == static_cast<BYTE>(text[2]) &&
        type[3] == static_cast<BYTE>(text[3]);
}

std::wstring PngTransferName(BYTE value)
{
    switch (value) {
    case 1:
        return L"BT.709";
    case 13:
        return L"sRGB";
    case 16:
        return L"PQ";
    case 18:
        return L"HLG";
    default: {
        wchar_t text[32] = {};
        swprintf_s(text, L"cICP %u", value);
        return text;
    }
    }
}

std::wstring PngPrimariesName(BYTE value)
{
    switch (value) {
    case 1:
        return L"BT.709";
    case 9:
        return L"BT.2020";
    case 12:
        return L"Display P3";
    default: {
        wchar_t text[32] = {};
        swprintf_s(text, L"cICP %u", value);
        return text;
    }
    }
}

uint32_t ReadBe32(const BYTE* data)
{
    return (static_cast<uint32_t>(data[0]) << 24) |
        (static_cast<uint32_t>(data[1]) << 16) |
        (static_cast<uint32_t>(data[2]) << 8) |
        static_cast<uint32_t>(data[3]);
}

void ApplyCicpMetadata(BYTE primaries, BYTE transfer, ImageColorInfo* color)
{
    color->dynamic_range.primaries = PngPrimariesName(primaries);
    color->dynamic_range.transfer_function = PngTransferName(transfer);
    const bool high_dynamic_range = transfer == 16 || transfer == 18;
    color->dynamic_range.high_dynamic_range = color->dynamic_range.high_dynamic_range || high_dynamic_range;
    color->dynamic_range.has_hdr_metadata = color->dynamic_range.has_hdr_metadata || high_dynamic_range;
}

void InspectIccProfileMetadata(const BYTE* data, size_t size, ImageColorInfo* color)
{
    if (data == nullptr || color == nullptr || size < 132) {
        return;
    }

    constexpr char kIccSignature[] = "acsp";
    if (std::memcmp(data + 36, kIccSignature, sizeof(kIccSignature) - 1) != 0) {
        return;
    }

    const uint32_t tag_count = ReadBe32(data + 128);
    if (tag_count > (size - 132) / 12) {
        return;
    }

    constexpr char kCicpTag[] = "cicp";
    for (uint32_t index = 0; index < tag_count; ++index) {
        const BYTE* tag = data + 132 + static_cast<size_t>(index) * 12;
        if (std::memcmp(tag, kCicpTag, sizeof(kCicpTag) - 1) != 0) {
            continue;
        }

        const uint32_t offset = ReadBe32(tag + 4);
        const uint32_t length = ReadBe32(tag + 8);
        if (offset > size || length > size - offset || length < 12) {
            return;
        }

        const BYTE* cicp = data + offset;
        if (std::memcmp(cicp, kCicpTag, sizeof(kCicpTag) - 1) != 0) {
            return;
        }

        ApplyCicpMetadata(cicp[8], cicp[9], color);
        return;
    }
}

void InspectPngMetadata(const wchar_t* path, ImageColorInfo* color)
{
    if (path == nullptr || color == nullptr) {
        return;
    }

    std::vector<BYTE> bytes;
    if (!ReadFilePrefix(path, &bytes) || bytes.size() < 33) {
        return;
    }

    constexpr std::array<BYTE, 8> kPngSignature{{0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A}};
    if (!std::equal(kPngSignature.begin(), kPngSignature.end(), bytes.begin())) {
        return;
    }

    color->container = L"PNG";
    size_t offset = 8;
    while (offset + 12 <= bytes.size()) {
        const uint32_t length = ReadBe32(bytes, offset);
        const BYTE* type = bytes.data() + offset + 4;
        const size_t data_offset = offset + 8;
        if (data_offset + length + 4 > bytes.size()) {
            break;
        }

        if (IsChunkType(type, "IHDR") && length >= 13) {
            color->bits_per_channel = bytes[data_offset + 8];
            const BYTE color_type = bytes[data_offset + 9];
            color->channel_count = color_type == 6 ? 4 : (color_type == 2 ? 3 : (color_type == 4 ? 2 : 1));
        } else if (IsChunkType(type, "iCCP")) {
            color->has_icc_profile = true;
        } else if (IsChunkType(type, "cICP") && length >= 4) {
            ApplyCicpMetadata(bytes[data_offset], bytes[data_offset + 1], color);
        } else if (IsChunkType(type, "cLLi") || IsChunkType(type, "mDCv")) {
            color->dynamic_range.has_hdr_metadata = true;
        } else if (IsChunkType(type, "IEND")) {
            break;
        }

        offset = data_offset + length + 4;
    }
}

void InspectJpegMetadata(const wchar_t* path, ImageColorInfo* color)
{
    if (path == nullptr || color == nullptr) {
        return;
    }

    std::vector<BYTE> bytes;
    if (!ReadFilePrefix(path, &bytes) || bytes.size() < 4 || bytes[0] != 0xFF || bytes[1] != 0xD8) {
        return;
    }

    color->container = L"JPEG";
    size_t offset = 2;
    while (offset + 4 <= bytes.size()) {
        if (bytes[offset] != 0xFF) {
            ++offset;
            continue;
        }
        while (offset < bytes.size() && bytes[offset] == 0xFF) {
            ++offset;
        }
        if (offset >= bytes.size()) {
            break;
        }

        const BYTE marker = bytes[offset++];
        if (marker == 0xDA || marker == 0xD9) {
            break;
        }
        if (offset + 2 > bytes.size()) {
            break;
        }

        const uint16_t length = ReadBe16(bytes, offset);
        const size_t data_offset = offset + 2;
        if (length < 2 || data_offset + length - 2 > bytes.size()) {
            break;
        }

        if (marker == 0xE2 && length >= 16) {
            constexpr char kIccSignature[] = "ICC_PROFILE";
            if (std::memcmp(bytes.data() + data_offset, kIccSignature, sizeof(kIccSignature) - 1) == 0) {
                color->has_icc_profile = true;
                InspectIccProfileMetadata(
                    bytes.data() + data_offset + 14,
                    static_cast<size_t>(length - 2 - 14),
                    color);
            }
        } else if ((marker == 0xC0 || marker == 0xC1 || marker == 0xC2) && length >= 8) {
            color->bits_per_channel = bytes[data_offset];
            color->channel_count = bytes[data_offset + 5];
            if (marker == 0xC2) {
                color->pixel_format = L"Progressive JPEG";
            }
        }

        offset = data_offset + length - 2;
    }
}

std::wstring WicPixelFormatName(const WICPixelFormatGUID& format)
{
    if (IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA)) return L"32bpp BGRA";
    if (IsEqualGUID(format, GUID_WICPixelFormat32bppPBGRA)) return L"32bpp PBGRA";
    if (IsEqualGUID(format, GUID_WICPixelFormat24bppBGR)) return L"24bpp BGR";
    if (IsEqualGUID(format, GUID_WICPixelFormat48bppRGB)) return L"48bpp RGB";
    if (IsEqualGUID(format, GUID_WICPixelFormat64bppRGBA)) return L"64bpp RGBA";
    if (IsEqualGUID(format, GUID_WICPixelFormat64bppPRGBA)) return L"64bpp PRGBA";
    if (IsEqualGUID(format, GUID_WICPixelFormat64bppRGBAHalf)) return L"64bpp RGBA half";
    if (IsEqualGUID(format, GUID_WICPixelFormat128bppRGBAFloat)) return L"128bpp RGBA float";
    return L"WIC source";
}

void FillWicColorInfo(IWICBitmapSource* source, ImageColorInfo* color)
{
    if (source == nullptr || color == nullptr) {
        return;
    }

    WICPixelFormatGUID format = {};
    if (FAILED(source->GetPixelFormat(&format))) {
        return;
    }

    if (color->pixel_format.empty()) {
        color->pixel_format = WicPixelFormatName(format);
    }
    if (color->bits_per_channel == 0) {
        if (IsEqualGUID(format, GUID_WICPixelFormat48bppRGB)) {
            color->bits_per_channel = 16;
            color->channel_count = 3;
        } else if (IsEqualGUID(format, GUID_WICPixelFormat64bppRGBA) ||
            IsEqualGUID(format, GUID_WICPixelFormat64bppPRGBA) ||
            IsEqualGUID(format, GUID_WICPixelFormat64bppRGBAHalf)) {
            color->bits_per_channel = 16;
            color->channel_count = 4;
        } else if (IsEqualGUID(format, GUID_WICPixelFormat128bppRGBAFloat)) {
            color->bits_per_channel = 32;
            color->channel_count = 4;
        } else if (IsEqualGUID(format, GUID_WICPixelFormat24bppBGR)) {
            color->bits_per_channel = 8;
            color->channel_count = 3;
        } else if (IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA) ||
            IsEqualGUID(format, GUID_WICPixelFormat32bppPBGRA)) {
            color->bits_per_channel = 8;
            color->channel_count = 4;
        }
    }
}

void FinalizeColorMetadata(const wchar_t* path, IWICBitmapSource* source, ImageMetadata* metadata)
{
    if (metadata == nullptr) {
        return;
    }

    ImageColorInfo& color = metadata->color_info;
    if (path != nullptr && path[0] != L'\0') {
        color.source_path = path;
        color.source_preserved = true;
        const std::wstring ext = image_utils::ToLowerExtension(path);
        if (ext == L".png" || ext == L".apng") {
            InspectPngMetadata(path, &color);
        } else if (ext == L".jpg" || ext == L".jpeg") {
            InspectJpegMetadata(path, &color);
        }
    }

    FillWicColorInfo(source, &color);
    WICPixelFormatGUID format = {};
    const bool has_wic_format = source != nullptr && SUCCEEDED(source->GetPixelFormat(&format));
    color.dynamic_range.high_dynamic_range =
        color.dynamic_range.high_dynamic_range ||
        color.bits_per_channel > 8 ||
        (has_wic_format && IsEqualGUID(format, GUID_WICPixelFormat64bppRGBAHalf)) ||
        (has_wic_format && IsEqualGUID(format, GUID_WICPixelFormat128bppRGBAFloat));
    color.dynamic_range.tone_mapped_to_sdr = color.dynamic_range.high_dynamic_range || color.bits_per_channel > 8;
    color.display_path = color.dynamic_range.tone_mapped_to_sdr ? L"SDR display copy" : L"SDR";
    RebuildColorRows(metadata);
}

uint16_t FloatToHalf(float value)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    const uint32_t sign = (bits >> 16) & 0x8000;
    int exponent = static_cast<int>((bits >> 23) & 0xFF) - 127 + 15;
    uint32_t mantissa = bits & 0x7FFFFF;

    if (exponent <= 0) {
        if (exponent < -10) {
            return static_cast<uint16_t>(sign);
        }
        mantissa = (mantissa | 0x800000) >> static_cast<uint32_t>(1 - exponent);
        return static_cast<uint16_t>(sign | ((mantissa + 0x1000) >> 13));
    }
    if (exponent >= 31) {
        return static_cast<uint16_t>(sign | 0x7C00);
    }

    return static_cast<uint16_t>(sign | (static_cast<uint32_t>(exponent) << 10) | ((mantissa + 0x1000) >> 13));
}

float PqToScRgb(float encoded)
{
    constexpr double m1 = 2610.0 / 16384.0;
    constexpr double m2 = 2523.0 / 32.0;
    constexpr double c1 = 3424.0 / 4096.0;
    constexpr double c2 = 2413.0 / 128.0;
    constexpr double c3 = 2392.0 / 128.0;

    const double n = std::clamp(static_cast<double>(encoded), 0.0, 1.0);
    const double n_power = std::pow(n, 1.0 / m2);
    const double ratio = (std::max)(0.0, n_power - c1) / (c2 - c3 * n_power);
    const double nits = 10000.0 * std::pow(ratio, 1.0 / m1);
    return static_cast<float>(nits / 80.0);
}

float HlgToScRgb(float encoded)
{
    constexpr double a = 0.17883277;
    constexpr double b = 0.28466892;
    constexpr double c = 0.55991073;
    const double e = std::clamp(static_cast<double>(encoded), 0.0, 1.0);
    const double linear = e <= 0.5 ? (e * e) / 3.0 : (std::exp((e - c) / a) + b) / 12.0;
    return static_cast<float>(linear * 8.0);
}

float HdrEncodedToScRgb(float encoded, const ImageColorInfo& color)
{
    if (color.dynamic_range.transfer_function == L"PQ") {
        return PqToScRgb(encoded);
    }
    if (color.dynamic_range.transfer_function == L"HLG") {
        return HlgToScRgb(encoded);
    }
    return encoded;
}

void Bt2020ToScRgb(float red, float green, float blue, float* out_red, float* out_green, float* out_blue)
{
    const float x = 0.636958f * red + 0.144617f * green + 0.168881f * blue;
    const float y = 0.262700f * red + 0.677998f * green + 0.059302f * blue;
    const float z = 0.028073f * green + 1.060985f * blue;

    *out_red = 3.240454f * x - 1.537139f * y - 0.498531f * z;
    *out_green = -0.969266f * x + 1.876011f * y + 0.041556f * z;
    *out_blue = 0.055643f * x - 0.204026f * y + 1.057225f * z;
}

HRESULT CreateHdrDisplayBitmap(
    IWICImagingFactory2* wic_factory,
    IWICBitmapSource* source,
    ID2D1DeviceContext* d2d_context,
    ImageMetadata* metadata,
    ID2D1Bitmap1** bitmap)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, wic_factory);
    RETURN_HR_IF_NULL(E_INVALIDARG, source);
    RETURN_HR_IF_NULL(E_INVALIDARG, d2d_context);
    RETURN_HR_IF_NULL(E_INVALIDARG, metadata);
    RETURN_HR_IF_NULL(E_POINTER, bitmap);
    *bitmap = nullptr;

    const ImageColorInfo& color = metadata->color_info;
    if (!color.dynamic_range.high_dynamic_range ||
        (color.dynamic_range.transfer_function != L"PQ" && color.dynamic_range.transfer_function != L"HLG")) {
        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    }

    UINT width = 0;
    UINT height = 0;
    RETURN_IF_FAILED(source->GetSize(&width, &height));
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), width == 0 || height == 0);

    wil::com_ptr<IWICFormatConverter> converter;
    RETURN_IF_FAILED(wic_factory->CreateFormatConverter(converter.put()));
    RETURN_IF_FAILED(converter->Initialize(
        source,
        GUID_WICPixelFormat48bppRGB,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom));

    const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);
    RETURN_HR_IF(
        HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW),
        pixel_count > (std::numeric_limits<size_t>::max)() / 8);
    std::vector<uint16_t> rgb48(pixel_count * 3);
    const UINT source_stride = width * 6;
    RETURN_IF_FAILED(converter->CopyPixels(
        nullptr,
        source_stride,
        static_cast<UINT>(rgb48.size() * sizeof(uint16_t)),
        reinterpret_cast<BYTE*>(rgb48.data())));

    std::vector<uint16_t> rgba16f(pixel_count * 4);
    const bool bt2020 = color.dynamic_range.primaries == L"BT.2020";
    for (size_t index = 0; index < pixel_count; ++index) {
        float red = HdrEncodedToScRgb(static_cast<float>(rgb48[index * 3]) / 65535.0f, color);
        float green = HdrEncodedToScRgb(static_cast<float>(rgb48[index * 3 + 1]) / 65535.0f, color);
        float blue = HdrEncodedToScRgb(static_cast<float>(rgb48[index * 3 + 2]) / 65535.0f, color);
        if (bt2020) {
            Bt2020ToScRgb(red, green, blue, &red, &green, &blue);
        }

        rgba16f[index * 4] = FloatToHalf((std::max)(0.0f, red));
        rgba16f[index * 4 + 1] = FloatToHalf((std::max)(0.0f, green));
        rgba16f[index * 4 + 2] = FloatToHalf((std::max)(0.0f, blue));
        rgba16f[index * 4 + 3] = FloatToHalf(1.0f);
    }

    const D2D1_BITMAP_PROPERTIES1 bitmap_properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_NONE,
        D2D1::PixelFormat(DXGI_FORMAT_R16G16B16A16_FLOAT, D2D1_ALPHA_MODE_IGNORE),
        96.0f,
        96.0f);
    RETURN_IF_FAILED(d2d_context->CreateBitmap(
        D2D1::SizeU(width, height),
        rgba16f.data(),
        width * 8,
        bitmap_properties,
        bitmap));

    metadata->color_info.display_path = L"HDR scRGB FP16";
    metadata->color_info.dynamic_range.tone_mapped_to_sdr = false;
    RebuildColorRows(metadata);
    return S_OK;
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
    *source = nullptr;
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), width == 0 || height == 0);

    const size_t stride = static_cast<size_t>(width) * 4;
    RETURN_HR_IF(
        HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW),
        stride > (std::numeric_limits<UINT>::max)() ||
            bgra.size() > (std::numeric_limits<UINT>::max)());
    RETURN_HR_IF(
        HRESULT_FROM_WIN32(ERROR_INVALID_DATA),
        bgra.size() != stride * static_cast<size_t>(height));

    wil::com_ptr<IWICBitmap> bitmap;
    RETURN_IF_FAILED(wic_factory->CreateBitmapFromMemory(
        width,
        height,
        GUID_WICPixelFormat32bppBGRA,
        static_cast<UINT>(stride),
        static_cast<UINT>(bgra.size()),
        const_cast<BYTE*>(bgra.data()),
        bitmap.put()));

    wil::com_ptr<IWICBitmap> cached_bitmap;
    RETURN_IF_FAILED(wic_factory->CreateBitmapFromSource(
        bitmap.get(),
        WICBitmapCacheOnLoad,
        cached_bitmap.put()));

    *source = cached_bitmap.detach();
    return S_OK;
}

} // namespace

HRESULT ImageDecoder::Initialize()
{
    RETURN_IF_FAILED(CoCreateInstance(
        CLSID_WICImagingFactory2,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(wic_factory_.put())));

    return S_OK;
}

HRESULT ImageDecoder::DecodeImageFile(
    const wchar_t* path,
    ID2D1DeviceContext* d2d_context,
    DecodedImageSet* image_set)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, path);
    RETURN_HR_IF_NULL(E_INVALIDARG, d2d_context);
    RETURN_HR_IF_NULL(E_POINTER, image_set);
    RETURN_HR_IF_NULL(E_UNEXPECTED, wic_factory_);

    DecodedImageSet decoded_set;
    wil::com_ptr<IWICBitmapDecoder> decoder;
    const HRESULT wic_hr = wic_factory_->CreateDecoderFromFilename(
        path,
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        decoder.put());

    if (SUCCEEDED(wic_hr)) {
        AnimationPixels animation;
        const std::wstring ext = image_utils::ToLowerExtension(path);
        HRESULT animation_hr = HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        if (ext == L".gif") {
            animation_hr = DecodeGifAnimationPixels(wic_factory_.get(), decoder.get(), &animation);
        } else if (ext == L".png" || ext == L".apng") {
            animation_hr = DecodeApngAnimationPixels(wic_factory_.get(), path, &animation);
        } else if (ext == L".webp") {
            animation_hr = DecodeWebpAnimationPixels(wic_factory_.get(), path, &animation);
        }

        if (SUCCEEDED(animation_hr) && animation.frames.size() > 1) {
            decoded_set.animation_loop = animation.loop;
            decoded_set.animation_frames.reserve(animation.frames.size());
            for (const AnimationFramePixels& frame : animation.frames) {
                wil::com_ptr<IWICBitmapSource> source;
                RETURN_IF_FAILED(CreateBitmapSourceFromBgra(
                    wic_factory_.get(),
                    animation.width,
                    animation.height,
                    frame.bgra,
                    source.put()));

                DecodedImage decoded_frame;
                RETURN_IF_FAILED(DecodeBitmapSource(source.get(), d2d_context, &decoded_frame));
                decoded_set.animation_frames.push_back(DecodedAnimationFrame{
                    .image = std::move(decoded_frame),
                    .duration_ms = frame.duration_ms,
                });
            }

            decoded_set.image = DecodedImage{
                .bitmap = decoded_set.animation_frames.front().image.bitmap,
                .pixel_source = decoded_set.animation_frames.front().image.pixel_source,
                .pixel_size = decoded_set.animation_frames.front().image.pixel_size,
            };
            wil::com_ptr<IWICBitmapFrameDecode> first_frame;
            if (SUCCEEDED(decoder->GetFrame(0, first_frame.put()))) {
                ReadImageExifMetadata(first_frame.get(), &decoded_set.image.metadata);
                FinalizeColorMetadata(path, first_frame.get(), &decoded_set.image.metadata);
            }
            *image_set = std::move(decoded_set);
            return S_OK;
        }
    }

    RETURN_IF_FAILED(DecodeFirstFrame(path, d2d_context, &decoded_set.image));
    *image_set = std::move(decoded_set);
    return S_OK;
}

HRESULT ImageDecoder::DecodeFirstFrame(
    const wchar_t* path,
    ID2D1DeviceContext* d2d_context,
    DecodedImage* image)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, path);
    RETURN_HR_IF_NULL(E_INVALIDARG, d2d_context);
    RETURN_HR_IF_NULL(E_POINTER, image);
    RETURN_HR_IF_NULL(E_UNEXPECTED, wic_factory_);

    DecodedImage decoded;
    wil::com_ptr<IWICBitmapDecoder> decoder;
    const HRESULT wic_hr = wic_factory_->CreateDecoderFromFilename(
        path,
        nullptr,
        GENERIC_READ,
        WICDecodeMetadataCacheOnDemand,
        decoder.put());

    if (FAILED(wic_hr)) {
        if (!IsStbFallbackExtension(path)) {
            RETURN_IF_FAILED(wic_hr);
        }

        wil::com_ptr<IWICBitmapSource> stb_source;
        RETURN_IF_FAILED(DecodeStbImageFile(wic_factory_.get(), path, stb_source.put()));
        RETURN_IF_FAILED(DecodeBitmapSource(stb_source.get(), d2d_context, &decoded));
        FinalizeColorMetadata(path, stb_source.get(), &decoded.metadata);
        *image = std::move(decoded);
        return S_OK;
    }

    wil::com_ptr<IWICBitmapFrameDecode> frame;
    RETURN_IF_FAILED(decoder->GetFrame(0, frame.put()));
    RETURN_IF_FAILED(DecodeBitmapSource(frame.get(), d2d_context, &decoded));
    RETURN_IF_FAILED(ReadImageExifMetadata(frame.get(), &decoded.metadata));
    FinalizeColorMetadata(path, frame.get(), &decoded.metadata);
    wil::com_ptr<ID2D1Bitmap1> hdr_bitmap;
    if (SUCCEEDED(CreateHdrDisplayBitmap(
            wic_factory_.get(),
            frame.get(),
            d2d_context,
            &decoded.metadata,
            hdr_bitmap.put()))) {
        decoded.bitmap = std::move(hdr_bitmap);
        decoded.display_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    }

    *image = std::move(decoded);
    return S_OK;
}

HRESULT ImageDecoder::DecodeBitmapSource(
    IWICBitmapSource* source,
    ID2D1DeviceContext* d2d_context,
    DecodedImage* image)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, source);
    RETURN_HR_IF_NULL(E_INVALIDARG, d2d_context);
    RETURN_HR_IF_NULL(E_POINTER, image);
    RETURN_HR_IF_NULL(E_UNEXPECTED, wic_factory_);

    DecodedImage decoded;
    RETURN_IF_FAILED(source->GetSize(&decoded.pixel_size.width, &decoded.pixel_size.height));

    wil::com_ptr<IWICFormatConverter> bgra_converter;
    RETURN_IF_FAILED(wic_factory_->CreateFormatConverter(bgra_converter.put()));
    RETURN_IF_FAILED(bgra_converter->Initialize(
        source,
        GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeMedianCut));
    decoded.pixel_source = bgra_converter;

    wil::com_ptr<IWICFormatConverter> pbgra_converter;
    RETURN_IF_FAILED(wic_factory_->CreateFormatConverter(pbgra_converter.put()));
    RETURN_IF_FAILED(pbgra_converter->Initialize(
        source,
        GUID_WICPixelFormat32bppPBGRA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeMedianCut));

    const D2D1_BITMAP_PROPERTIES1 bitmap_properties = D2D1::BitmapProperties1(
        D2D1_BITMAP_OPTIONS_NONE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        96.0f,
        96.0f);
    RETURN_IF_FAILED(d2d_context->CreateBitmapFromWicBitmap(
        pbgra_converter.get(),
        bitmap_properties,
        decoded.bitmap.put()));
    FinalizeColorMetadata(nullptr, source, &decoded.metadata);

    *image = std::move(decoded);
    return S_OK;
}

IWICImagingFactory2* ImageDecoder::WicFactory() const
{
    return wic_factory_.get();
}
