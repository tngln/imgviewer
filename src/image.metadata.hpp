#pragma once

#include <string>
#include <vector>

#include <wincodec.h>

struct ImageMetadataRow final {
    std::wstring label;
    std::wstring value;
};

struct ImageDynamicRangeInfo final {
    bool high_dynamic_range = false;
    bool has_hdr_metadata = false;
    bool tone_mapped_to_sdr = false;
    std::wstring transfer_function;
    std::wstring primaries;
};

struct ImageColorInfo final {
    UINT bits_per_channel = 0;
    UINT channel_count = 0;
    bool has_icc_profile = false;
    bool source_preserved = false;
    std::wstring pixel_format;
    std::wstring container;
    std::wstring display_path;
    std::wstring source_path;
    ImageDynamicRangeInfo dynamic_range;
};

struct ImageMetadata final {
    ImageColorInfo color_info;
    std::vector<ImageMetadataRow> color_rows;
    std::vector<ImageMetadataRow> exif_rows;
};

HRESULT ReadImageExifMetadata(IWICBitmapFrameDecode* frame, ImageMetadata* metadata);
