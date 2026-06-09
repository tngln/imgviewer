#include "image.metadata.hpp"

#include <cmath>
#include <cwchar>
#include <cwctype>
#include <string>
#include <utility>

#include <propvarutil.h>
#include <wil/com.h>
#include <wil/result_macros.h>

#include "imgviewer.strings.hpp"

namespace {

struct Rational final {
    double value = 0.0;
    int numerator = 0;
    int denominator = 1;
};

struct ExifFieldSpec final {
    const wchar_t* label;
    const wchar_t* queries[4];
    size_t query_count;
    std::wstring (*formatter)(const PROPVARIANT& value);
};

bool IsBlank(const std::wstring& value)
{
    for (wchar_t ch : value) {
        if (ch != L'\0' && !iswspace(ch)) {
            return false;
        }
    }
    return true;
}

std::wstring TrimTrailingNulls(std::wstring value)
{
    while (!value.empty() && value.back() == L'\0') {
        value.pop_back();
    }
    return value;
}

std::wstring WideFromAnsi(const char* text)
{
    if (text == nullptr || text[0] == '\0') {
        return {};
    }

    const int length = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
    if (length <= 1) {
        return {};
    }

    std::wstring value(static_cast<size_t>(length - 1), L'\0');
    MultiByteToWideChar(CP_ACP, 0, text, -1, value.data(), length);
    return value;
}

std::wstring FormatStringValue(const PROPVARIANT& value)
{
    std::wstring text;
    switch (value.vt) {
    case VT_LPWSTR:
        text = value.pwszVal != nullptr ? value.pwszVal : L"";
        break;
    case VT_BSTR:
        text = value.bstrVal != nullptr ? value.bstrVal : L"";
        break;
    case VT_LPSTR:
        text = WideFromAnsi(value.pszVal);
        break;
    default:
        return {};
    }

    text = TrimTrailingNulls(std::move(text));
    return IsBlank(text) ? std::wstring{} : text;
}

bool TryReadUnsignedInteger(const PROPVARIANT& value, unsigned int* number)
{
    if (number == nullptr) {
        return false;
    }

    switch (value.vt) {
    case VT_UI1:
        *number = value.bVal;
        return true;
    case VT_UI2:
        *number = value.uiVal;
        return true;
    case VT_UI4:
        *number = value.ulVal;
        return true;
    case VT_UINT:
        *number = value.uintVal;
        return true;
    case VT_I2:
        if (value.iVal >= 0) {
            *number = static_cast<unsigned int>(value.iVal);
            return true;
        }
        return false;
    case VT_I4:
        if (value.lVal >= 0) {
            *number = static_cast<unsigned int>(value.lVal);
            return true;
        }
        return false;
    case VT_VECTOR | VT_UI2:
        if (value.caui.cElems > 0 && value.caui.pElems != nullptr) {
            *number = value.caui.pElems[0];
            return true;
        }
        return false;
    default:
        return false;
    }
}

bool TryReadRational(const PROPVARIANT& value, Rational* rational)
{
    if (rational == nullptr) {
        return false;
    }

    LONG high = 0;
    LONG low = 0;
    if (value.vt == VT_UI8) {
        high = static_cast<LONG>(value.uhVal.HighPart);
        low = static_cast<LONG>(value.uhVal.LowPart);
    } else if (value.vt == VT_I8) {
        high = value.hVal.HighPart;
        low = value.hVal.LowPart;
    } else {
        return false;
    }

    if (low == 0) {
        return false;
    }

    *rational = Rational{
        .value = static_cast<double>(high) / static_cast<double>(low),
        .numerator = high,
        .denominator = low,
    };
    return true;
}

std::wstring FormatText(const PROPVARIANT& value)
{
    return FormatStringValue(value);
}

std::wstring FormatIso(const PROPVARIANT& value)
{
    unsigned int iso = 0;
    if (!TryReadUnsignedInteger(value, &iso) || iso == 0) {
        return {};
    }

    wchar_t text[32] = {};
    swprintf_s(text, L"%u", iso);
    return text;
}

std::wstring FormatFocalLength(const PROPVARIANT& value)
{
    Rational focal_length;
    if (!TryReadRational(value, &focal_length) || focal_length.value <= 0.0) {
        return {};
    }

    wchar_t text[64] = {};
    swprintf_s(text, L"%.1f mm", focal_length.value);
    return text;
}

std::wstring FormatAperture(const PROPVARIANT& value)
{
    Rational aperture;
    if (!TryReadRational(value, &aperture) || aperture.value <= 0.0) {
        return {};
    }

    wchar_t text[64] = {};
    swprintf_s(text, L"f/%.1f", aperture.value);
    return text;
}

std::wstring FormatExposureTime(const PROPVARIANT& value)
{
    Rational exposure_time;
    if (!TryReadRational(value, &exposure_time) || exposure_time.value <= 0.0) {
        return {};
    }

    wchar_t text[64] = {};
    if (exposure_time.value < 1.0 && exposure_time.numerator > 0) {
        const int denominator = static_cast<int>(std::lround(static_cast<double>(exposure_time.denominator) / exposure_time.numerator));
        if (denominator > 0) {
            swprintf_s(text, L"1/%d s", denominator);
            return text;
        }
    }

    swprintf_s(text, L"%.3g s", exposure_time.value);
    return text;
}

std::wstring FormatExposureBias(const PROPVARIANT& value)
{
    Rational exposure_bias;
    if (!TryReadRational(value, &exposure_bias)) {
        return {};
    }

    wchar_t text[64] = {};
    swprintf_s(text, L"%+.1f EV", exposure_bias.value);
    return text;
}

std::wstring FormatOrientation(const PROPVARIANT& value)
{
    unsigned int orientation = 0;
    if (!TryReadUnsignedInteger(value, &orientation) || orientation == 0) {
        return {};
    }

    switch (orientation) {
    case 1:
        return ImgViewerString(ImgViewerStringId::OrientationNormal);
    case 2:
        return ImgViewerString(ImgViewerStringId::OrientationMirroredHorizontal);
    case 3:
        return ImgViewerString(ImgViewerStringId::OrientationRotated180);
    case 4:
        return ImgViewerString(ImgViewerStringId::OrientationMirroredVertical);
    case 5:
        return ImgViewerString(ImgViewerStringId::OrientationMirroredHorizontalRotated270);
    case 6:
        return ImgViewerString(ImgViewerStringId::OrientationRotated90);
    case 7:
        return ImgViewerString(ImgViewerStringId::OrientationMirroredHorizontalRotated90);
    case 8:
        return ImgViewerString(ImgViewerStringId::OrientationRotated270);
    default: {
        wchar_t text[32] = {};
        swprintf_s(text, L"%u", orientation);
        return text;
    }
    }
}

bool TryReadFormattedValue(
    IWICMetadataQueryReader* reader,
    const ExifFieldSpec& spec,
    std::wstring* formatted_value)
{
    if (reader == nullptr || formatted_value == nullptr) {
        return false;
    }

    for (size_t index = 0; index < spec.query_count; ++index) {
        const wchar_t* query = spec.queries[index];
        PROPVARIANT value;
        PropVariantInit(&value);
        const HRESULT hr = reader->GetMetadataByName(query, &value);
        if (SUCCEEDED(hr)) {
            std::wstring text = spec.formatter(value);
            PropVariantClear(&value);
            if (!text.empty()) {
                *formatted_value = std::move(text);
                return true;
            }
        } else {
            PropVariantClear(&value);
        }
    }

    return false;
}

} // namespace

HRESULT ReadImageExifMetadata(IWICBitmapFrameDecode* frame, ImageMetadata* metadata)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, frame);
    RETURN_HR_IF_NULL(E_POINTER, metadata);

    ImageMetadata result;
    wil::com_ptr<IWICMetadataQueryReader> reader;
    if (FAILED(frame->GetMetadataQueryReader(reader.put()))) {
        *metadata = std::move(result);
        return S_OK;
    }

    const ExifFieldSpec kFields[] = {
        {ImgViewerString(ImgViewerStringId::DateTaken), {L"/app1/ifd/exif/{ushort=36867}", L"/ifd/exif/{ushort=36867}", L"/app1/ifd/{ushort=306}", L"/ifd/{ushort=306}"}, 4, FormatText},
        {ImgViewerString(ImgViewerStringId::CameraMake), {L"/app1/ifd/{ushort=271}", L"/ifd/{ushort=271}"}, 2, FormatText},
        {ImgViewerString(ImgViewerStringId::CameraModel), {L"/app1/ifd/{ushort=272}", L"/ifd/{ushort=272}"}, 2, FormatText},
        {ImgViewerString(ImgViewerStringId::Lens), {L"/app1/ifd/exif/{ushort=42036}", L"/ifd/exif/{ushort=42036}"}, 2, FormatText},
        {ImgViewerString(ImgViewerStringId::FocalLength), {L"/app1/ifd/exif/{ushort=37386}", L"/ifd/exif/{ushort=37386}"}, 2, FormatFocalLength},
        {ImgViewerString(ImgViewerStringId::Aperture), {L"/app1/ifd/exif/{ushort=33437}", L"/ifd/exif/{ushort=33437}"}, 2, FormatAperture},
        {ImgViewerString(ImgViewerStringId::ExposureTime), {L"/app1/ifd/exif/{ushort=33434}", L"/ifd/exif/{ushort=33434}"}, 2, FormatExposureTime},
        {ImgViewerString(ImgViewerStringId::Iso), {L"/app1/ifd/exif/{ushort=34855}", L"/ifd/exif/{ushort=34855}"}, 2, FormatIso},
        {ImgViewerString(ImgViewerStringId::ExposureBias), {L"/app1/ifd/exif/{ushort=37380}", L"/ifd/exif/{ushort=37380}"}, 2, FormatExposureBias},
        {ImgViewerString(ImgViewerStringId::Orientation), {L"/app1/ifd/{ushort=274}", L"/ifd/{ushort=274}"}, 2, FormatOrientation},
    };

    for (const ExifFieldSpec& field : kFields) {
        std::wstring value;
        if (TryReadFormattedValue(reader.get(), field, &value)) {
            result.exif_rows.push_back(ImageMetadataRow{.label = field.label, .value = std::move(value)});
        }
    }

    *metadata = std::move(result);
    return S_OK;
}
