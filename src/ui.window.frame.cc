#include "ui.window.frame.hpp"

#include <algorithm>
#include <memory>
#include <vector>

#include <d2d1helper.h>

#include "math.hpp"
#include "ui.layout.hpp"
#include "ui.text.hpp"
#include "ui.theme.hpp"

namespace {

constexpr wchar_t kMinimizeIcon[] = L"\xE921";
constexpr wchar_t kMaximizeIcon[] = L"\xE922";
constexpr wchar_t kRestoreIcon[] = L"\xE923";
constexpr wchar_t kCloseIcon[] = L"\xE8BB";

struct ButtonSpec final {
    UiWindowFrame::ButtonKey button = UiWindowFrame::ButtonKey::Minimize;
    UiAction action = kUiActionNone;
    const wchar_t* name = L"";
    const wchar_t* tooltip = L"";
    const wchar_t* automation_id = L"";
    const wchar_t* icon = L"";
    bool danger = false;
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
        Metadata(id, UiElementRole::Button, spec.action, spec.name, spec.tooltip, spec.automation_id),
        spec.icon);
}

constexpr std::array<ButtonSpec, UiWindowFrame::kButtonCount> kButtonSpecs{{
    {UiWindowFrame::ButtonKey::Minimize, kUiActionWindowMinimize, L"Minimize", L"Minimize", L"minimize", kMinimizeIcon},
    {UiWindowFrame::ButtonKey::MaximizeRestore, kUiActionWindowToggleMaximize, L"Maximize or Restore",
        L"Maximize or restore", L"maximize-restore", kMaximizeIcon},
    {UiWindowFrame::ButtonKey::Close, kUiActionWindowClose, L"Close", L"Close", L"close", kCloseIcon, true},
}};

constexpr std::array<UiWindowFrame::ButtonKey, UiWindowFrame::kButtonCount> kButtonsByPosition{
    UiWindowFrame::ButtonKey::Close,
    UiWindowFrame::ButtonKey::MaximizeRestore,
    UiWindowFrame::ButtonKey::Minimize,
};

} // namespace

constexpr size_t UiWindowFrame::ButtonIndex(ButtonKey button)
{
    return static_cast<size_t>(button);
}

UiWindowFrame::UiWindowFrame(UiElement& root, UiElementIdGenerator& ids, UiWindowFrameOptions options) :
    options_(options),
    title_text_(options.title != nullptr && options.title[0] != L'\0' ? options.title : L"")
{
    for (const ButtonSpec& spec : kButtonSpecs) {
        ButtonInstance& button = buttons_[ButtonIndex(spec.button)];
        button.id = ids.Next();
        button.element = static_cast<IconButton*>(root.AddChild(CreateButton(spec, button.id)));
    }
    Button(ButtonKey::Minimize)->SetEnabled(options_.show_minimize);
    Button(ButtonKey::MaximizeRestore)->SetEnabled(options_.show_maximize);
    Button(ButtonKey::Close)->SetEnabled(options_.show_close);
    Button(ButtonKey::Minimize)->SetHitTestVisible(options_.show_minimize);
    Button(ButtonKey::MaximizeRestore)->SetHitTestVisible(options_.show_maximize);
    Button(ButtonKey::Close)->SetHitTestVisible(options_.show_close);
}

void UiWindowFrame::Draw(const UiDrawContext& draw_context, UiRootState root_state, UiWindowFrameState frame_state)
{
    const UiDraw draw(draw_context);
    Layout(draw_context.viewport_size);
    Button(ButtonKey::MaximizeRestore)->SetIcon(frame_state.maximized ? kRestoreIcon : kMaximizeIcon);

    draw.FillRect(
        titlebar_rect_,
        D2D1::ColorF(ui_theme::color::kTitleBarBackground.r, ui_theme::color::kTitleBarBackground.g,
            ui_theme::color::kTitleBarBackground.b, ui_theme::color::kTitleBarBackgroundOpacity));
    if (options_.show_border && !frame_state.maximized) {
        draw.DrawRect(
            D2D1::RectF(
                ui_theme::metrics::kWindowBorderInset,
                ui_theme::metrics::kWindowBorderInset,
                (std::max)(ui_theme::metrics::kWindowBorderMinimum,
                    draw_context.viewport_size.width - ui_theme::metrics::kWindowBorderInset),
                (std::max)(ui_theme::metrics::kWindowBorderMinimum,
                    draw_context.viewport_size.height - ui_theme::metrics::kWindowBorderInset)),
            ui_theme::color::kBorder,
            1.0f);
    }

    const std::wstring title_text = ui_text::TruncateText(
        draw_context.dwrite_factory,
        draw_context.body_text_format,
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

    if (options_.show_minimize) {
        DrawButton(ButtonKey::Minimize, draw_context, root_state);
    }
    if (options_.show_maximize) {
        DrawButton(ButtonKey::MaximizeRestore, draw_context, root_state);
    }
    if (options_.show_close) {
        DrawButton(ButtonKey::Close, draw_context, root_state);
    }
}

bool UiWindowFrame::IsPointInCaptionDragArea(const UiElement& root, D2D1_POINT_2F point) const
{
    const UiElement* hit_element = root.HitTest(point);
    const UiElementId hit_id = hit_element != nullptr ? hit_element->Id() : UiElementId::None;
    return math::Contains(titlebar_rect_, point) && hit_id == UiElementId::None;
}

void UiWindowFrame::SetTitleText(const wchar_t* title)
{
    title_text_ = title != nullptr && title[0] != L'\0' ? title : L"";
}

D2D1_RECT_F UiWindowFrame::ButtonRect(ButtonKey button) const
{
    return Button(button)->Rect();
}

D2D1_RECT_F UiWindowFrame::TitleBarRect() const
{
    return titlebar_rect_;
}

void UiWindowFrame::Layout(D2D1_SIZE_F viewport_size)
{
    titlebar_rect_ = D2D1::RectF(0.0f, 0.0f, viewport_size.width, ui_theme::metrics::kTitleBarHeight);
    const float caption_edge_padding = ui_theme::metrics::kCaptionButtonEdgePadding;
    const D2D1_RECT_F caption_button_area = D2D1::RectF(
        titlebar_rect_.left,
        titlebar_rect_.top + caption_edge_padding,
        titlebar_rect_.right - caption_edge_padding,
        titlebar_rect_.bottom);

    std::vector<ButtonKey> visible_buttons;
    for (ButtonKey button : kButtonsByPosition) {
        if ((button == ButtonKey::Minimize && options_.show_minimize) ||
            (button == ButtonKey::MaximizeRestore && options_.show_maximize) ||
            (button == ButtonKey::Close && options_.show_close)) {
            visible_buttons.push_back(button);
        }
    }

    const std::vector<D2D1_RECT_F> caption_buttons = ui_layout::PlaceRightAlignedRow(
        caption_button_area,
        ui_theme::metrics::kCaptionButtonWidth,
        ui_theme::metrics::kTitleBarHeight - caption_edge_padding,
        visible_buttons.size());
    for (size_t index = 0; index < visible_buttons.size(); ++index) {
        Button(visible_buttons[index])->SetRect(caption_buttons[index]);
    }

    float title_right = titlebar_rect_.right - ui_theme::metrics::kTitleTextRightPadding - options_.title_right_reserved_width;
    for (ButtonKey button : visible_buttons) {
        title_right = (std::min)(title_right, Button(button)->Rect().left - ui_theme::metrics::kTitleTextRightPadding);
    }
    title_text_rect_ = D2D1::RectF(
        options_.title_left_reserved_width + ui_theme::metrics::kTitleTextLeft,
        1.0f,
        (std::max)(ui_theme::metrics::kTitleTextLeft + 1.0f, title_right),
        ui_theme::metrics::kTitleBarHeight);
}

IconButton* UiWindowFrame::Button(ButtonKey button)
{
    return buttons_[ButtonIndex(button)].element;
}

const IconButton* UiWindowFrame::Button(ButtonKey button) const
{
    return buttons_[ButtonIndex(button)].element;
}

UiElementState UiWindowFrame::ButtonState(ButtonKey button, UiRootState root_state, bool danger) const
{
    const ButtonInstance& instance = buttons_[ButtonIndex(button)];
    return UiElementState{
        .hovered = root_state.hovered == instance.id,
        .pressed = root_state.pressed == instance.id,
        .danger = danger,
        .enabled = instance.element != nullptr && instance.element->IsEnabled(),
    };
}

void UiWindowFrame::DrawButton(
    ButtonKey button,
    const UiDrawContext& draw_context,
    UiRootState root_state,
    bool danger) const
{
    const ButtonSpec& spec = kButtonSpecs[ButtonIndex(button)];
    Button(button)->Draw(draw_context, ButtonState(button, root_state, danger || spec.danger));
}
