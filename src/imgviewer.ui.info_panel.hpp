#pragma once

#include <array>
#include <string>
#include <vector>

#include <d2d1_1.h>

#include "image.analysis.hpp"
#include "image.metadata.hpp"
#include "ui.draw.hpp"
#include "ui.element.hpp"
#include "ui.table.hpp"

class IconButton;

enum class ImgViewerHistogramChannel {
    Luma,
    Red,
    Green,
    Blue,
};

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

class ImgViewerUiInfoPanel final {
public:
    explicit ImgViewerUiInfoPanel(UiElement& root);

    void SetState(ImgViewerUiInfoPanelState state);
    bool IsVisible() const;
    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const;
    void Arrange(D2D1_RECT_F final_rect) const;
    void Render(const UiDrawContext& draw_context, UiRootState state) const;
    UiEventResult OnPointerEvent(const UiPointerEvent& event);

private:
    void DrawSectionHeader(const UiDraw& draw, const wchar_t* text, float top) const;
    void DrawHistogram(const UiDraw& draw, const D2D1_RECT_F& rect) const;
    void DrawHistogramTabs(const UiDraw& draw, const D2D1_RECT_F& rect) const;
    void DrawColorSummary(const UiDraw& draw, const D2D1_RECT_F& rect) const;
    void DrawColorChip(
        const UiDraw& draw,
        const wchar_t* label,
        ImageColorSample color,
        D2D1_RECT_F rect) const;
    bool IsHistogramChannelVisible(ImgViewerHistogramChannel channel) const;
    float BodyContentHeight() const;
    float BodyViewportHeight() const;
    float MaxScrollOffset() const;
    float EffectiveScrollOffset() const;
    float BodyTop() const;
    D2D1_RECT_F HistogramTabRect(ImgViewerHistogramChannel channel) const;
    bool ToggleHistogramChannelFromPoint(D2D1_POINT_2F point);
    bool ScrollByWheelDelta(int wheel_delta);

    UiElement* panel_ = nullptr;
    IconButton* close_button_ = nullptr;
    Table* basic_table_ = nullptr;
    Table* color_table_ = nullptr;
    Table* exif_table_ = nullptr;
    UiElementId panel_id_ = UiElementId::None;
    ImgViewerUiInfoPanelState state_;
    std::array<bool, 4> visible_histogram_channels_{true, false, false, false};
    float scroll_offset_ = 0.0f;
};
