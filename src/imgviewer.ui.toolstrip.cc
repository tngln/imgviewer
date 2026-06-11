#include "imgviewer.ui.toolstrip.hpp"

#include <algorithm>
#include <cmath>
#include <memory>

#include <d2d1helper.h>
#include <wil/com.h>

#include "imgviewer.config.hpp"
#include "imgviewer.ui.action.hpp"
#include "ui.button_behavior.hpp"
#include "ui.draw.hpp"
#include "ui.theme.hpp"

namespace {

constexpr float kToolbarGapAboveAnchor = 6.0f;

class ToolStripButton final : public UiElement {
public:
    ToolStripButton(UiElementMetadata metadata, const ToolStripItemSpec& spec) :
        UiElement(metadata),
        visual_(spec.visual),
        color_(spec.color),
        width_(spec.width),
        shape_kind_(spec.shape_kind),
        label_(spec.label),
        transparent_(spec.transparent)
    {
        SetFocusable(true);
    }

    void Render(const UiDrawContext& context, UiRootState root_state) const override
    {
        const UiElementState state = VisualState(root_state);
        const D2D1_RECT_F rect = Rect();
        const UiDraw draw(context);

        switch (visual_) {
        case ToolStripItemVisual::ColorSwatch:
            RenderColorSwatch(draw, rect, state);
            break;
        case ToolStripItemVisual::WidthLine:
            RenderWidthLine(context, draw, rect, state);
            break;
        case ToolStripItemVisual::ShapeKind:
            RenderShapeKind(context, draw, rect, state);
            break;
        case ToolStripItemVisual::TextLabel:
            RenderTextLabel(draw, rect, state);
            break;
        default:
            break;
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
    void RenderColorSwatch(const UiDraw& draw, D2D1_RECT_F rect, UiElementState state) const
    {
        draw.FillRect(rect, ui_theme::WidgetFillColor(state));
        const float inset = 5.0f;
        const D2D1_RECT_F swatch = D2D1::RectF(rect.left + inset, rect.top + inset, rect.right - inset, rect.bottom - inset);
        if (transparent_) {
            draw.FillRect(swatch, D2D1::ColorF(D2D1::ColorF::White, 0.9f));
            draw.FillRect(D2D1::RectF(swatch.left, swatch.top, (swatch.left + swatch.right) * 0.5f, (swatch.top + swatch.bottom) * 0.5f), ui_theme::color::kCheckerboardDark);
            draw.FillRect(D2D1::RectF((swatch.left + swatch.right) * 0.5f, (swatch.top + swatch.bottom) * 0.5f, swatch.right, swatch.bottom), ui_theme::color::kCheckerboardDark);
        } else {
            draw.FillRoundedRect(D2D1::RoundedRect(swatch, 2.0f, 2.0f), color_);
        }
        draw.DrawRoundedRect(
            D2D1::RoundedRect(swatch, 2.0f, 2.0f),
            transparent_ || color_.r + color_.g + color_.b > 2.4f ? ui_theme::color::kBorder : D2D1::ColorF(D2D1::ColorF::White, 0.7f),
            ui_theme::metrics::kStrokeWidth);
    }

    void RenderWidthLine(const UiDrawContext& context, const UiDraw& draw, D2D1_RECT_F rect, UiElementState state) const
    {
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
    }

    void RenderShapeKind(const UiDrawContext& context, const UiDraw& draw, D2D1_RECT_F rect, UiElementState state) const
    {
        draw.FillRect(rect, ui_theme::WidgetFillColor(state));
        wil::com_ptr<ID2D1SolidColorBrush> brush;
        if (context.d2d_context != nullptr &&
            SUCCEEDED(context.d2d_context->CreateSolidColorBrush(ui_theme::color::kAccent, brush.put()))) {
            const D2D1_RECT_F icon = D2D1::RectF(rect.left + 8.0f, rect.top + 8.0f, rect.right - 8.0f, rect.bottom - 8.0f);
            switch (shape_kind_) {
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
    }

    void RenderTextLabel(const UiDraw& draw, D2D1_RECT_F rect, UiElementState state) const
    {
        draw.FillRect(rect, ui_theme::WidgetFillColor(state));
        draw.DrawBodyText(
            label_,
            D2D1::RectF(rect.left + 3.0f, rect.top + ui_theme::metrics::kTextTopOffset, rect.right - 3.0f, rect.bottom),
            state.enabled ? ui_theme::color::kBodyText : ui_theme::color::kButtonDisabledContent,
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }

    ToolStripItemVisual visual_ = ToolStripItemVisual::ColorSwatch;
    D2D1_COLOR_F color_ = {};
    float width_ = 0.0f;
    ImgViewerShapeKind shape_kind_ = ImgViewerShapeKind::Rectangle;
    const wchar_t* label_ = L"";
    bool transparent_ = false;
};

} // namespace

std::unique_ptr<UiElement> CreateToolStripButton(UiElementMetadata metadata, const ToolStripItemSpec& spec)
{
    switch (spec.visual) {
    case ToolStripItemVisual::Icon:
        return std::make_unique<IconButton>(metadata, spec.icon);
    case ToolStripItemVisual::PathIcon:
        if (spec.path_icon != nullptr) {
            return std::make_unique<IconButton>(metadata, *spec.path_icon);
        }
        return std::make_unique<IconButton>(metadata, spec.icon);
    default:
        return std::make_unique<ToolStripButton>(metadata, spec);
    }
}

ImgViewerUiToolStrip::ImgViewerUiToolStrip(
    UiElement& root,
    const wchar_t* name,
    std::vector<ToolStripItemSpec> specs) :
    specs_(std::move(specs)),
    border_color_(ui_theme::color::kBorder),
    border_stroke_width_(ui_theme::metrics::kStrokeWidth)
{
    toolbar_ = std::make_unique<ImgViewerFloatingToolbar>(root, name);

    buttons_.reserve(specs_.size());
    for (size_t i = 0; i < specs_.size(); ++i) {
        const ToolStripItemSpec& spec = specs_[i];
        UiElementMetadata metadata = UiMetadata(
            UiElementRole::Button,
            UiAction(static_cast<int>(spec.action), spec.arg),
            ImgViewerString(spec.name),
            ImgViewerString(spec.tooltip));
        auto element = CreateToolStripButton(metadata, spec);
        ButtonInstance button;
        button.element = toolbar_->Panel()->AddItem(std::move(element), ui_theme::metrics::kToolbarButtonSize);
        button.id = button.element->Id();
        buttons_.push_back(button);
    }

    SetScalePercent(scale_percent_);
    SetVisible(false);
}

StackPanel* ImgViewerUiToolStrip::Panel() const
{
    return toolbar_->Panel();
}

D2D1_RECT_F ImgViewerUiToolStrip::Rect() const
{
    return toolbar_->Rect();
}

UiElement* ImgViewerUiToolStrip::Button(size_t index) const
{
    if (index >= buttons_.size()) {
        return nullptr;
    }
    return buttons_[index].element;
}

float ImgViewerUiToolStrip::ScaledValue(float value) const
{
    return toolbar_->ScaledValue(value);
}

void ImgViewerUiToolStrip::SetScalePercent(int percent)
{
    scale_percent_ = ClampToolbarScalePercent(percent);
    toolbar_->SetScalePercent(scale_percent_);
    const float icon_scale = static_cast<float>(scale_percent_) / 100.0f;
    for (const ButtonInstance& button : buttons_) {
        if (button.element != nullptr) {
            auto* icon_button = dynamic_cast<IconButton*>(button.element);
            if (icon_button != nullptr) {
                icon_button->SetIconScale(icon_scale);
            }
        }
    }
}

void ImgViewerUiToolStrip::SetVisible(bool visible)
{
    visible_ = visible;
    UpdateVisualState();
}

bool ImgViewerUiToolStrip::Visible() const
{
    return visible_;
}

void ImgViewerUiToolStrip::SetActiveStates(const std::vector<bool>& active_states)
{
    const size_t count = (std::min)(active_states.size(), buttons_.size());
    for (size_t i = 0; i < count; ++i) {
        if (buttons_[i].element != nullptr) {
            buttons_[i].element->SetVisualActive(visible_ && active_states[i]);
        }
    }
}

D2D1_SIZE_F ImgViewerUiToolStrip::Measure(const UiDrawContext&, D2D1_SIZE_F) const
{
    if (!visible_) {
        return D2D1::SizeF();
    }

    return toolbar_->Measure(buttons_.size(), extra_width_, extra_item_count_);
}

void ImgViewerUiToolStrip::Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_rect)
{
    if (!visible_) {
        toolbar_->ArrangeHidden();
        return;
    }

    toolbar_->ArrangeAboveAnchor(final_rect, anchor_rect, buttons_.size(), extra_width_, extra_item_count_, kToolbarGapAboveAnchor);
}

void ImgViewerUiToolStrip::Render(const UiDrawContext& draw_context, UiRootState state)
{
    if (!visible_) {
        return;
    }

    toolbar_->RenderBackground(draw_context, border_color_, border_stroke_width_);
    toolbar_->Panel()->Render(draw_context, state);
}

UiEventResult ImgViewerUiToolStrip::OnPointerEvent(const UiPointerEvent&)
{
    return {};
}

void ImgViewerUiToolStrip::SetBorderColor(D2D1_COLOR_F color)
{
    border_color_ = color;
}

void ImgViewerUiToolStrip::SetBorderStrokeWidth(float width)
{
    border_stroke_width_ = width;
}

void ImgViewerUiToolStrip::SetExtraWidth(float extra_width)
{
    extra_width_ = extra_width;
}

void ImgViewerUiToolStrip::SetExtraItemCount(size_t extra_item_count)
{
    extra_item_count_ = extra_item_count;
}

const std::vector<ToolStripItemSpec>& ImgViewerUiToolStrip::Specs() const
{
    return specs_;
}

void ImgViewerUiToolStrip::UpdateVisualState()
{
    toolbar_->Panel()->SetEnabled(visible_);
    for (const ButtonInstance& button : buttons_) {
        if (button.element != nullptr) {
            button.element->SetEnabled(visible_);
        }
    }
}
