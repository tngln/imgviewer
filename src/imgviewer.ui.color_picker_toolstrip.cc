#include "imgviewer.ui.color_picker_toolstrip.hpp"

#include <memory>
#include <utility>

#include <d2d1helper.h>
#include <wil/com.h>

#include "imgviewer.action.hpp"
#include "imgviewer.config.hpp"
#include "imgviewer.strings.hpp"
#include "imgviewer.ui.action.hpp"
#include "math.hpp"
#include "ui.theme.hpp"

namespace {

constexpr wchar_t kCopyIcon[] = L"\xE8C8";
constexpr float kValueWidth = 84.0f;
constexpr float kToolbarGapAboveAnchor = 6.0f;
constexpr float kTextPaddingX = 8.0f;

class ReadOnlyColorValue final : public UiElement {
public:
    explicit ReadOnlyColorValue(UiElementMetadata metadata) : UiElement(metadata) {}

    void SetText(const wchar_t* text)
    {
        text_ = text != nullptr ? text : L"";
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

} // namespace

ImgViewerUiColorPickerToolstrip::ImgViewerUiColorPickerToolstrip(UiElement& root)
{
    toolbar_ = std::make_unique<ImgViewerFloatingToolbar>(root, ImgViewerString(ImgViewerStringId::ColorPickerTools), L"color-picker-toolstrip");

    auto copy_button = std::make_unique<IconButton>(
        UiMetadata(
            UiElementRole::Button,
            UiActionFromImgViewerAction(ImgViewerAction::CopyColorPickerValue),
            ImgViewerString(ImgViewerStringId::CopyColorPickerValue),
            ImgViewerString(ImgViewerStringId::CopyColorPickerValue),
            L"copy-color-picker-value"),
        kCopyIcon);
    copy_button_ = toolbar_->Panel()->AddItem(std::move(copy_button), ui_theme::metrics::kToolbarButtonSize);

    auto value = std::make_unique<ReadOnlyColorValue>(
        UiMetadata(UiElementRole::Edit, kUiActionNone, ImgViewerString(ImgViewerStringId::ColorValue), L"", L"color-picker-value"));
    value_element_ = toolbar_->Panel()->AddItem(std::move(value), kValueWidth);

    SetScalePercent(scale_percent_);
    SetState(state_);
}

void ImgViewerUiColorPickerToolstrip::SetScalePercent(int percent)
{
    scale_percent_ = ClampToolbarScalePercent(percent);
    toolbar_->SetScalePercent(scale_percent_);
    if (copy_button_ != nullptr) {
        copy_button_->SetIconScale(static_cast<float>(scale_percent_) / 100.0f);
    }
}

void ImgViewerUiColorPickerToolstrip::SetState(ImgViewerUiColorPickerToolstripState state)
{
    state_ = std::move(state);
    display_text_ = state_.has_sample && !state_.hex_text.empty() ? state_.hex_text : L"#------";
    if (auto* value = static_cast<ReadOnlyColorValue*>(value_element_)) {
        value->SetText(display_text_.c_str());
    }
    UpdateVisualState();
}

D2D1_RECT_F ImgViewerUiColorPickerToolstrip::Rect() const
{
    return toolbar_->Rect();
}

D2D1_SIZE_F ImgViewerUiColorPickerToolstrip::Measure(const UiDrawContext&, D2D1_SIZE_F) const
{
    if (!state_.visible) {
        return D2D1::SizeF();
    }

    return toolbar_->Measure(1, toolbar_->ScaledValue(kValueWidth), 1);
}

void ImgViewerUiColorPickerToolstrip::Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_toolbar_rect)
{
    if (!state_.visible) {
        toolbar_->ArrangeHidden();
        return;
    }

    toolbar_->ArrangeAboveAnchor(
        final_rect,
        anchor_toolbar_rect,
        1,
        toolbar_->ScaledValue(kValueWidth),
        1,
        kToolbarGapAboveAnchor);
}

void ImgViewerUiColorPickerToolstrip::Render(const UiDrawContext& draw_context, UiRootState state)
{
    if (!state_.visible) {
        return;
    }

    toolbar_->RenderBackground(draw_context, ui_theme::color::kBorder, ui_theme::metrics::kStrokeWidth);
    toolbar_->Panel()->Render(draw_context, state);
}

UiEventResult ImgViewerUiColorPickerToolstrip::OnPointerEvent(const UiPointerEvent&)
{
    return {};
}

bool ImgViewerUiColorPickerToolstrip::IsValueElement(UiElementId id) const
{
    return value_element_ != nullptr && value_element_->Id() == id;
}

const wchar_t* ImgViewerUiColorPickerToolstrip::ValueText() const
{
    return display_text_.c_str();
}

void ImgViewerUiColorPickerToolstrip::UpdateVisualState()
{
    toolbar_->Panel()->SetEnabled(state_.visible);
    if (value_element_ != nullptr) {
        value_element_->SetEnabled(state_.visible);
    }
    if (copy_button_ != nullptr) {
        copy_button_->SetEnabled(state_.visible && state_.has_sample);
    }
}
