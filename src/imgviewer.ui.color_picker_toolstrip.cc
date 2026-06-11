#include "imgviewer.ui.color_picker_toolstrip.hpp"

#include <memory>
#include <utility>
#include <vector>

#include <d2d1helper.h>
#include <wil/com.h>

#include "imgviewer.action.hpp"
#include "imgviewer.strings.hpp"
#include "ui.theme.hpp"

namespace {

constexpr wchar_t kCopyIcon[] = L"\xE8C8";
constexpr float kValueWidth = 84.0f;
constexpr float kTextPaddingX = 8.0f;

class ReadOnlyColorValue final : public UiElement, public UiValueAccessible {
public:
    explicit ReadOnlyColorValue(UiElementMetadata metadata) : UiElement(metadata) {}

    void SetText(const wchar_t* text)
    {
        text_ = text != nullptr ? text : L"";
    }

    const wchar_t* AccessibilityValue() const override
    {
        return text_.c_str();
    }

    bool AccessibilityIsReadOnly() const override
    {
        return true;
    }

    void Render(const UiDrawContext& context, UiRootState root_state) const override
    {
        const UiElementState state = VisualState(root_state);
        const D2D1_RECT_F rect = Rect();
        const UiDraw draw(context);
        draw.FillRoundedRect(
            D2D1::RoundedRect(rect, ui_theme::metrics::kButtonCornerRadius, ui_theme::metrics::kButtonCornerRadius),
            ui_theme::color::kButtonDisabled);
        draw.DrawRoundedRect(
            D2D1::RoundedRect(rect, ui_theme::metrics::kButtonCornerRadius, ui_theme::metrics::kButtonCornerRadius),
            state.active ? ui_theme::color::kAccent : ui_theme::color::kBorder,
            state.active ? ui_theme::metrics::kActiveStrokeWidth : ui_theme::metrics::kStrokeWidth);

        const D2D1_RECT_F text_rect = D2D1::RectF(
            rect.left + kTextPaddingX,
            rect.top + ui_theme::metrics::kTextTopOffset,
            rect.right - kTextPaddingX,
            rect.bottom - ui_theme::metrics::kTextTopOffset);
        wil::com_ptr<ID2D1SolidColorBrush> brush;
        if (context.d2d_context != nullptr &&
            SUCCEEDED(context.d2d_context->CreateSolidColorBrush(ui_theme::color::kBodyText, brush.put()))) {
            context.d2d_context->PushAxisAlignedClip(text_rect, D2D1_ANTIALIAS_MODE_ALIASED);
            context.d2d_context->DrawTextW(
                text_.c_str(),
                static_cast<UINT32>(text_.size()),
                context.body_text_format,
                text_rect,
                brush.get());
            context.d2d_context->PopAxisAlignedClip();
        }
    }

private:
    std::wstring text_;
};

const ToolStripItemSpec kCopySpec = {
    ImgViewerAction::CopyColorPickerValue,
    ImgViewerStringId::CopyColorPickerValue,
    ImgViewerStringId::CopyColorPickerValue,
    ToolStripItemVisual::Icon,
    0,
    {},
    0.0f,
    ImgViewerShapeKind::Rectangle,
    L"",
    kCopyIcon,
};

std::vector<ToolStripItemSpec> BuildSpecs()
{
    return {kCopySpec};
}

} // namespace

ImgViewerUiColorPickerToolstrip::ImgViewerUiColorPickerToolstrip(UiElement& root)
{
    toolstrip_ = std::make_unique<ImgViewerUiToolStrip>(
        root, ImgViewerString(ImgViewerStringId::ColorPickerTools), BuildSpecs());
    toolstrip_->SetExtraWidth(kValueWidth);
    toolstrip_->SetExtraItemCount(1);

    auto value = std::make_unique<ReadOnlyColorValue>(
        UiMetadata(UiElementRole::Edit, kUiActionNone, ImgViewerString(ImgViewerStringId::ColorValue)));
    value_element_ = toolstrip_->Panel()->AddItem(std::move(value), kValueWidth);

    SetScalePercent(125);
    SetState(state_);
}

void ImgViewerUiColorPickerToolstrip::SetScalePercent(int percent)
{
    toolstrip_->SetScalePercent(percent);
}

void ImgViewerUiColorPickerToolstrip::SetState(ImgViewerUiColorPickerToolstripState state)
{
    state_ = std::move(state);
    display_text_ = state_.has_sample && !state_.hex_text.empty() ? state_.hex_text : L"#------";
    if (auto* value = static_cast<ReadOnlyColorValue*>(value_element_)) {
        value->SetText(display_text_.c_str());
    }

    toolstrip_->SetVisible(state_.visible);
    if (value_element_ != nullptr) {
        value_element_->SetEnabled(state_.visible);
    }
    if (auto* copy_button = toolstrip_->Button(0)) {
        copy_button->SetEnabled(state_.visible && state_.has_sample);
    }
}

D2D1_RECT_F ImgViewerUiColorPickerToolstrip::Rect() const
{
    return toolstrip_->Rect();
}

D2D1_SIZE_F ImgViewerUiColorPickerToolstrip::Measure(const UiDrawContext& context, D2D1_SIZE_F available_size) const
{
    return toolstrip_->Measure(context, available_size);
}

void ImgViewerUiColorPickerToolstrip::Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_toolbar_rect)
{
    toolstrip_->Arrange(final_rect, anchor_toolbar_rect);
}

void ImgViewerUiColorPickerToolstrip::Render(const UiDrawContext& draw_context, UiRootState state)
{
    toolstrip_->Render(draw_context, state);
}

UiEventResult ImgViewerUiColorPickerToolstrip::OnPointerEvent(const UiPointerEvent& event)
{
    return toolstrip_->OnPointerEvent(event);
}
