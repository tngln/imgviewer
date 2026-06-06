#include "imgviewer.ui.titlebar.hpp"

#include <array>
#include <memory>

#include <d2d1helper.h>

#include "imgviewer.ui.action.hpp"
#include "ui.theme.hpp"

namespace {

constexpr wchar_t kTopMostIcon[] = L"\xE718";
constexpr wchar_t kMenuIcon[] = L"\xE700";

struct ButtonSpec final {
    ImgViewerUiTitleBar::ButtonKey button = ImgViewerUiTitleBar::ButtonKey::Menu;
    ImgViewerAction action = ImgViewerAction::None;
    const wchar_t* name = L"";
    const wchar_t* tooltip = L"";
    const wchar_t* automation_id = L"";
    const wchar_t* icon = L"";
};

UiElementMetadata Metadata(
    UiElementId id,
    UiElementRole role,
    UiAction action,
    const wchar_t* name,
    const wchar_t* tooltip,
    const wchar_t* automation_id)
{
    return UiElementMetadata{
        .id = id,
        .role = role,
        .action = action,
        .name = name,
        .tooltip = tooltip,
        .automation_id = automation_id,
    };
}

std::unique_ptr<IconButton> CreateButton(const ButtonSpec& spec, UiElementId id)
{
    return std::make_unique<IconButton>(
        Metadata(id, UiElementRole::Button, UiActionFromImgViewerAction(spec.action), spec.name, spec.tooltip, spec.automation_id),
        spec.icon);
}

constexpr std::array<ButtonSpec, ImgViewerUiTitleBar::kButtonCount> kButtonSpecs{{
    {ImgViewerUiTitleBar::ButtonKey::Menu, ImgViewerAction::OpenMenu, L"Menu", L"Open menu", L"menu", kMenuIcon},
    {ImgViewerUiTitleBar::ButtonKey::TopMost, ImgViewerAction::ToggleTopMost, L"Top Most", L"Keep window on top",
        L"top-most", kTopMostIcon},
}};

constexpr bool ButtonSpecsMatchKeys()
{
    for (size_t index = 0; index < kButtonSpecs.size(); ++index) {
        if (static_cast<size_t>(kButtonSpecs[index].button) != index) {
            return false;
        }
    }
    return true;
}

static_assert(ButtonSpecsMatchKeys());

} // namespace

constexpr size_t ImgViewerUiTitleBar::ButtonIndex(ButtonKey button)
{
    return static_cast<size_t>(button);
}

ImgViewerUiTitleBar::ImgViewerUiTitleBar(UiElement& root, UiElementIdGenerator& ids) :
    frame_(
        root,
        ids,
        UiWindowFrameOptions{
            .title = L"ImgViewer",
            .title_left_reserved_width = ui_theme::metrics::kCaptionButtonWidth,
            .title_right_reserved_width = ui_theme::metrics::kCaptionButtonWidth,
        })
{
    for (const ButtonSpec& spec : kButtonSpecs) {
        ButtonInstance& button = buttons_[ButtonIndex(spec.button)];
        button.id = ids.Next();
        button.element = static_cast<IconButton*>(root.AddChild(CreateButton(spec, button.id)));
    }
}

void ImgViewerUiTitleBar::Draw(
    const UiDrawContext& draw_context,
    UiRootState state,
    bool top_most,
    bool maximized)
{
    frame_.Draw(draw_context, state, UiWindowFrameState{.maximized = maximized});
    Layout(draw_context.viewport_size);
    DrawButton(ButtonKey::TopMost, draw_context, state, top_most);
    DrawButton(ButtonKey::Menu, draw_context, state);
}

bool ImgViewerUiTitleBar::IsPointInCaptionDragArea(const UiElement& root, D2D1_POINT_2F point) const
{
    return frame_.IsPointInCaptionDragArea(root, point);
}

void ImgViewerUiTitleBar::SetTitleText(const wchar_t* title)
{
    frame_.SetTitleText(title != nullptr && title[0] != L'\0' ? title : L"ImgViewer");
}

void ImgViewerUiTitleBar::Layout(D2D1_SIZE_F)
{
    const float caption_edge_padding = ui_theme::metrics::kCaptionButtonEdgePadding;
    Button(ButtonKey::Menu)->SetRect(D2D1::RectF(
        0.0f,
        caption_edge_padding,
        ui_theme::metrics::kCaptionButtonWidth,
        ui_theme::metrics::kTitleBarHeight));

    const D2D1_RECT_F minimize = frame_.ButtonRect(UiWindowFrame::ButtonKey::Minimize);
    Button(ButtonKey::TopMost)->SetRect(D2D1::RectF(
        minimize.left - ui_theme::metrics::kCaptionButtonWidth,
        caption_edge_padding,
        minimize.left,
        ui_theme::metrics::kTitleBarHeight));
}

IconButton* ImgViewerUiTitleBar::Button(ButtonKey button)
{
    return buttons_[ButtonIndex(button)].element;
}

const IconButton* ImgViewerUiTitleBar::Button(ButtonKey button) const
{
    return buttons_[ButtonIndex(button)].element;
}

UiElementState ImgViewerUiTitleBar::ButtonState(ButtonKey button, UiRootState state, bool active, bool danger) const
{
    const ButtonInstance& instance = buttons_[ButtonIndex(button)];
    return UiElementState{
        .hovered = state.hovered == instance.id,
        .pressed = state.pressed == instance.id,
        .active = active,
        .danger = danger,
        .enabled = instance.element != nullptr && instance.element->IsEnabled(),
    };
}

void ImgViewerUiTitleBar::DrawButton(
    ButtonKey button,
    const UiDrawContext& draw_context,
    UiRootState state,
    bool active,
    bool danger) const
{
    Button(button)->Draw(draw_context, ButtonState(button, state, active, danger));
}
