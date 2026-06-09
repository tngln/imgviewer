#include "imgviewer.ui.pen_toolstrip.hpp"

#include <algorithm>
#include <cmath>
#include <memory>

#include <d2d1helper.h>
#include <wil/com.h>

#include "imgviewer.action.hpp"
#include "imgviewer.config.hpp"
#include "imgviewer.strings.hpp"
#include "imgviewer.ui.action.hpp"
#include "ui.theme.hpp"

namespace {

constexpr float kToolbarGapAboveAnchor = 6.0f;

bool SameColor(D2D1_COLOR_F left, D2D1_COLOR_F right)
{
    constexpr float kTolerance = 0.001f;
    return std::abs(left.r - right.r) < kTolerance &&
        std::abs(left.g - right.g) < kTolerance &&
        std::abs(left.b - right.b) < kTolerance &&
        std::abs(left.a - right.a) < kTolerance;
}

UiEventResult ToolButtonPointerEvent(UiElement& button, const UiPointerEvent& event)
{
    if (event.button != UiPointerButton::Left &&
        (event.type == UiEventType::PointerDown || event.type == UiEventType::PointerUp)) {
        return {};
    }

    if (event.type == UiEventType::PointerDown) {
        const bool can_activate = button.IsEnabled();
        return UiEventResult{
            .handled = true,
            .needs_render = can_activate,
            .capture = can_activate ? UiCaptureRequest::Capture : UiCaptureRequest::None,
            .focus = can_activate && button.IsFocusable() ? UiFocusRequest::FocusTarget : UiFocusRequest::None,
            .focus_target = can_activate ? button.Id() : UiElementId::None,
        };
    }

    if (event.type == UiEventType::PointerUp && event.captured == button.Id()) {
        return UiEventResult{
            .handled = true,
            .needs_render = button.IsEnabled(),
            .capture = UiCaptureRequest::Release,
            .action = button.IsEnabled() && event.target == button.Id() ? button.Action() : kUiActionNone,
        };
    }

    return {};
}

UiEventResult ToolButtonKeyEvent(UiElement& button, const UiKeyEvent& event)
{
    if (event.type != UiEventType::KeyDown || !button.IsEnabled()) {
        return {};
    }

    if (event.virtual_key != VK_RETURN && event.virtual_key != VK_SPACE) {
        return {};
    }

    return UiEventResult{
        .handled = true,
        .needs_render = true,
        .action = button.Action(),
    };
}

class PenColorButton final : public UiElement {
public:
    PenColorButton(UiElementMetadata metadata, D2D1_COLOR_F color) : UiElement(metadata), color_(color)
    {
        SetFocusable(true);
    }

    D2D1_COLOR_F Color() const { return color_; }

    void Render(const UiDrawContext& context, UiRootState root_state) const override
    {
        const UiElementState state = VisualState(root_state);
        const D2D1_RECT_F rect = Rect();
        const UiDraw draw(context);
        draw.FillRect(rect, ui_theme::WidgetFillColor(state));
        const float inset = 5.0f;
        const D2D1_RECT_F swatch = D2D1::RectF(rect.left + inset, rect.top + inset, rect.right - inset, rect.bottom - inset);
        draw.FillRoundedRect(D2D1::RoundedRect(swatch, 2.0f, 2.0f), color_);
        draw.DrawRoundedRect(
            D2D1::RoundedRect(swatch, 2.0f, 2.0f),
            color_.r + color_.g + color_.b > 2.4f ? ui_theme::color::kBorder : D2D1::ColorF(D2D1::ColorF::White, 0.7f),
            ui_theme::metrics::kStrokeWidth);
        if (state.active) {
            draw.DrawRoundedRect(
                D2D1::RoundedRect(D2D1::RectF(rect.left + 2.0f, rect.top + 2.0f, rect.right - 2.0f, rect.bottom - 2.0f), 3.0f, 3.0f),
                ui_theme::color::kAccent,
                ui_theme::metrics::kActiveStrokeWidth);
        }
    }

    UiEventResult OnPointerEvent(const UiPointerEvent& event) override
    {
        return ToolButtonPointerEvent(*this, event);
    }

    UiEventResult OnKeyEvent(const UiKeyEvent& event) override
    {
        return ToolButtonKeyEvent(*this, event);
    }

private:
    D2D1_COLOR_F color_ = D2D1::ColorF(D2D1::ColorF::Red);
};

class PenWidthButton final : public UiElement {
public:
    PenWidthButton(UiElementMetadata metadata, float width) : UiElement(metadata), width_(width)
    {
        SetFocusable(true);
    }

    float Width() const { return width_; }

    void Render(const UiDrawContext& context, UiRootState root_state) const override
    {
        const UiElementState state = VisualState(root_state);
        const D2D1_RECT_F rect = Rect();
        const UiDraw draw(context);
        draw.FillRect(rect, ui_theme::WidgetFillColor(state));
        wil::com_ptr<ID2D1SolidColorBrush> brush;
        if (context.d2d_context != nullptr &&
            SUCCEEDED(context.d2d_context->CreateSolidColorBrush(ui_theme::color::kAccent, brush.put()))) {
            const float y = (rect.top + rect.bottom) * 0.5f;
            context.d2d_context->DrawLine(
                D2D1::Point2F(rect.left + 5.0f, y),
                D2D1::Point2F(rect.right - 5.0f, y),
                brush.get(),
                width_);
        }
        if (state.active) {
            draw.DrawRoundedRect(
                D2D1::RoundedRect(D2D1::RectF(rect.left + 2.0f, rect.top + 2.0f, rect.right - 2.0f, rect.bottom - 2.0f), 3.0f, 3.0f),
                ui_theme::color::kAccent,
                ui_theme::metrics::kActiveStrokeWidth);
        }
    }

    UiEventResult OnPointerEvent(const UiPointerEvent& event) override
    {
        return ToolButtonPointerEvent(*this, event);
    }

    UiEventResult OnKeyEvent(const UiKeyEvent& event) override
    {
        return ToolButtonKeyEvent(*this, event);
    }

private:
    float width_ = 4.0f;
};

struct ButtonSpec final {
    ImgViewerUiPenToolstrip::ButtonKey button = ImgViewerUiPenToolstrip::ButtonKey::Red;
    ImgViewerAction action = ImgViewerAction::None;
    ImgViewerStringId name = ImgViewerStringId::Empty;
    ImgViewerStringId tooltip = ImgViewerStringId::Empty;
    const wchar_t* automation_id = L"";
    D2D1_COLOR_F color = {};
    float width = 0.0f;
};

const std::array<ButtonSpec, ImgViewerUiPenToolstrip::kButtonCount> kButtonSpecs{{
    {ImgViewerUiPenToolstrip::ButtonKey::Red, ImgViewerAction::EditPenColorRed,
        ImgViewerStringId::Red, ImgViewerStringId::RedPen, L"edit-pen-red", D2D1::ColorF(D2D1::ColorF::Red), 0.0f},
    {ImgViewerUiPenToolstrip::ButtonKey::Yellow, ImgViewerAction::EditPenColorYellow,
        ImgViewerStringId::Yellow, ImgViewerStringId::YellowPen, L"edit-pen-yellow", D2D1::ColorF(D2D1::ColorF::Yellow), 0.0f},
    {ImgViewerUiPenToolstrip::ButtonKey::Green, ImgViewerAction::EditPenColorGreen,
        ImgViewerStringId::Green, ImgViewerStringId::GreenPen, L"edit-pen-green", D2D1::ColorF(D2D1::ColorF::Lime), 0.0f},
    {ImgViewerUiPenToolstrip::ButtonKey::Cyan, ImgViewerAction::EditPenColorCyan,
        ImgViewerStringId::Cyan, ImgViewerStringId::CyanPen, L"edit-pen-cyan", D2D1::ColorF(D2D1::ColorF::Cyan), 0.0f},
    {ImgViewerUiPenToolstrip::ButtonKey::Blue, ImgViewerAction::EditPenColorBlue,
        ImgViewerStringId::Blue, ImgViewerStringId::BluePen, L"edit-pen-blue", D2D1::ColorF(D2D1::ColorF::DodgerBlue), 0.0f},
    {ImgViewerUiPenToolstrip::ButtonKey::Magenta, ImgViewerAction::EditPenColorMagenta,
        ImgViewerStringId::Magenta, ImgViewerStringId::MagentaPen, L"edit-pen-magenta", D2D1::ColorF(D2D1::ColorF::Magenta), 0.0f},
    {ImgViewerUiPenToolstrip::ButtonKey::White, ImgViewerAction::EditPenColorWhite,
        ImgViewerStringId::White, ImgViewerStringId::WhitePen, L"edit-pen-white", D2D1::ColorF(D2D1::ColorF::White), 0.0f},
    {ImgViewerUiPenToolstrip::ButtonKey::Black, ImgViewerAction::EditPenColorBlack,
        ImgViewerStringId::Black, ImgViewerStringId::BlackPen, L"edit-pen-black", D2D1::ColorF(D2D1::ColorF::Black), 0.0f},
    {ImgViewerUiPenToolstrip::ButtonKey::Width2, ImgViewerAction::EditPenWidth2,
        ImgViewerStringId::PenWidth2, ImgViewerStringId::PenWidth2, L"edit-pen-width-2", {}, 2.0f},
    {ImgViewerUiPenToolstrip::ButtonKey::Width4, ImgViewerAction::EditPenWidth4,
        ImgViewerStringId::PenWidth4, ImgViewerStringId::PenWidth4, L"edit-pen-width-4", {}, 4.0f},
    {ImgViewerUiPenToolstrip::ButtonKey::Width8, ImgViewerAction::EditPenWidth8,
        ImgViewerStringId::PenWidth8, ImgViewerStringId::PenWidth8, L"edit-pen-width-8", {}, 8.0f},
    {ImgViewerUiPenToolstrip::ButtonKey::Width12, ImgViewerAction::EditPenWidth12,
        ImgViewerStringId::PenWidth12, ImgViewerStringId::PenWidth12, L"edit-pen-width-12", {}, 12.0f},
}};

} // namespace

constexpr size_t ImgViewerUiPenToolstrip::ButtonIndex(ButtonKey button)
{
    return static_cast<size_t>(button);
}

ImgViewerUiPenToolstrip::ImgViewerUiPenToolstrip(UiElement& root)
{
    toolbar_ = std::make_unique<ImgViewerFloatingToolbar>(root, ImgViewerString(ImgViewerStringId::PenTools), L"pen-toolstrip");

    for (const ButtonSpec& spec : kButtonSpecs) {
        ButtonInstance& button = buttons_[ButtonIndex(spec.button)];
        UiElementMetadata metadata = UiMetadata(
            UiElementRole::Button,
            UiActionFromImgViewerAction(spec.action),
            ImgViewerString(spec.name),
            ImgViewerString(spec.tooltip),
            spec.automation_id);
        std::unique_ptr<UiElement> element;
        if (spec.width > 0.0f) {
            element = std::make_unique<PenWidthButton>(metadata, spec.width);
        } else {
            element = std::make_unique<PenColorButton>(metadata, spec.color);
        }
        button.element = toolbar_->Panel()->AddItem(std::move(element), ui_theme::metrics::kToolbarButtonSize);
        button.id = button.element->Id();
    }

    SetScalePercent(scale_percent_);
    SetState(state_);
}

void ImgViewerUiPenToolstrip::SetScalePercent(int percent)
{
    scale_percent_ = ClampToolbarScalePercent(percent);
    toolbar_->SetScalePercent(scale_percent_);
}

void ImgViewerUiPenToolstrip::SetState(ImgViewerUiPenToolstripState state)
{
    state_ = state;
    UpdateVisualState();
}

D2D1_RECT_F ImgViewerUiPenToolstrip::Rect() const
{
    return toolbar_->Rect();
}

D2D1_SIZE_F ImgViewerUiPenToolstrip::Measure(const UiDrawContext&, D2D1_SIZE_F) const
{
    if (!state_.visible) {
        return D2D1::SizeF();
    }

    return toolbar_->Measure(kButtonSpecs.size());
}

void ImgViewerUiPenToolstrip::Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_toolbar_rect)
{
    if (!state_.visible) {
        toolbar_->ArrangeHidden();
        return;
    }

    toolbar_->ArrangeAboveAnchor(final_rect, anchor_toolbar_rect, kButtonSpecs.size(), 0.0f, 0, kToolbarGapAboveAnchor);
}

void ImgViewerUiPenToolstrip::Render(const UiDrawContext& draw_context, UiRootState state)
{
    if (!state_.visible) {
        return;
    }

    toolbar_->RenderBackground(draw_context, ui_theme::color::kBorder, ui_theme::metrics::kStrokeWidth);
    toolbar_->Panel()->Render(draw_context, state);
}

UiEventResult ImgViewerUiPenToolstrip::OnPointerEvent(const UiPointerEvent&)
{
    return {};
}

void ImgViewerUiPenToolstrip::UpdateVisualState()
{
    toolbar_->Panel()->SetEnabled(state_.visible);
    for (size_t index = 0; index < kButtonSpecs.size(); ++index) {
        UiElement* element = buttons_[index].element;
        if (element == nullptr) {
            continue;
        }
        element->SetEnabled(state_.visible);
        const ButtonSpec& spec = kButtonSpecs[index];
        const bool active = spec.width > 0.0f
            ? std::abs(state_.width - spec.width) < 0.01f
            : SameColor(state_.color, spec.color);
        element->SetVisualActive(state_.visible && active);
    }
}
