#include "imgviewer.ui.info_panel.hpp"

#include <algorithm>
#include <array>
#include <cwchar>
#include <memory>
#include <utility>

#include <d2d1helper.h>

#include "math.hpp"
#include "ui.events.hpp"
#include "ui.theme.hpp"

namespace {

constexpr float kPanelWidth = 320.0f;
constexpr float kPanelMargin = 12.0f;
constexpr float kPanelTopPadding = 18.0f;
constexpr float kPanelSidePadding = 18.0f;
constexpr float kHeaderHeight = 30.0f;
constexpr float kRowHeight = 48.0f;
constexpr float kLabelHeight = 18.0f;
constexpr float kValueTopOffset = 18.0f;

UiElementMetadata Metadata(
    UiElementId id,
    UiElementRole role,
    const wchar_t* name,
    const wchar_t* automation_id,
    bool is_control = true,
    bool is_content = true)
{
    return UiElementMetadata{
        .id = id,
        .role = role,
        .name = name,
        .automation_id = automation_id,
        .is_control = is_control,
        .is_content = is_content,
    };
}

D2D1_COLOR_F WithOpacity(D2D1_COLOR_F color, float opacity)
{
    color.a = opacity;
    return color;
}

} // namespace

ImgViewerUiInfoPanel::ImgViewerUiInfoPanel(UiElement& root, UiElementIdGenerator& ids)
{
    panel_id_ = ids.Next();
    panel_ = root.AddChild(std::make_unique<UiElement>(Metadata(
        panel_id_,
        UiElementRole::Pane,
        L"Info Panel",
        L"info-panel",
        false,
        true)));
    panel_->SetHitTestVisible(false);
}

void ImgViewerUiInfoPanel::SetState(ImgViewerUiInfoPanelState state)
{
    state_ = std::move(state);
    if (panel_ != nullptr) {
        panel_->SetHitTestVisible(state_.visible);
    }
}

bool ImgViewerUiInfoPanel::IsVisible() const
{
    return state_.visible;
}

void ImgViewerUiInfoPanel::Draw(const UiDrawContext& draw_context) const
{
    if (!state_.visible || panel_ == nullptr) {
        return;
    }

    Layout(draw_context.viewport_size);

    const UiDraw draw(draw_context);
    const D2D1_RECT_F rect = panel_->Rect();
    draw.FillRoundedRect(
        D2D1::RoundedRect(rect, 8.0f, 8.0f),
        WithOpacity(ui_theme::color::kButtonDefault, 0.94f));
    draw.DrawRoundedRect(D2D1::RoundedRect(rect, 8.0f, 8.0f), ui_theme::color::kBorder);

    const D2D1_RECT_F content = D2D1::RectF(
        rect.left + kPanelSidePadding,
        rect.top + kPanelTopPadding,
        rect.right - kPanelSidePadding,
        rect.bottom - kPanelTopPadding);
    draw.DrawBodyText(
        L"Info",
        4,
        D2D1::RectF(content.left, content.top, content.right, content.top + kHeaderHeight),
        ui_theme::color::kBodyText);

    float top = content.top + kHeaderHeight + 8.0f;
    DrawRow(draw, L"Name", state_.name, top);
    top += kRowHeight;
    DrawRow(draw, L"Path", state_.path, top);
    top += kRowHeight;
    DrawRow(draw, L"Dimensions", state_.dimensions, top);
    top += kRowHeight;
    DrawRow(draw, L"Type", state_.type, top);
    top += kRowHeight;
    DrawRow(draw, L"File size", state_.file_size, top);
    top += kRowHeight;
    DrawRow(draw, L"Modified", state_.modified_time, top);
    top += kRowHeight;
    DrawRow(draw, L"Sequence", state_.sequence, top);
    top += kRowHeight;
    DrawRow(draw, L"Zoom", state_.zoom, top);
    top += kRowHeight;
    DrawRow(draw, L"Rotation", state_.rotation, top);
    top += kRowHeight;
    DrawRow(draw, L"Flips", state_.flips, top);
}

UiEventResult ImgViewerUiInfoPanel::OnPointerEvent(const UiPointerEvent& event) const
{
    if (!state_.visible || panel_ == nullptr || event.target != panel_id_) {
        return {};
    }

    return UiEventResult{.handled = true};
}

void ImgViewerUiInfoPanel::Layout(D2D1_SIZE_F viewport_size) const
{
    if (panel_ == nullptr) {
        return;
    }

    const float width = (std::min)(kPanelWidth, (std::max)(180.0f, viewport_size.width - kPanelMargin * 2.0f));
    const float left = (std::max)(kPanelMargin, viewport_size.width - width - kPanelMargin);
    const float top = ui_theme::metrics::kTitleBarHeight + kPanelMargin;
    const float bottom = (std::max)(top + 120.0f, viewport_size.height - kPanelMargin);
    panel_->SetRect(D2D1::RectF(left, top, left + width, bottom));
}

void ImgViewerUiInfoPanel::DrawRow(
    const UiDraw& draw,
    const wchar_t* label,
    const std::wstring& value,
    float top) const
{
    const D2D1_RECT_F panel_rect = panel_ != nullptr ? panel_->Rect() : D2D1::RectF();
    const D2D1_RECT_F row = D2D1::RectF(
        panel_rect.left + kPanelSidePadding,
        top,
        panel_rect.right - kPanelSidePadding,
        top + kRowHeight);
    const wchar_t* display_value = value.empty() ? L"-" : value.c_str();
    const UINT32 display_value_length = value.empty() ? 1 : static_cast<UINT32>(value.size());
    draw.DrawBodyText(
        label,
        static_cast<UINT32>(std::wcslen(label)),
        D2D1::RectF(row.left, row.top, row.right, row.top + kLabelHeight),
        ui_theme::color::kMutedText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
    draw.DrawBodyText(
        display_value,
        display_value_length,
        D2D1::RectF(row.left, row.top + kValueTopOffset, row.right, row.bottom),
        ui_theme::color::kBodyText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
