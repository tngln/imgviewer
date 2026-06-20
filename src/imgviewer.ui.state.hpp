#pragma once

#include <string>
#include <vector>

#include <d2d1_1.h>
#include <d2d1helper.h>

#include "image.analysis.hpp"
#include "image.metadata.hpp"
#include "imgviewer.edit.hpp"

struct ImgViewerUiInfoPanelState final {
    bool visible = false;
    bool has_analysis = false;
    bool analysis_unavailable = false;
    std::wstring name;
    std::wstring path;
    std::wstring dimensions;
    std::wstring type;
    std::wstring file_size;
    std::wstring modified_time;
    std::vector<ImageMetadataRow> color_rows;
    std::vector<ImageMetadataRow> exif_rows;
    ImagePixelAnalysis analysis;
};

struct ImgViewerUiEditToolbarState final {
    bool visible = false;
    ImgViewerEditTool tool = ImgViewerEditTool::Select;
    bool dirty = false;
    bool can_undo = false;
    bool can_redo = false;
};

struct ImgViewerUiPenToolstripState final {
    bool visible = false;
    D2D1_COLOR_F color = D2D1::ColorF(D2D1::ColorF::Red);
    float width = 4.0f;
};

struct ImgViewerUiShapeToolstripState final {
    bool visible = false;
    ImgViewerShapeKind kind = ImgViewerShapeKind::Rectangle;
    D2D1_COLOR_F color = D2D1::ColorF(D2D1::ColorF::Red);
};

struct ImgViewerUiTextToolstripState final {
    bool visible = false;
    ImgViewerTextStyle style;
};

struct ImgViewerUiSelectionToolstripState final {
    bool visible = false;
};
