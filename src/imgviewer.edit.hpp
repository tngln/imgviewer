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
    Pen,
    Text,
    Crop,
};

struct ImgViewerEditStroke final {
    std::vector<D2D1_POINT_2F> points;
    D2D1_COLOR_F color = D2D1::ColorF(D2D1::ColorF::Red);
    float width = 4.0f;
};

struct ImgViewerEditText final {
    D2D1_POINT_2F origin = {};
    std::wstring text;
    D2D1_COLOR_F color = D2D1::ColorF(D2D1::ColorF::Yellow);
};

struct ImgViewerEditDocument final {
    wil::com_ptr<IWICBitmapSource> source;
    D2D1_SIZE_U source_size = {};
    D2D1_RECT_F crop_rect = {};
    bool has_crop = false;
    int rotation_quadrants = 0;
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
    bool drawing_stroke = false;
    ImgViewerEditStroke current_stroke;
    bool drawing_crop = false;
    D2D1_RECT_F crop_rect = {};
    D2D1_RECT_F current_crop_rect = {};
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
    ImgViewerEditSnapshot Snapshot() const;

    HRESULT Begin(IWICBitmapSource* source, D2D1_SIZE_U source_size);
    void Clear();
    void SetActive(bool active);
    void SetTool(ImgViewerEditTool tool);
    bool RotateClockwise();
    bool Undo();
    bool Redo();
    void MarkSaved();
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
    };

    struct HistoryEntry final {
        HistoryKind kind = HistoryKind::Stroke;
        ImgViewerEditStroke stroke;
        ImgViewerEditText text;
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
    bool active_ = false;
    bool drawing_stroke_ = false;
    bool drawing_crop_ = false;
    bool editing_text_ = false;
    size_t editing_text_index_ = 0;
    D2D1_POINT_2F crop_start_ = {};
    D2D1_RECT_F current_crop_rect_ = {};
    ImgViewerEditStroke current_stroke_;
};
