#include "imgviewer.ui.titlebar.hpp"

#include "imgviewer.ui.hpp"

#include <algorithm>
#include <memory>
#include <vector>

#include <d2d1helper.h>

#include "math.hpp"
#include "ui.layout.hpp"
#include "ui.text.hpp"
#include "ui.theme.hpp"

namespace {

constexpr wchar_t kTopMostIcon[] = L"\xE718";
constexpr wchar_t kMenuIcon[] = L"\xE700";
constexpr wchar_t kMinimizeIcon[] = L"\xE921";
constexpr wchar_t kMaximizeIcon[] = L"\xE922";
constexpr wchar_t kRestoreIcon[] = L"\xE923";
constexpr wchar_t kCloseIcon[] = L"\xE8BB";

struct ButtonSpec final {
    ImgViewerUiTitleBar::ButtonKey button = ImgViewerUiTitleBar::ButtonKey::TopMost;
    ImgViewerAction action = ImgViewerAction::None;
    const wchar_t* name = L"";
    const wchar_t* tooltip = L"";
    const wchar_t* automation_id = L"";
    const wchar_t* icon = L"";
    bool danger = false;
};

UiElementMetadata Metadata(
    UiElementId id,
    UiElementRole role,
    ImgViewerAction action,
    const wchar_t* name,
    const wchar_t* tooltip,
    const wchar_t* automation_id,
    bool is_control = true,
    bool is_content = true)
{
    return UiElementMetadata{
        .id = id,
        .role = role,
        .action = action,
        .name = name,
        .tooltip = tooltip,
        .automation_id = automation_id,
        .is_control = is_control,
        .is_content = is_content,
    };
}

std::unique_ptr<IconButton> CreateButton(const ButtonSpec& spec, UiElementId id)
{
    return std::make_unique<IconButton>(
        Metadata(id, UiElementRole::Button, spec.action, spec.name, spec.tooltip, spec.automation_id),
        spec.icon);
}

constexpr std::array<ButtonSpec, ImgViewerUiTitleBar::kButtonCount> kButtonSpecs{{
    {ImgViewerUiTitleBar::ButtonKey::Menu, ImgViewerAction::OpenMenu, L"Menu", L"Open menu", L"menu", kMenuIcon},
    {ImgViewerUiTitleBar::ButtonKey::TopMost, ImgViewerAction::ToggleTopMost, L"Top Most", L"Keep window on top",
        L"top-most", kTopMostIcon},
    {ImgViewerUiTitleBar::ButtonKey::Minimize, ImgViewerAction::Minimize, L"Minimize", L"Minimize", L"minimize",
        kMinimizeIcon},
    {ImgViewerUiTitleBar::ButtonKey::MaximizeRestore, ImgViewerAction::ToggleMaximize, L"Maximize or Restore",
        L"Maximize or restore", L"maximize-restore", kMaximizeIcon},
    {ImgViewerUiTitleBar::ButtonKey::Close, ImgViewerAction::Close, L"Close", L"Close", L"close", kCloseIcon, true},
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

constexpr std::array<ImgViewerUiTitleBar::ButtonKey, 4> kButtonsByPosition{
    ImgViewerUiTitleBar::ButtonKey::Close,
    ImgViewerUiTitleBar::ButtonKey::MaximizeRestore,
    ImgViewerUiTitleBar::ButtonKey::Minimize,
    ImgViewerUiTitleBar::ButtonKey::TopMost,
};

} // namespace

constexpr size_t ImgViewerUiTitleBar::ButtonIndex(ButtonKey button)
{
    return static_cast<size_t>(button);
}

ImgViewerUiTitleBar::ImgViewerUiTitleBar(UiElement& root, UiElementIdGenerator& ids)
{
    for (const ButtonSpec& spec : kButtonSpecs) {
        ButtonInstance& button = buttons_[ButtonIndex(spec.button)];
        button.id = ids.Next();
        button.element = static_cast<IconButton*>(root.AddChild(CreateButton(spec, button.id)));
    }
}

void ImgViewerUiTitleBar::Draw(
    const UiDrawContext& draw_context,
    D2D1_SIZE_F viewport_size,
    IDWriteFactory* dwrite_factory,
    IDWriteTextFormat* body_text_format,
    ImgViewerUiState state,
    bool top_most,
    bool maximized)
{
    const UiDraw draw(draw_context);
    Layout(viewport_size);
    Button(ButtonKey::MaximizeRestore)->SetIcon(maximized ? kRestoreIcon : kMaximizeIcon);

    draw.FillRect(
        titlebar_rect_,
        D2D1::ColorF(ui_theme::color::kTitleBarBackground.r, ui_theme::color::kTitleBarBackground.g,
            ui_theme::color::kTitleBarBackground.b, ui_theme::color::kTitleBarBackgroundOpacity));
    if (!maximized) {
        draw.DrawRect(
            D2D1::RectF(
                ui_theme::metrics::kWindowBorderInset,
                ui_theme::metrics::kWindowBorderInset,
                (std::max)(ui_theme::metrics::kWindowBorderMinimum,
                    viewport_size.width - ui_theme::metrics::kWindowBorderInset),
                (std::max)(ui_theme::metrics::kWindowBorderMinimum,
                    viewport_size.height - ui_theme::metrics::kWindowBorderInset)),
            ui_theme::color::kBorder,
            1.0f);
    }

    const std::wstring title_text = ui_text::TruncateText(
        dwrite_factory,
        body_text_format,
        title_text_.c_str(),
        static_cast<UINT32>(title_text_.size()),
        math::RectWidth(title_text_rect_));
    draw.DrawBodyText(
        title_text.c_str(),
        static_cast<UINT32>(title_text.size()),
        title_text_rect_,
        ui_theme::color::kBodyText,
        D2D1_DRAW_TEXT_OPTIONS_CLIP | D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT,
        DWRITE_MEASURING_MODE_NATURAL);

    DrawButton(ButtonKey::TopMost, draw_context, state, top_most);
    DrawButton(ButtonKey::Menu, draw_context, state);
    DrawButton(ButtonKey::Minimize, draw_context, state);
    DrawButton(ButtonKey::MaximizeRestore, draw_context, state);
    DrawButton(ButtonKey::Close, draw_context, state);
}

bool ImgViewerUiTitleBar::IsPointInCaptionDragArea(const UiElement& root, D2D1_POINT_2F point) const
{
    const UiElement* hit_element = root.HitTest(point);
    const UiElementId hit_id = hit_element != nullptr ? hit_element->Id() : UiElementId::None;
    return math::Contains(titlebar_rect_, point) && hit_id == UiElementId::None;
}

void ImgViewerUiTitleBar::SetTitleText(const wchar_t* title)
{
    title_text_ = title != nullptr && title[0] != L'\0' ? title : L"ImgViewer";
}

void ImgViewerUiTitleBar::Layout(D2D1_SIZE_F viewport_size)
{
    titlebar_rect_ = D2D1::RectF(0.0f, 0.0f, viewport_size.width, ui_theme::metrics::kTitleBarHeight);
    const float caption_edge_padding = ui_theme::metrics::kCaptionButtonEdgePadding;
    const D2D1_RECT_F caption_button_area = D2D1::RectF(
        titlebar_rect_.left,
        titlebar_rect_.top + caption_edge_padding,
        titlebar_rect_.right - caption_edge_padding,
        titlebar_rect_.bottom);
    const std::vector<D2D1_RECT_F> caption_buttons = ui_layout::PlaceRightAlignedRow(
        caption_button_area,
        ui_theme::metrics::kCaptionButtonWidth,
        ui_theme::metrics::kTitleBarHeight - caption_edge_padding,
        kButtonsByPosition.size());
    for (size_t index = 0; index < kButtonsByPosition.size(); ++index) {
        Button(kButtonsByPosition[index])->SetRect(caption_buttons[index]);
    }

    Button(ButtonKey::Menu)->SetRect(D2D1::RectF(
        0.0f,
        titlebar_rect_.top + caption_edge_padding,
        ui_theme::metrics::kCaptionButtonWidth,
        ui_theme::metrics::kTitleBarHeight));

    title_text_rect_ = D2D1::RectF(
        ui_theme::metrics::kCaptionButtonWidth + ui_theme::metrics::kTitleTextLeft,
        1.0f,
        (std::max)(
            ui_theme::metrics::kTitleTextLeft + 1.0f,
            Button(ButtonKey::TopMost)->Rect().left - ui_theme::metrics::kTitleTextRightPadding),
        ui_theme::metrics::kTitleBarHeight);
}

IconButton* ImgViewerUiTitleBar::Button(ButtonKey button)
{
    return buttons_[ButtonIndex(button)].element;
}

const IconButton* ImgViewerUiTitleBar::Button(ButtonKey button) const
{
    return buttons_[ButtonIndex(button)].element;
}

UiElementState ImgViewerUiTitleBar::ButtonState(ButtonKey button, ImgViewerUiState state, bool active, bool danger) const
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
    ImgViewerUiState state,
    bool active,
    bool danger) const
{
    const ButtonSpec& spec = kButtonSpecs[ButtonIndex(button)];
    Button(button)->Draw(draw_context, ButtonState(button, state, active, danger || spec.danger));
}
