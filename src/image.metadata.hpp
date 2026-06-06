#pragma once

#include <string>
#include <vector>

#include <wincodec.h>

struct ImageMetadataRow final {
    std::wstring label;
    std::wstring value;
};

struct ImageMetadata final {
    std::vector<ImageMetadataRow> exif_rows;
};

HRESULT ReadImageExifMetadata(IWICBitmapFrameDecode* frame, ImageMetadata* metadata);
