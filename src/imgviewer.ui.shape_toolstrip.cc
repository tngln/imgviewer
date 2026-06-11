#include "imgviewer.ui.shape_toolstrip.hpp"

#include <algorithm>
#include <cmath>
#include <memory>

#include <d2d1helper.h>
#include <wil/com.h>

#include "imgviewer.action.hpp"
#include "imgviewer.config.hpp"
#include "imgviewer.strings.hpp"
#include "imgviewer.ui.action.hpp"
#include "math.hpp"
#include "ui.button_behavior.hpp"
#include "ui.theme.hpp"

namespace {

constexpr float kToolbarGapAboveAnchor = 6.0f;

class ShapeKindButton final : public UiElement {
public:
    ShapeKindButton(UiElementMetadata metadata, ImgViewerShapeKind kind) : UiElement(metadata), kind_(kind)
    {
        SetFocusable(true);
    }

    ImgViewerShapeKind Kind() const { return kind_; }

    void Render(const UiDrawContext& context, UiRootState root_state) const override
    {
        const UiElementState state = VisualState(root_state);
        const D2D1_RECT_F rect = Rect();
        const UiDraw draw(context);
        draw.FillRect(rect, ui_theme::WidgetFillColor(state));

        wil::com_ptr<ID2D1SolidColorBrush> brush;
        if (context.d2d_context != nullptr &&
            SUCCEEDED(context.d2d_context->CreateSolidColorBrush(ui_theme::color::kAccent, brush.put()))) {
            const D2D1_RECT_F icon = D2D1::RectF(rect.left + 8.0f, rect.top + 8.0f, rect.right - 8.0f, rect.bottom - 8.0f);
            switch (kind_) {
            case ImgViewerShapeKind::Rectangle:
                context.d2d_context->DrawRectangle(icon, brush.get(), 2.0f);
                break;
            case ImgViewerShapeKind::Ellipse:
                context.d2d_context->DrawEllipse(
                    D2D1::Ellipse(
                        D2D1::Point2F((icon.left + icon.right) * 0.5f, (icon.top + icon.bottom) * 0.5f),
                        (icon.right - icon.left) * 0.5f,
                        (icon.bottom - icon.top) * 0.5f),
                    brush.get(),
                    2.0f);
                break;
            case ImgViewerShapeKind::Line:
                context.d2d_context->DrawLine(
                    D2D1::Point2F(icon.left, icon.bottom),
                    D2D1::Point2F(icon.right, icon.top),
                    brush.get(),
                    2.0f);
                break;
            case ImgViewerShapeKind::Arrow:
                context.d2d_context->DrawLine(
                    D2D1::Point2F(icon.left, icon.bottom),
                    D2D1::Point2F(icon.right, icon.top),
                    brush.get(),
                    2.0f);
                context.d2d_context->DrawLine(
                    D2D1::Point2F(icon.right, icon.top),
                    D2D1::Point2F(icon.right - 7.0f, icon.top + 1.0f),
                    brush.get(),
                    2.0f);
                context.d2d_context->DrawLine(
                    D2D1::Point2F(icon.right, icon.top),
                    D2D1::Point2F(icon.right - 1.0f, icon.top + 7.0f),
                    brush.get(),
                    2.0f);
                break;
            }
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
    ImgViewerShapeKind kind_ = ImgViewerShapeKind::Rectangle;
};

class ShapeColorButton final : public UiElement {
public:
    ShapeColorButton(UiElementMetadata metadata, D2D1_COLOR_F color) : UiElement(metadata), color_(color)
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

struct ButtonSpec final {
    ImgViewerUiShapeToolstrip::ButtonKey button = ImgViewerUiShapeToolstrip::ButtonKey::Rectangle;
    ImgViewerAction action = ImgViewerAction::None;
    ImgViewerStringId name = ImgViewerStringId::Empty;
    ImgViewerStringId tooltip = ImgViewerStringId::Empty;
    const wchar_t* automation_id = L"";
    ImgViewerShapeKind kind = ImgViewerShapeKind::Rectangle;
    D2D1_COLOR_F color = {};
    bool is_color = false;
};

const std::array<ButtonSpec, ImgViewerUiShapeToolstrip::kButtonCount> kButtonSpecs{{
    {ImgViewerUiShapeToolstrip::ButtonKey::Rectangle, ImgViewerAction::EditShapeRectangle,
        ImgViewerStringId::Rectangle, ImgViewerStringId::RectangleShape, L"edit-shape-rectangle", ImgViewerShapeKind::Rectangle},
    {ImgViewerUiShapeToolstrip::ButtonKey::Ellipse, ImgViewerAction::EditShapeEllipse,
        ImgViewerStringId::Ellipse, ImgViewerStringId::EllipseShape, L"edit-shape-ellipse", ImgViewerShapeKind::Ellipse},
    {ImgViewerUiShapeToolstrip::ButtonKey::Line, ImgViewerAction::EditShapeLine,
        ImgViewerStringId::Line, ImgViewerStringId::LineShape, L"edit-shape-line", ImgViewerShapeKind::Line},
    {ImgViewerUiShapeToolstrip::ButtonKey::Arrow, ImgViewerAction::EditShapeArrow,
        ImgViewerStringId::Arrow, ImgViewerStringId::ArrowShape, L"edit-shape-arrow", ImgViewerShapeKind::Arrow},
    {ImgViewerUiShapeToolstrip::ButtonKey::Red, ImgViewerAction::EditPenColorRed,
        ImgViewerStringId::Red, ImgViewerStringId::RedShape, L"edit-shape-red", ImgViewerShapeKind::Rectangle, D2D1::ColorF(D2D1::ColorF::Red), true},
    {ImgViewerUiShapeToolstrip::ButtonKey::Yellow, ImgViewerAction::EditPenColorYellow,
        ImgViewerStringId::Yellow, ImgViewerStringId::YellowShape, L"edit-shape-yellow", ImgViewerShapeKind::Rectangle, D2D1::ColorF(D2D1::ColorF::Yellow), true},
    {ImgViewerUiShapeToolstrip::ButtonKey::Green, ImgViewerAction::EditPenColorGreen,
        ImgViewerStringId::Green, ImgViewerStringId::GreenShape, L"edit-shape-green", ImgViewerShapeKind::Rectangle, D2D1::ColorF(D2D1::ColorF::Lime), true},
    {ImgViewerUiShapeToolstrip::ButtonKey::Cyan, ImgViewerAction::EditPenColorCyan,
        ImgViewerStringId::Cyan, ImgViewerStringId::CyanShape, L"edit-shape-cyan", ImgViewerShapeKind::Rectangle, D2D1::ColorF(D2D1::ColorF::Cyan), true},
    {ImgViewerUiShapeToolstrip::ButtonKey::Blue, ImgViewerAction::EditPenColorBlue,
        ImgViewerStringId::Blue, ImgViewerStringId::BlueShape, L"edit-shape-blue", ImgViewerShapeKind::Rectangle, D2D1::ColorF(D2D1::ColorF::DodgerBlue), true},
    {ImgViewerUiShapeToolstrip::ButtonKey::Magenta, ImgViewerAction::EditPenColorMagenta,
        ImgViewerStringId::Magenta, ImgViewerStringId::MagentaShape, L"edit-shape-magenta", ImgViewerShapeKind::Rectangle, D2D1::ColorF(D2D1::ColorF::Magenta), true},
    {ImgViewerUiShapeToolstrip::ButtonKey::White, ImgViewerAction::EditPenColorWhite,
        ImgViewerStringId::White, ImgViewerStringId::WhiteShape, L"edit-shape-white", ImgViewerShapeKind::Rectangle, D2D1::ColorF(D2D1::ColorF::White), true},
    {ImgViewerUiShapeToolstrip::ButtonKey::Black, ImgViewerAction::EditPenColorBlack,
        ImgViewerStringId::Black, ImgViewerStringId::BlackShape, L"edit-shape-black", ImgViewerShapeKind::Rectangle, D2D1::ColorF(D2D1::ColorF::Black), true},
}};

} // namespace

constexpr size_t ImgViewerUiShapeToolstrip::ButtonIndex(ButtonKey button)
{
    return static_cast<size_t>(button);
}

ImgViewerUiShapeToolstrip::ImgViewerUiShapeToolstrip(UiElement& root)
{
    toolbar_ = std::make_unique<ImgViewerFloatingToolbar>(root, ImgViewerString(ImgViewerStringId::ShapeTools), L"shape-toolstrip");

    for (const ButtonSpec& spec : kButtonSpecs) {
        ButtonInstance& button = buttons_[ButtonIndex(spec.button)];
        UiElementMetadata metadata = UiMetadata(
            UiElementRole::Button,
            UiActionFromImgViewerAction(spec.action),
            ImgViewerString(spec.name),
            ImgViewerString(spec.tooltip),
            spec.automation_id);
        std::unique_ptr<UiElement> element;
        if (spec.is_color) {
            element = std::make_unique<ShapeColorButton>(metadata, spec.color);
        } else {
            element = std::make_unique<ShapeKindButton>(metadata, spec.kind);
        }
        button.element = toolbar_->Panel()->AddItem(std::move(element), ui_theme::metrics::kToolbarButtonSize);
        button.id = button.element->Id();
    }

    SetScalePercent(scale_percent_);
    SetState(state_);
}

void ImgViewerUiShapeToolstrip::SetScalePercent(int percent)
{
    scale_percent_ = ClampToolbarScalePercent(percent);
    toolbar_->SetScalePercent(scale_percent_);
}

void ImgViewerUiShapeToolstrip::SetState(ImgViewerUiShapeToolstripState state)
{
    state_ = state;
    UpdateVisualState();
}

D2D1_RECT_F ImgViewerUiShapeToolstrip::Rect() const
{
    return toolbar_->Rect();
}

D2D1_SIZE_F ImgViewerUiShapeToolstrip::Measure(const UiDrawContext&, D2D1_SIZE_F) const
{
    if (!state_.visible) {
        return D2D1::SizeF();
    }

    return toolbar_->Measure(kButtonSpecs.size());
}

void ImgViewerUiShapeToolstrip::Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_toolbar_rect)
{
    if (!state_.visible) {
        toolbar_->ArrangeHidden();
        return;
    }

    toolbar_->ArrangeAboveAnchor(final_rect, anchor_toolbar_rect, kButtonSpecs.size(), 0.0f, 0, kToolbarGapAboveAnchor);
}

void ImgViewerUiShapeToolstrip::Render(const UiDrawContext& draw_context, UiRootState state)
{
    if (!state_.visible) {
        return;
    }

    toolbar_->RenderBackground(draw_context, ui_theme::color::kBorder, ui_theme::metrics::kStrokeWidth);
    toolbar_->Panel()->Render(draw_context, state);
}

UiEventResult ImgViewerUiShapeToolstrip::OnPointerEvent(const UiPointerEvent&)
{
    return {};
}

void ImgViewerUiShapeToolstrip::UpdateVisualState()
{
    toolbar_->Panel()->SetEnabled(state_.visible);
    for (size_t index = 0; index < kButtonSpecs.size(); ++index) {
        UiElement* element = buttons_[index].element;
        if (element == nullptr) {
            continue;
        }
        element->SetEnabled(state_.visible);
        const ButtonSpec& spec = kButtonSpecs[index];
        const bool active = spec.is_color
            ? math::NearlyEqual(state_.color, spec.color)
            : state_.kind == spec.kind;
        element->SetVisualActive(state_.visible && active);
    }
}
