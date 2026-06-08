#pragma once

#include <d2d1_1.h>
#include <d2d1helper.h>
#include <wincodec.h>

#include <cstddef>
#include <string>
#include <vector>

#include <wil/com.h>

#include "imgviewer.viewer.hpp"

enum class ImgViewerEditTool {
    Select,
    PixelSelect,
    Pen,
    Text,
    Crop,
};

struct ImgViewerEditStroke final {
    std::vector<D2D1_POINT_2F> points;
    D2D1_COLOR_F color = D2D1::ColorF(D2D1::ColorF::Red);
    float width = 4.0f;
};

struct ImgViewerTextStyle final {
    std::wstring font_family = L"Segoe UI";
    float font_size = 20.0f;
    D2D1_COLOR_F text_color = D2D1::ColorF(D2D1::ColorF::Black);
    D2D1_COLOR_F background_color = D2D1::ColorF(D2D1::ColorF::Yellow, 0.82f);
    bool has_background = true;
};

struct ImgViewerEditText final {
    D2D1_POINT_2F origin = {};
    std::wstring text;
    ImgViewerTextStyle style;
};

struct ImgViewerEditMosaic final {
    D2D1_RECT_F rect = {};
    UINT block_size = 12;
};

struct ImgViewerEditDocument final {
    wil::com_ptr<IWICBitmapSource> source;
    D2D1_SIZE_U source_size = {};
    D2D1_RECT_F crop_rect = {};
    bool has_crop = false;
    int rotation_quadrants = 0;
    std::vector<ImgViewerEditMosaic> mosaics;
    std::vector<ImgViewerEditStroke> strokes;
    std::vector<ImgViewerEditText> texts;
    bool dirty = false;
};

struct ImgViewerEditSnapshot final {
    bool active = false;
    ImgViewerEditTool tool = ImgViewerEditTool::Select;
    int rotation_quadrants = 0;
    std::vector<ImgViewerEditStroke> strokes;
    std::vector<ImgViewerEditText> texts;
    std::vector<ImgViewerEditMosaic> mosaics;
    bool drawing_stroke = false;
    ImgViewerEditStroke current_stroke;
    bool drawing_crop = false;
    D2D1_RECT_F crop_rect = {};
    D2D1_RECT_F current_crop_rect = {};
    bool drawing_pixel_selection = false;
    bool has_pixel_selection = false;
    D2D1_RECT_F pixel_selection_rect = {};
    D2D1_RECT_F current_pixel_selection_rect = {};
    bool editing_text = false;
    size_t editing_text_index = 0;
};

class ImgViewerEditController final {
public:
    bool Active() const;
    bool HasDocument() const;
    bool Dirty() const;
    bool CanUndo() const;
    bool CanRedo() const;
    ImgViewerEditTool Tool() const;
    D2D1_COLOR_F PenColor() const;
    float PenWidth() const;
    const ImgViewerTextStyle& TextStyle() const;
    bool IsEditingText() const;
    bool IsDrawing() const;
    bool HasTransientCapture() const;
    bool HasPixelSelection() const;
    ImgViewerEditSnapshot Snapshot() const;

    HRESULT Begin(IWICBitmapSource* source, D2D1_SIZE_U source_size);
    void Clear();
    void SetActive(bool active);
    void SetTool(ImgViewerEditTool tool);
    void SetPenColor(D2D1_COLOR_F color);
    void SetPenWidth(float width);
    void SetTextFontFamily(std::wstring font_family);
    void SetTextFontSize(float font_size);
    void SetTextColor(D2D1_COLOR_F color);
    void SetTextBackground(D2D1_COLOR_F color, bool has_background);
    bool RotateClockwise();
    bool Undo();
    bool Redo();
    void MarkSaved();
    void CancelTransientTool();
    void ClearPixelSelection();
    HRESULT CopySelectedPixels(IWICImagingFactory2* wic_factory, IWICBitmapSource** source) const;
    bool MosaicSelection();
    bool OnTextInput(wchar_t character);
    bool OnTextKeyDown(UINT virtual_key);

    ImgViewerEventResult OnPointerDown(D2D1_POINT_2F point, const ImgViewerSnapshot& viewer, D2D1_SIZE_U viewport_size);
    ImgViewerEventResult OnPointerMove(D2D1_POINT_2F point, const ImgViewerSnapshot& viewer, D2D1_SIZE_U viewport_size);
    ImgViewerEventResult OnPointerUp(D2D1_POINT_2F point, const ImgViewerSnapshot& viewer, D2D1_SIZE_U viewport_size);

    HRESULT ExportPngSource(IWICImagingFactory2* wic_factory, IWICBitmapSource** source) const;

private:
    enum class HistoryKind {
        Stroke,
        Text,
        RotateClockwise,
        Crop,
        Mosaic,
    };

    struct HistoryEntry final {
        HistoryKind kind = HistoryKind::Stroke;
        ImgViewerEditStroke stroke;
        ImgViewerEditText text;
        ImgViewerEditMosaic mosaic;
        D2D1_RECT_F previous_crop_rect = {};
        bool previous_has_crop = false;
        D2D1_RECT_F crop_rect = {};
    };

    bool DocumentPointFromViewportPoint(
        D2D1_POINT_2F point,
        const ImgViewerSnapshot& viewer,
        D2D1_SIZE_U viewport_size,
        D2D1_POINT_2F* document_point) const;
    void PushHistory(HistoryEntry entry);

    ImgViewerEditDocument document_;
    std::vector<HistoryEntry> undo_stack_;
    std::vector<HistoryEntry> redo_stack_;
    ImgViewerEditTool tool_ = ImgViewerEditTool::Select;
    D2D1_COLOR_F pen_color_ = D2D1::ColorF(D2D1::ColorF::Red);
    float pen_width_ = 4.0f;
    ImgViewerTextStyle text_style_;
    bool active_ = false;
    bool drawing_stroke_ = false;
    bool drawing_crop_ = false;
    bool drawing_pixel_selection_ = false;
    bool has_pixel_selection_ = false;
    bool editing_text_ = false;
    size_t editing_text_index_ = 0;
    D2D1_POINT_2F crop_start_ = {};
    D2D1_RECT_F current_crop_rect_ = {};
    D2D1_POINT_2F pixel_selection_start_ = {};
    D2D1_RECT_F pixel_selection_rect_ = {};
    D2D1_RECT_F current_pixel_selection_rect_ = {};
    ImgViewerEditStroke current_stroke_;
};
