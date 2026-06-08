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
    Shape,
    Text,
    Crop,
};

enum class ImgViewerCropEdge {
    None,
    Left,
    Top,
    Right,
    Bottom,
};

enum class ImgViewerEditObjectKind {
    None,
    Stroke,
    Shape,
    Text,
    Mosaic,
};

struct ImgViewerEditObjectRef final {
    ImgViewerEditObjectKind kind = ImgViewerEditObjectKind::None;
    size_t index = 0;
};

struct ImgViewerEditStroke final {
    std::vector<D2D1_POINT_2F> points;
    D2D1_COLOR_F color = D2D1::ColorF(D2D1::ColorF::Red);
    float width = 4.0f;
};

enum class ImgViewerShapeKind {
    Rectangle,
    Ellipse,
    Line,
    Arrow,
};

struct ImgViewerEditShape final {
    ImgViewerShapeKind kind = ImgViewerShapeKind::Rectangle;
    D2D1_RECT_F rect = {};
    D2D1_POINT_2F start = {};
    D2D1_POINT_2F end = {};
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
    std::vector<ImgViewerEditShape> shapes;
    std::vector<ImgViewerEditText> texts;
    bool dirty = false;
};

struct ImgViewerEditSnapshot final {
    bool active = false;
    ImgViewerEditTool tool = ImgViewerEditTool::Select;
    int rotation_quadrants = 0;
    std::vector<ImgViewerEditStroke> strokes;
    std::vector<ImgViewerEditShape> shapes;
    std::vector<ImgViewerEditText> texts;
    std::vector<ImgViewerEditMosaic> mosaics;
    bool drawing_stroke = false;
    ImgViewerEditStroke current_stroke;
    bool drawing_shape = false;
    ImgViewerEditShape current_shape;
    bool drawing_crop = false;
    bool has_crop = false;
    D2D1_RECT_F crop_rect = {};
    D2D1_RECT_F current_crop_rect = {};
    bool has_pending_crop = false;
    D2D1_RECT_F pending_crop_rect = {};
    ImgViewerCropEdge active_crop_edge = ImgViewerCropEdge::None;
    bool dragging_crop_edge = false;
    bool drawing_pixel_selection = false;
    bool has_pixel_selection = false;
    D2D1_RECT_F pixel_selection_rect = {};
    D2D1_RECT_F current_pixel_selection_rect = {};
    bool has_selected_object = false;
    ImgViewerEditObjectRef selected_object;
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
    ImgViewerShapeKind ShapeKind() const;
    const ImgViewerTextStyle& TextStyle() const;
    bool IsEditingText() const;
    bool IsDrawing() const;
    bool HasTransientCapture() const;
    bool HasPixelSelection() const;
    bool HasCrop() const;
    D2D1_RECT_F CropRect() const;
    bool HasSelection() const;
    ImgViewerEditObjectRef SelectedObject() const;
    ImgViewerEditSnapshot Snapshot() const;

    HRESULT Begin(IWICBitmapSource* source, D2D1_SIZE_U source_size);
    void Clear();
    void SetActive(bool active);
    void SetTool(ImgViewerEditTool tool);
    void SetPenColor(D2D1_COLOR_F color);
    void SetPenWidth(float width);
    void SetShapeKind(ImgViewerShapeKind kind);
    void SetTextFontFamily(std::wstring font_family);
    void SetTextFontSize(float font_size);
    void SetTextColor(D2D1_COLOR_F color);
    void SetTextBackground(D2D1_COLOR_F color, bool has_background);
    void BeginCropSession();
    bool CommitCropSession();
    bool CancelCropSession();
    bool RotateClockwise();
    bool Undo();
    bool Redo();
    void MarkSaved();
    void CancelTransientTool();
    void CancelSelection();
    void ClearPixelSelection();
    bool DeleteSelection();
    bool BeginTextEditOnSelection();
    HRESULT CopySelectedPixels(IWICImagingFactory2* wic_factory, IWICBitmapSource** source) const;
    bool MosaicSelection();
    bool OnTextInput(wchar_t character);
    bool OnTextKeyDown(UINT virtual_key);

    ImgViewerEventResult OnPointerDown(D2D1_POINT_2F point, const ImgViewerSnapshot& viewer, D2D1_SIZE_U viewport_size);
    ImgViewerEventResult OnPointerMove(D2D1_POINT_2F point, const ImgViewerSnapshot& viewer, D2D1_SIZE_U viewport_size);
    ImgViewerEventResult OnPointerUp(D2D1_POINT_2F point, const ImgViewerSnapshot& viewer, D2D1_SIZE_U viewport_size);
    ImgViewerEventResult OnPointerDoubleClick(D2D1_POINT_2F point, const ImgViewerSnapshot& viewer, D2D1_SIZE_U viewport_size);

    HRESULT ExportPngSource(IWICImagingFactory2* wic_factory, IWICBitmapSource** source) const;

private:
    enum class HistoryKind {
        Stroke,
        Shape,
        Text,
        RotateClockwise,
        Crop,
        Mosaic,
        DeleteObject,
        MoveObject,
    };

    struct HistoryEntry final {
        HistoryKind kind = HistoryKind::Stroke;
        ImgViewerEditObjectRef object;
        ImgViewerEditStroke stroke;
        ImgViewerEditStroke after_stroke;
        ImgViewerEditShape shape;
        ImgViewerEditShape after_shape;
        ImgViewerEditText text;
        ImgViewerEditText after_text;
        ImgViewerEditMosaic mosaic;
        ImgViewerEditMosaic after_mosaic;
        D2D1_RECT_F previous_crop_rect = {};
        bool previous_has_crop = false;
        D2D1_RECT_F crop_rect = {};
    };

    bool DocumentPointFromViewportPoint(
        D2D1_POINT_2F point,
        const ImgViewerSnapshot& viewer,
        D2D1_SIZE_U viewport_size,
        D2D1_POINT_2F* document_point) const;
    float DocumentHitSlop(const ImgViewerSnapshot& viewer, D2D1_SIZE_U viewport_size) const;
    ImgViewerCropEdge CropEdgeAt(D2D1_POINT_2F document_point, float hit_slop) const;
    D2D1_RECT_F ClampCropRect(D2D1_RECT_F rect) const;
    void UpdatePendingCropEdge(D2D1_POINT_2F document_point);
    bool HitTestObject(D2D1_POINT_2F document_point, float hit_slop, ImgViewerEditObjectRef* object) const;
    bool IsValidObject(ImgViewerEditObjectRef object) const;
    bool CaptureMoveOriginal(ImgViewerEditObjectRef object);
    bool ApplyObjectOffset(ImgViewerEditObjectRef object, D2D1_POINT_2F offset);
    bool CommitObjectMove();
    void PushHistory(HistoryEntry entry);

    ImgViewerEditDocument document_;
    std::vector<HistoryEntry> undo_stack_;
    std::vector<HistoryEntry> redo_stack_;
    ImgViewerEditTool tool_ = ImgViewerEditTool::Select;
    D2D1_COLOR_F pen_color_ = D2D1::ColorF(D2D1::ColorF::Red);
    float pen_width_ = 4.0f;
    ImgViewerShapeKind shape_kind_ = ImgViewerShapeKind::Rectangle;
    ImgViewerTextStyle text_style_;
    bool active_ = false;
    bool drawing_stroke_ = false;
    bool drawing_shape_ = false;
    bool drawing_crop_ = false;
    bool has_pending_crop_ = false;
    bool dragging_crop_edge_ = false;
    bool drawing_pixel_selection_ = false;
    bool has_pixel_selection_ = false;
    bool editing_text_ = false;
    bool has_selected_object_ = false;
    bool moving_selected_object_ = false;
    size_t editing_text_index_ = 0;
    ImgViewerEditObjectRef selected_object_;
    D2D1_POINT_2F move_start_document_point_ = {};
    ImgViewerEditStroke move_original_stroke_;
    ImgViewerEditShape move_original_shape_;
    ImgViewerEditText move_original_text_;
    ImgViewerEditMosaic move_original_mosaic_;
    D2D1_POINT_2F crop_start_ = {};
    D2D1_RECT_F current_crop_rect_ = {};
    D2D1_RECT_F pending_crop_rect_ = {};
    D2D1_RECT_F crop_session_original_rect_ = {};
    bool crop_session_original_has_crop_ = false;
    ImgViewerCropEdge active_crop_edge_ = ImgViewerCropEdge::None;
    ImgViewerCropEdge dragging_crop_edge_kind_ = ImgViewerCropEdge::None;
    D2D1_POINT_2F pixel_selection_start_ = {};
    D2D1_RECT_F pixel_selection_rect_ = {};
    D2D1_RECT_F current_pixel_selection_rect_ = {};
    ImgViewerEditStroke current_stroke_;
    ImgViewerEditShape current_shape_;
};
