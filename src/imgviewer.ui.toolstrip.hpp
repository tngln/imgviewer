#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <d2d1_1.h>

#include "imgviewer.edit.hpp"
#include "imgviewer.strings.hpp"
#include "imgviewer.ui.floating_toolbar.hpp"
#include "ui.button.hpp"
#include "ui.events.hpp"

namespace icons { struct PathIcon; }

enum class ToolStripItemVisual {
    ColorSwatch,
    WidthLine,
    ShapeKind,
    TextLabel,
    Icon,
    PathIcon,
};

struct ToolStripItemSpec final {
    ImgViewerAction action = ImgViewerAction::None;
    ImgViewerStringId name = ImgViewerStringId::Empty;
    ImgViewerStringId tooltip = ImgViewerStringId::Empty;
    ToolStripItemVisual visual = ToolStripItemVisual::Icon;
    int32_t arg = 0;

    D2D1_COLOR_F color = {};
    float width = 0.0f;
    ImgViewerShapeKind shape_kind = ImgViewerShapeKind::Rectangle;
    const wchar_t* label = L"";
    const wchar_t* icon = L"";
    const icons::PathIcon* path_icon = nullptr;
    bool transparent = false;
};

class ImgViewerUiToolStrip {
public:
    ImgViewerUiToolStrip(
        UiElement& root,
        const wchar_t* name,
        std::vector<ToolStripItemSpec> specs);

    StackPanel* Panel() const;
    D2D1_RECT_F Rect() const;
    float ScaledValue(float value) const;
    UiElement* Button(size_t index) const;

    void SetScalePercent(int percent);
    void SetVisible(bool visible);
    bool Visible() const;
    void SetActiveStates(const std::vector<bool>& active_states);

    D2D1_SIZE_F Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const;
    void Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_rect);
    void Render(const UiDrawContext& draw_context, UiRootState state);
    UiEventResult OnPointerEvent(const UiPointerEvent& event);

    void SetBorderColor(D2D1_COLOR_F color);
    void SetBorderStrokeWidth(float width);

    void SetExtraWidth(float extra_width);
    void SetExtraItemCount(size_t extra_item_count);

    const std::vector<ToolStripItemSpec>& Specs() const;

private:
    struct ButtonInstance final {
        UiElementId id = UiElementId::None;
        UiElement* element = nullptr;
    };

    void UpdateVisualState();

    std::unique_ptr<ImgViewerFloatingToolbar> toolbar_;
    std::vector<ButtonInstance> buttons_;
    std::vector<ToolStripItemSpec> specs_;
    bool visible_ = false;
    int scale_percent_ = 125;
    float extra_width_ = 0.0f;
    size_t extra_item_count_ = 0;
    D2D1_COLOR_F border_color_ = {};
    float border_stroke_width_ = 0.0f;
};

std::unique_ptr<UiElement> CreateToolStripButton(UiElementMetadata metadata, const ToolStripItemSpec& spec);

// State for the pure spec-table tool strips. ImgViewerUi owns the strips
// directly and computes per-item active states from these (see imgviewer.ui.cc),
// so no per-strip wrapper class is needed.
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

struct ImgViewerUiSelectionToolstripState final {
    bool visible = false;
};

std::vector<ToolStripItemSpec> BuildPenToolStripSpecs();
std::vector<ToolStripItemSpec> BuildShapeToolStripSpecs();
std::vector<ToolStripItemSpec> BuildSelectionToolStripSpecs();
