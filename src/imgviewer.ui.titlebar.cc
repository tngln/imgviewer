#include "imgviewer.ui.titlebar.hpp"

#include "imgviewer.ui.hpp"

#include <algorithm>
#include <memory>
#include <vector>

#include <d2d1helper.h>

#include "imgviewer.strings.hpp"
#include "imgviewer.ui.action.hpp"
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
    ImgViewerStringId name = ImgViewerStringId::Empty;
    ImgViewerStringId tooltip = ImgViewerStringId::Empty;
    const wchar_t* automation_id = L"";
    const wchar_t* icon = L"";
    bool danger = false;
};

std::unique_ptr<IconButton> CreateButton(const ButtonSpec& spec, UiElementMetadata metadata)
{
    return std::make_unique<IconButton>(
        metadata,
        spec.icon);
}

constexpr std::array<ButtonSpec, ImgViewerUiTitleBar::kButtonCount> kButtonSpecs{{
    {ImgViewerUiTitleBar::ButtonKey::Menu, ImgViewerAction::OpenMenu, ImgViewerStringId::Menu, ImgViewerStringId::OpenMenuTooltip, L"menu", kMenuIcon},
    {ImgViewerUiTitleBar::ButtonKey::TopMost, ImgViewerAction::ToggleTopMost, ImgViewerStringId::TopMost, ImgViewerStringId::KeepWindowOnTop,
        L"top-most", kTopMostIcon},
    {ImgViewerUiTitleBar::ButtonKey::Minimize, ImgViewerAction::Minimize, ImgViewerStringId::Minimize, ImgViewerStringId::Minimize, L"minimize",
        kMinimizeIcon},
    {ImgViewerUiTitleBar::ButtonKey::MaximizeRestore, ImgViewerAction::ToggleMaximize, ImgViewerStringId::MaximizeOrRestore,
        ImgViewerStringId::MaximizeOrRestore, L"maximize-restore", kMaximizeIcon},
    {ImgViewerUiTitleBar::ButtonKey::Close, ImgViewerAction::Close, ImgViewerStringId::Close, ImgViewerStringId::Close, L"close", kCloseIcon, true},
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

constexpr std::array<ImgViewerUiTitleBar::ButtonKey, 4> kCaptionButtonsLeftToRight{
    ImgViewerUiTitleBar::ButtonKey::TopMost,
    ImgViewerUiTitleBar::ButtonKey::Minimize,
    ImgViewerUiTitleBar::ButtonKey::MaximizeRestore,
    ImgViewerUiTitleBar::ButtonKey::Close,
};

} // namespace

constexpr size_t ImgViewerUiTitleBar::ButtonIndex(ButtonKey button)
{
    return static_cast<size_t>(button);
}

ImgViewerUiTitleBar::ImgViewerUiTitleBar(UiElement& root)
{
    caption_buttons_ = static_cast<StackPanel*>(root.AddChild(std::make_unique<StackPanel>(
        UiMetadata(UiElementRole::Pane, kUiActionNone, ImgViewerString(ImgViewerStringId::CaptionButtons), L"", L"caption-buttons", false, false),
        ui_layout::StackDirection::Horizontal)));
    for (const ButtonSpec& spec : kButtonSpecs) {
        ButtonInstance& button = buttons_[ButtonIndex(spec.button)];
        std::unique_ptr<IconButton> element = CreateButton(
            spec,
            UiMetadata(
                UiElementRole::Button,
                UiActionFromImgViewerAction(spec.action),
                ImgViewerString(spec.name),
                ImgViewerString(spec.tooltip),
                spec.automation_id));
        element->SetVisualDanger(spec.danger);
        button.element = spec.button == ButtonKey::Menu
            ? static_cast<IconButton*>(root.AddChild(std::move(element)))
            : caption_buttons_->AddItem(std::move(element), ui_theme::metrics::kCaptionButtonWidth);
        button.id = button.element->Id();
    }
}

D2D1_SIZE_F ImgViewerUiTitleBar::Measure(const UiDrawContext&, D2D1_SIZE_F available_size) const
{
    return D2D1::SizeF(available_size.width, ui_theme::metrics::kTitleBarHeight);
}

void ImgViewerUiTitleBar::Arrange(D2D1_RECT_F final_rect)
{
    titlebar_rect_ = final_rect;
    const float caption_edge_padding = ui_theme::metrics::kCaptionButtonEdgePadding;
    const D2D1_RECT_F caption_button_area = D2D1::RectF(
        titlebar_rect_.left,
        titlebar_rect_.top + caption_edge_padding,
        titlebar_rect_.right - caption_edge_padding,
        titlebar_rect_.bottom);
    const float caption_button_width = ui_theme::metrics::kCaptionButtonWidth * static_cast<float>(kCaptionButtonsLeftToRight.size());
    caption_buttons_->Measure(UiDrawContext{}, D2D1::SizeF(caption_button_width, ui_theme::metrics::kTitleBarHeight));
    caption_buttons_->Arrange(D2D1::RectF(
        caption_button_area.right - caption_button_width,
        caption_button_area.top,
        caption_button_area.right,
        caption_button_area.bottom));

    Button(ButtonKey::Menu)->Arrange(D2D1::RectF(
        caption_edge_padding,
        titlebar_rect_.top + caption_edge_padding,
        ui_theme::metrics::kCaptionButtonWidth + caption_edge_padding,
        ui_theme::metrics::kTitleBarHeight));

    title_text_rect_ = D2D1::RectF(
        ui_theme::metrics::kCaptionButtonWidth + ui_theme::metrics::kTitleTextLeft,
        0.5f,
        (std::max)(
            ui_theme::metrics::kTitleTextLeft + 0.5f,
            Button(ButtonKey::TopMost)->Rect().left - ui_theme::metrics::kTitleTextRightPadding),
        ui_theme::metrics::kTitleBarHeight);
}

void ImgViewerUiTitleBar::Render(
    const UiDrawContext& draw_context,
    UiRootState state,
    bool top_most,
    bool maximized,
    bool edit_mode)
{
    const UiDraw draw(draw_context);
    Button(ButtonKey::MaximizeRestore)->SetIcon(maximized ? kRestoreIcon : kMaximizeIcon);
    Button(ButtonKey::TopMost)->SetVisualActive(top_most);

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
                    draw_context.viewport_size.width - ui_theme::metrics::kWindowBorderInset),
                (std::max)(ui_theme::metrics::kWindowBorderMinimum,
                    draw_context.viewport_size.height - ui_theme::metrics::kWindowBorderInset)),
            edit_mode ? ui_theme::color::kEditModeBorder : ui_theme::color::kBorder,
            ui_theme::metrics::kStrokeWidth);
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

    RenderButton(ButtonKey::Menu, draw_context, state);
    caption_buttons_->Render(draw_context, state);
}

bool ImgViewerUiTitleBar::IsPointInCaptionDragArea(const UiElement& root, D2D1_POINT_2F point) const
{
    const UiElement* hit_element = root.HitTest(point);
    const UiElementId hit_id = hit_element != nullptr ? hit_element->Id() : UiElementId::None;
    return math::Contains(titlebar_rect_, point) && hit_id == UiElementId::None;
}

void ImgViewerUiTitleBar::SetTitleText(const wchar_t* title)
{
    title_text_ = title != nullptr && title[0] != L'\0' ? title : ImgViewerString(ImgViewerStringId::AppName);
}

IconButton* ImgViewerUiTitleBar::Button(ButtonKey button)
{
    return buttons_[ButtonIndex(button)].element;
}

const IconButton* ImgViewerUiTitleBar::Button(ButtonKey button) const
{
    return buttons_[ButtonIndex(button)].element;
}

void ImgViewerUiTitleBar::RenderButton(
    ButtonKey button,
    const UiDrawContext& draw_context,
    UiRootState state,
    bool danger)
{
    const ButtonSpec& spec = kButtonSpecs[ButtonIndex(button)];
    Button(button)->SetVisualDanger(danger || spec.danger);
    Button(button)->Render(draw_context, state);
}
