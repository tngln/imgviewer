#include "image.animation_decoder.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

#include <propvarutil.h>
#include <wil/com.h>
#include <wil/result_macros.h>

#include "image.bitmap.hpp"
#include "image.utils.hpp"

namespace {

constexpr BYTE kPngSignature[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
constexpr UINT kDefaultFrameDurationMs = 100;

struct RectU final {
    UINT x = 0;
    UINT y = 0;
    UINT width = 0;
    UINT height = 0;
};

enum class DisposeOp {
    None,
    Background,
    Previous,
};

enum class BlendOp {
    Source,
    Over,
};

struct PngChunk final {
    char type[4] = {};
    size_t data_offset = 0;
    UINT data_size = 0;
};

struct ApngFrameControl final {
    RectU rect;
    UINT duration_ms = kDefaultFrameDurationMs;
    DisposeOp dispose = DisposeOp::None;
    BlendOp blend = BlendOp::Source;
};

struct WebpFrame final {
    RectU rect;
    UINT duration_ms = kDefaultFrameDurationMs;
    DisposeOp dispose = DisposeOp::None;
    BlendOp blend = BlendOp::Over;
    std::vector<BYTE> webp_bytes;
};

bool RectFits(RectU rect, UINT width, UINT height)
{
    return rect.width > 0 &&
        rect.height > 0 &&
        rect.x <= width &&
        rect.y <= height &&
        rect.width <= width - rect.x &&
        rect.height <= height - rect.y;
}

UINT NormalizeDuration(UINT duration_ms)
{
    return duration_ms == 0 ? kDefaultFrameDurationMs : (std::max)(10U, duration_ms);
}

HRESULT ReadUnsignedMetadata(IWICMetadataQueryReader* reader, const wchar_t* query, UINT* number)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, reader);
    RETURN_HR_IF_NULL(E_POINTER, number);

    PROPVARIANT value;
    PropVariantInit(&value);
    const HRESULT hr = reader->GetMetadataByName(query, &value);
    if (FAILED(hr)) {
        PropVariantClear(&value);
        return hr;
    }

    bool parsed = true;
    UINT result = 0;
    switch (value.vt) {
    case VT_UI1:
        result = value.bVal;
        break;
    case VT_UI2:
        result = value.uiVal;
        break;
    case VT_UI4:
        result = value.ulVal;
        break;
    case VT_UINT:
        result = value.uintVal;
        break;
    case VT_I2:
        parsed = value.iVal >= 0;
        result = static_cast<UINT>(value.iVal);
        break;
    case VT_I4:
    case VT_INT:
        parsed = value.intVal >= 0;
        result = static_cast<UINT>(value.intVal);
        break;
    default:
        parsed = false;
        break;
    }
    PropVariantClear(&value);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), !parsed);
    *number = result;
    return S_OK;
}

HRESULT DecodeImageBytes(
    IWICImagingFactory2* wic_factory,
    std::vector<BYTE>& bytes,
    std::vector<BYTE>* bgra,
    UINT* width,
    UINT* height)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, wic_factory);
    RETURN_HR_IF_NULL(E_POINTER, bgra);
    RETURN_HR_IF_NULL(E_POINTER, width);
    RETURN_HR_IF_NULL(E_POINTER, height);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), bytes.empty());
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE), bytes.size() > (std::numeric_limits<DWORD>::max)());

    wil::com_ptr<IWICStream> stream;
    RETURN_IF_FAILED(wic_factory->CreateStream(stream.put()));
    RETURN_IF_FAILED(stream->InitializeFromMemory(bytes.data(), static_cast<DWORD>(bytes.size())));

    wil::com_ptr<IWICBitmapDecoder> decoder;
    RETURN_IF_FAILED(wic_factory->CreateDecoderFromStream(
        stream.get(),
        nullptr,
        WICDecodeMetadataCacheOnDemand,
        decoder.put()));

    wil::com_ptr<IWICBitmapFrameDecode> frame;
    RETURN_IF_FAILED(decoder->GetFrame(0, frame.put()));
    RETURN_IF_FAILED(image_bitmap::CopySourceBgra(wic_factory, frame.get(), bgra, width, height));
    return S_OK;
}

void ClearRect(std::vector<BYTE>* canvas, UINT canvas_width, RectU rect)
{
    for (UINT y = 0; y < rect.height; ++y) {
        BYTE* row = canvas->data() + (static_cast<size_t>(rect.y + y) * canvas_width + rect.x) * 4;
        std::fill(row, row + static_cast<size_t>(rect.width) * 4, BYTE{0});
    }
}

void CopyRect(
    std::vector<BYTE>* canvas,
    UINT canvas_width,
    RectU rect,
    const std::vector<BYTE>& source)
{
    const size_t source_stride = static_cast<size_t>(rect.width) * 4;
    for (UINT y = 0; y < rect.height; ++y) {
        BYTE* destination_row = canvas->data() + (static_cast<size_t>(rect.y + y) * canvas_width + rect.x) * 4;
        const BYTE* source_row = source.data() + static_cast<size_t>(y) * source_stride;
        std::copy(source_row, source_row + source_stride, destination_row);
    }
}

void BlendRect(
    std::vector<BYTE>* canvas,
    UINT canvas_width,
    RectU rect,
    const std::vector<BYTE>& source)
{
    for (UINT y = 0; y < rect.height; ++y) {
        BYTE* destination = canvas->data() + (static_cast<size_t>(rect.y + y) * canvas_width + rect.x) * 4;
        const BYTE* source_row = source.data() + static_cast<size_t>(y) * rect.width * 4;
        for (UINT x = 0; x < rect.width; ++x) {
            const BYTE sa = source_row[x * 4 + 3];
            if (sa == 255) {
                destination[x * 4 + 0] = source_row[x * 4 + 0];
                destination[x * 4 + 1] = source_row[x * 4 + 1];
                destination[x * 4 + 2] = source_row[x * 4 + 2];
                destination[x * 4 + 3] = 255;
                continue;
            }
            if (sa == 0) {
                continue;
            }

            const UINT da = destination[x * 4 + 3];
            const UINT out_a = sa + (da * (255U - sa) + 127U) / 255U;
            if (out_a == 0) {
                destination[x * 4 + 0] = 0;
                destination[x * 4 + 1] = 0;
                destination[x * 4 + 2] = 0;
                destination[x * 4 + 3] = 0;
                continue;
            }

            for (UINT channel = 0; channel < 3; ++channel) {
                const UINT sc = source_row[x * 4 + channel];
                const UINT dc = destination[x * 4 + channel];
                const UINT premul = sc * sa + (dc * da * (255U - sa) + 127U) / 255U;
                destination[x * 4 + channel] = static_cast<BYTE>((premul + out_a / 2U) / out_a);
            }
            destination[x * 4 + 3] = static_cast<BYTE>(out_a);
        }
    }
}

void ComposeFrame(
    std::vector<BYTE>* canvas,
    UINT canvas_width,
    RectU rect,
    const std::vector<BYTE>& frame_bgra,
    BlendOp blend)
{
    if (blend == BlendOp::Source) {
        CopyRect(canvas, canvas_width, rect, frame_bgra);
        return;
    }

    BlendRect(canvas, canvas_width, rect, frame_bgra);
}

HRESULT AddCompositedFrame(
    AnimationPixels* animation,
    std::vector<BYTE>* canvas,
    UINT canvas_width,
    RectU rect,
    const std::vector<BYTE>& frame_bgra,
    UINT duration_ms,
    DisposeOp dispose,
    BlendOp blend)
{
    RETURN_HR_IF_NULL(E_POINTER, animation);
    RETURN_HR_IF_NULL(E_POINTER, canvas);

    std::vector<BYTE> previous;
    if (dispose == DisposeOp::Previous) {
        previous = *canvas;
    }

    ComposeFrame(canvas, canvas_width, rect, frame_bgra, blend);
    animation->frames.push_back(AnimationFramePixels{
        .bgra = *canvas,
        .duration_ms = NormalizeDuration(duration_ms),
    });

    if (dispose == DisposeOp::Background) {
        ClearRect(canvas, canvas_width, rect);
    } else if (dispose == DisposeOp::Previous) {
        *canvas = std::move(previous);
    }

    return S_OK;
}

HRESULT ParsePngChunks(const std::vector<BYTE>& bytes, std::vector<PngChunk>* chunks)
{
    RETURN_HR_IF_NULL(E_POINTER, chunks);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), bytes.size() < sizeof(kPngSignature));
    RETURN_HR_IF(
        HRESULT_FROM_WIN32(ERROR_INVALID_DATA),
        !std::equal(std::begin(kPngSignature), std::end(kPngSignature), bytes.begin()));

    size_t offset = sizeof(kPngSignature);
    while (offset + 12 <= bytes.size()) {
        const UINT data_size = image_utils::ReadBe32(bytes.data() + offset);
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), data_size > bytes.size() - offset - 12);

        PngChunk chunk;
        chunk.type[0] = static_cast<char>(bytes[offset + 4]);
        chunk.type[1] = static_cast<char>(bytes[offset + 5]);
        chunk.type[2] = static_cast<char>(bytes[offset + 6]);
        chunk.type[3] = static_cast<char>(bytes[offset + 7]);
        chunk.data_offset = offset + 8;
        chunk.data_size = data_size;
        chunks->push_back(chunk);

        offset += 12 + data_size;
        if (image_utils::IsFourCC(chunk.type, "IEND")) {
            return S_OK;
        }
    }

    return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
}

UINT Crc32(const BYTE* data, size_t size)
{
    UINT crc = 0xffffffffU;
    for (size_t index = 0; index < size; ++index) {
        crc ^= data[index];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
        }
    }
    return crc ^ 0xffffffffU;
}

void AppendPngChunk(std::vector<BYTE>* png, const char type[4], const BYTE* data, UINT data_size)
{
    image_utils::AppendBe32(png, data_size);
    const size_t type_offset = png->size();
    png->insert(png->end(), type, type + 4);
    if (data_size > 0) {
        png->insert(png->end(), data, data + data_size);
    }
    image_utils::AppendBe32(png, Crc32(png->data() + type_offset, static_cast<size_t>(data_size) + 4));
}

HRESULT MakeFramePng(
    const std::vector<BYTE>& source,
    const std::vector<PngChunk>& chunks,
    const ApngFrameControl& control,
    const std::vector<std::vector<BYTE>>& idat_payloads,
    std::vector<BYTE>* png)
{
    RETURN_HR_IF_NULL(E_POINTER, png);
    png->assign(std::begin(kPngSignature), std::end(kPngSignature));

    const PngChunk* ihdr = nullptr;
    for (const PngChunk& chunk : chunks) {
        if (image_utils::IsFourCC(chunk.type, "IHDR")) {
            ihdr = &chunk;
            break;
        }
    }
    RETURN_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), ihdr);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), ihdr->data_size != 13);

    BYTE ihdr_data[13] = {};
    std::copy(
        source.data() + ihdr->data_offset,
        source.data() + ihdr->data_offset + ihdr->data_size,
        ihdr_data);
    ihdr_data[0] = static_cast<BYTE>((control.rect.width >> 24) & 0xff);
    ihdr_data[1] = static_cast<BYTE>((control.rect.width >> 16) & 0xff);
    ihdr_data[2] = static_cast<BYTE>((control.rect.width >> 8) & 0xff);
    ihdr_data[3] = static_cast<BYTE>(control.rect.width & 0xff);
    ihdr_data[4] = static_cast<BYTE>((control.rect.height >> 24) & 0xff);
    ihdr_data[5] = static_cast<BYTE>((control.rect.height >> 16) & 0xff);
    ihdr_data[6] = static_cast<BYTE>((control.rect.height >> 8) & 0xff);
    ihdr_data[7] = static_cast<BYTE>(control.rect.height & 0xff);
    AppendPngChunk(png, "IHDR", ihdr_data, 13);

    for (const PngChunk& chunk : chunks) {
        if (image_utils::IsFourCC(chunk.type, "IHDR") || image_utils::IsFourCC(chunk.type, "IDAT") || image_utils::IsFourCC(chunk.type, "IEND") ||
            image_utils::IsFourCC(chunk.type, "acTL") || image_utils::IsFourCC(chunk.type, "fcTL") || image_utils::IsFourCC(chunk.type, "fdAT")) {
            continue;
        }
        AppendPngChunk(png, chunk.type, source.data() + chunk.data_offset, chunk.data_size);
    }

    for (const std::vector<BYTE>& payload : idat_payloads) {
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE), payload.size() > (std::numeric_limits<UINT>::max)());
        AppendPngChunk(png, "IDAT", payload.data(), static_cast<UINT>(payload.size()));
    }
    AppendPngChunk(png, "IEND", nullptr, 0);
    return S_OK;
}

HRESULT DecodeWicFrameRect(
    IWICBitmapFrameDecode* frame,
    RectU fallback,
    RectU* rect,
    UINT* duration_ms,
    DisposeOp* dispose)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, frame);
    RETURN_HR_IF_NULL(E_POINTER, rect);
    RETURN_HR_IF_NULL(E_POINTER, duration_ms);
    RETURN_HR_IF_NULL(E_POINTER, dispose);

    *rect = fallback;
    *duration_ms = kDefaultFrameDurationMs;
    *dispose = DisposeOp::None;

    wil::com_ptr<IWICMetadataQueryReader> reader;
    if (FAILED(frame->GetMetadataQueryReader(reader.put()))) {
        return S_OK;
    }

    UINT value = 0;
    if (SUCCEEDED(ReadUnsignedMetadata(reader.get(), L"/imgdesc/Left", &value))) rect->x = value;
    if (SUCCEEDED(ReadUnsignedMetadata(reader.get(), L"/imgdesc/Top", &value))) rect->y = value;
    if (SUCCEEDED(ReadUnsignedMetadata(reader.get(), L"/imgdesc/Width", &value))) rect->width = value;
    if (SUCCEEDED(ReadUnsignedMetadata(reader.get(), L"/imgdesc/Height", &value))) rect->height = value;
    if (SUCCEEDED(ReadUnsignedMetadata(reader.get(), L"/grctlext/Delay", &value))) {
        *duration_ms = value * 10U;
    }
    if (SUCCEEDED(ReadUnsignedMetadata(reader.get(), L"/grctlext/Disposal", &value))) {
        if (value == 2) {
            *dispose = DisposeOp::Background;
        } else if (value == 3) {
            *dispose = DisposeOp::Previous;
        }
    }

    return S_OK;
}

bool TryReadGifLoop(IWICBitmapDecoder* decoder)
{
    if (decoder == nullptr) {
        return true;
    }

    wil::com_ptr<IWICMetadataQueryReader> reader;
    if (FAILED(decoder->GetMetadataQueryReader(reader.put()))) {
        return true;
    }

    PROPVARIANT value;
    PropVariantInit(&value);
    const HRESULT hr = reader->GetMetadataByName(L"/appext/application/NETSCAPE2.0/Data", &value);
    if (FAILED(hr)) {
        PropVariantClear(&value);
        return true;
    }

    bool loop = true;
    if (value.vt == (VT_VECTOR | VT_UI1) && value.caub.cElems >= 3 && value.caub.pElems != nullptr) {
        const UINT loop_count = static_cast<UINT>(value.caub.pElems[1]) |
            (static_cast<UINT>(value.caub.pElems[2]) << 8);
        loop = loop_count != 1;
    }
    PropVariantClear(&value);
    return loop;
}

HRESULT AppendRiffWebp(std::vector<BYTE>* webp, const BYTE* payload, size_t payload_size)
{
    RETURN_HR_IF_NULL(E_POINTER, webp);
    RETURN_HR_IF_NULL(E_INVALIDARG, payload);
    RETURN_HR_IF(
        HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE),
        payload_size + 4 > (std::numeric_limits<UINT>::max)());

    webp->clear();
    webp->insert(webp->end(), {'R', 'I', 'F', 'F'});
    image_utils::AppendLe32(webp, static_cast<UINT>(payload_size + 4));
    webp->insert(webp->end(), {'W', 'E', 'B', 'P'});
    webp->insert(webp->end(), payload, payload + payload_size);
    return S_OK;
}

} // namespace

HRESULT DecodeGifAnimationPixels(
    IWICImagingFactory2* wic_factory,
    IWICBitmapDecoder* decoder,
    AnimationPixels* animation)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, wic_factory);
    RETURN_HR_IF_NULL(E_INVALIDARG, decoder);
    RETURN_HR_IF_NULL(E_POINTER, animation);

    UINT frame_count = 0;
    RETURN_IF_FAILED(decoder->GetFrameCount(&frame_count));
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), frame_count <= 1);

    wil::com_ptr<IWICBitmapFrameDecode> first_frame;
    RETURN_IF_FAILED(decoder->GetFrame(0, first_frame.put()));
    RETURN_IF_FAILED(first_frame->GetSize(&animation->width, &animation->height));
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), animation->width == 0 || animation->height == 0);

    animation->loop = TryReadGifLoop(decoder);
    animation->frames.clear();
    std::vector<BYTE> canvas(static_cast<size_t>(animation->width) * animation->height * 4, 0);
    for (UINT index = 0; index < frame_count; ++index) {
        wil::com_ptr<IWICBitmapFrameDecode> frame;
        RETURN_IF_FAILED(decoder->GetFrame(index, frame.put()));

        RectU rect{0, 0, animation->width, animation->height};
        UINT duration_ms = kDefaultFrameDurationMs;
        DisposeOp dispose = DisposeOp::None;
        RETURN_IF_FAILED(DecodeWicFrameRect(frame.get(), rect, &rect, &duration_ms, &dispose));
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), !RectFits(rect, animation->width, animation->height));

        std::vector<BYTE> frame_bgra;
        UINT frame_width = 0;
        UINT frame_height = 0;
        RETURN_IF_FAILED(image_bitmap::CopySourceBgra(wic_factory, frame.get(), &frame_bgra, &frame_width, &frame_height));
        if (frame_width != rect.width || frame_height != rect.height) {
            rect.width = frame_width;
            rect.height = frame_height;
            RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), !RectFits(rect, animation->width, animation->height));
        }

        RETURN_IF_FAILED(AddCompositedFrame(
            animation,
            &canvas,
            animation->width,
            rect,
            frame_bgra,
            duration_ms,
            dispose,
            BlendOp::Over));
    }

    return S_OK;
}

HRESULT DecodeApngAnimationPixels(
    IWICImagingFactory2* wic_factory,
    const wchar_t* path,
    AnimationPixels* animation)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, wic_factory);
    RETURN_HR_IF_NULL(E_INVALIDARG, path);
    RETURN_HR_IF_NULL(E_POINTER, animation);

    std::vector<BYTE> bytes;
    RETURN_IF_FAILED(image_utils::ReadFileBytes(path, &bytes));

    std::vector<PngChunk> chunks;
    RETURN_IF_FAILED(ParsePngChunks(bytes, &chunks));

    const PngChunk* ihdr = nullptr;
    bool has_actl = false;
    UINT play_count = 0;
    for (const PngChunk& chunk : chunks) {
        if (image_utils::IsFourCC(chunk.type, "IHDR")) {
            ihdr = &chunk;
        } else if (image_utils::IsFourCC(chunk.type, "acTL")) {
            has_actl = true;
            RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), chunk.data_size != 8);
            play_count = image_utils::ReadBe32(bytes.data() + chunk.data_offset + 4);
        }
    }
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), !has_actl || ihdr == nullptr || ihdr->data_size != 13);

    animation->width = image_utils::ReadBe32(bytes.data() + ihdr->data_offset);
    animation->height = image_utils::ReadBe32(bytes.data() + ihdr->data_offset + 4);
    animation->loop = play_count == 0;
    animation->frames.clear();

    std::vector<BYTE> canvas(static_cast<size_t>(animation->width) * animation->height * 4, 0);
    bool collecting = false;
    ApngFrameControl control;
    std::vector<std::vector<BYTE>> idat_payloads;

    const auto flush_frame = [&]() -> HRESULT {
        if (!collecting) {
            return S_OK;
        }
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), idat_payloads.empty());
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), !RectFits(control.rect, animation->width, animation->height));

        std::vector<BYTE> frame_png;
        RETURN_IF_FAILED(MakeFramePng(bytes, chunks, control, idat_payloads, &frame_png));

        std::vector<BYTE> frame_bgra;
        UINT frame_width = 0;
        UINT frame_height = 0;
        RETURN_IF_FAILED(DecodeImageBytes(wic_factory, frame_png, &frame_bgra, &frame_width, &frame_height));
        RETURN_HR_IF(
            HRESULT_FROM_WIN32(ERROR_INVALID_DATA),
            frame_width != control.rect.width || frame_height != control.rect.height);

        RETURN_IF_FAILED(AddCompositedFrame(
            animation,
            &canvas,
            animation->width,
            control.rect,
            frame_bgra,
            control.duration_ms,
            control.dispose,
            control.blend));

        idat_payloads.clear();
        collecting = false;
        return S_OK;
    };

    for (const PngChunk& chunk : chunks) {
        if (image_utils::IsFourCC(chunk.type, "fcTL")) {
            RETURN_IF_FAILED(flush_frame());
            RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), chunk.data_size != 26);
            const BYTE* data = bytes.data() + chunk.data_offset;
            const UINT delay_num = (static_cast<UINT>(data[20]) << 8) | data[21];
            const UINT delay_den = (static_cast<UINT>(data[22]) << 8) | data[23];
            control = ApngFrameControl{
                .rect = RectU{
                    .x = image_utils::ReadBe32(data + 12),
                    .y = image_utils::ReadBe32(data + 16),
                    .width = image_utils::ReadBe32(data + 4),
                    .height = image_utils::ReadBe32(data + 8),
                },
                .duration_ms = delay_num == 0
                    ? kDefaultFrameDurationMs
                    : static_cast<UINT>((static_cast<uint64_t>(delay_num) * 1000U) / (delay_den == 0 ? 100U : delay_den)),
                .dispose = data[24] == 1 ? DisposeOp::Background : (data[24] == 2 ? DisposeOp::Previous : DisposeOp::None),
                .blend = data[25] == 1 ? BlendOp::Over : BlendOp::Source,
            };
            collecting = true;
        } else if (image_utils::IsFourCC(chunk.type, "IDAT") && collecting) {
            idat_payloads.push_back(std::vector<BYTE>(
                bytes.data() + chunk.data_offset,
                bytes.data() + chunk.data_offset + chunk.data_size));
        } else if (image_utils::IsFourCC(chunk.type, "fdAT") && collecting) {
            RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), chunk.data_size < 4);
            idat_payloads.push_back(std::vector<BYTE>(
                bytes.data() + chunk.data_offset + 4,
                bytes.data() + chunk.data_offset + chunk.data_size));
        } else if (image_utils::IsFourCC(chunk.type, "IEND")) {
            RETURN_IF_FAILED(flush_frame());
        }
    }

    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), animation->frames.size() <= 1);
    return S_OK;
}

HRESULT DecodeWebpAnimationPixels(
    IWICImagingFactory2* wic_factory,
    const wchar_t* path,
    AnimationPixels* animation)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, wic_factory);
    RETURN_HR_IF_NULL(E_INVALIDARG, path);
    RETURN_HR_IF_NULL(E_POINTER, animation);

    std::vector<BYTE> bytes;
    RETURN_IF_FAILED(image_utils::ReadFileBytes(path, &bytes));
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), bytes.size() < 12);
    RETURN_HR_IF(
        HRESULT_FROM_WIN32(ERROR_INVALID_DATA),
        bytes[0] != 'R' || bytes[1] != 'I' || bytes[2] != 'F' || bytes[3] != 'F' ||
            bytes[8] != 'W' || bytes[9] != 'E' || bytes[10] != 'B' || bytes[11] != 'P');

    animation->frames.clear();
    animation->loop = true;
    std::vector<WebpFrame> frames;

    size_t offset = 12;
    while (offset + 8 <= bytes.size()) {
        const char* type = reinterpret_cast<const char*>(bytes.data() + offset);
        const UINT chunk_size = image_utils::ReadLe32(bytes.data() + offset + 4);
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), chunk_size > bytes.size() - offset - 8);
        const BYTE* payload = bytes.data() + offset + 8;

        if (type[0] == 'V' && type[1] == 'P' && type[2] == '8' && type[3] == 'X') {
            RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), chunk_size < 10);
            animation->width = image_utils::ReadLe24(payload + 4) + 1;
            animation->height = image_utils::ReadLe24(payload + 7) + 1;
        } else if (type[0] == 'A' && type[1] == 'N' && type[2] == 'I' && type[3] == 'M') {
            RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), chunk_size < 6);
            animation->loop = (payload[4] | (static_cast<UINT>(payload[5]) << 8)) != 1;
        } else if (type[0] == 'A' && type[1] == 'N' && type[2] == 'M' && type[3] == 'F') {
            RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), chunk_size < 16);
            WebpFrame frame;
            frame.rect = RectU{
                .x = image_utils::ReadLe24(payload) * 2,
                .y = image_utils::ReadLe24(payload + 3) * 2,
                .width = image_utils::ReadLe24(payload + 6) + 1,
                .height = image_utils::ReadLe24(payload + 9) + 1,
            };
            frame.duration_ms = image_utils::ReadLe24(payload + 12);
            const BYTE flags = payload[15];
            frame.blend = (flags & 0x02) != 0 ? BlendOp::Source : BlendOp::Over;
            frame.dispose = (flags & 0x01) != 0 ? DisposeOp::Background : DisposeOp::None;
            RETURN_IF_FAILED(AppendRiffWebp(&frame.webp_bytes, payload + 16, chunk_size - 16));
            frames.push_back(std::move(frame));
        }

        offset += 8 + chunk_size + (chunk_size & 1U);
    }

    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), animation->width == 0 || animation->height == 0 || frames.size() <= 1);

    std::vector<BYTE> canvas(static_cast<size_t>(animation->width) * animation->height * 4, 0);
    for (WebpFrame& frame : frames) {
        RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), !RectFits(frame.rect, animation->width, animation->height));
        std::vector<BYTE> frame_bgra;
        UINT frame_width = 0;
        UINT frame_height = 0;
        RETURN_IF_FAILED(DecodeImageBytes(wic_factory, frame.webp_bytes, &frame_bgra, &frame_width, &frame_height));
        RETURN_HR_IF(
            HRESULT_FROM_WIN32(ERROR_INVALID_DATA),
            frame_width != frame.rect.width || frame_height != frame.rect.height);

        RETURN_IF_FAILED(AddCompositedFrame(
            animation,
            &canvas,
            animation->width,
            frame.rect,
            frame_bgra,
            frame.duration_ms,
            frame.dispose,
            frame.blend));
    }

    return S_OK;
}
