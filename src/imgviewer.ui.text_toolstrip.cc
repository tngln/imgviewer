#include "imgviewer.ui.text_toolstrip.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <d2d1helper.h>
#include <wil/com.h>
#include <wil/resource.h>

#include "imgviewer.action.hpp"
#include "imgviewer.palette.hpp"
#include "imgviewer.strings.hpp"
#include "imgviewer.ui.action.hpp"
#include "math.hpp"

namespace {

constexpr float kFontDropdownWidth = 180.0f;

const ToolStripItemSpec kSpecs[] = {
    {ImgViewerAction::EditSetTextFontSize, ImgViewerStringId::TextSize12, ImgViewerStringId::Text12PxTooltip, ToolStripItemVisual::TextLabel, PackFloat(12.0f), {}, 12.0f, ImgViewerShapeKind::Rectangle, L"12"},
    {ImgViewerAction::EditSetTextFontSize, ImgViewerStringId::TextSize16, ImgViewerStringId::Text16PxTooltip, ToolStripItemVisual::TextLabel, PackFloat(16.0f), {}, 16.0f, ImgViewerShapeKind::Rectangle, L"16"},
    {ImgViewerAction::EditSetTextFontSize, ImgViewerStringId::TextSize20, ImgViewerStringId::Text20PxTooltip, ToolStripItemVisual::TextLabel, PackFloat(20.0f), {}, 20.0f, ImgViewerShapeKind::Rectangle, L"20"},
    {ImgViewerAction::EditSetTextFontSize, ImgViewerStringId::TextSize28, ImgViewerStringId::Text28PxTooltip, ToolStripItemVisual::TextLabel, PackFloat(28.0f), {}, 28.0f, ImgViewerShapeKind::Rectangle, L"28"},
    {ImgViewerAction::EditSetTextFontSize, ImgViewerStringId::TextSize36, ImgViewerStringId::Text36PxTooltip, ToolStripItemVisual::TextLabel, PackFloat(36.0f), {}, 36.0f, ImgViewerShapeKind::Rectangle, L"36"},
    {ImgViewerAction::EditSetTextColor, ImgViewerStringId::Red, ImgViewerStringId::RedText, ToolStripItemVisual::ColorSwatch, PackColor(D2D1::ColorF(D2D1::ColorF::Red)), D2D1::ColorF(D2D1::ColorF::Red)},
    {ImgViewerAction::EditSetTextColor, ImgViewerStringId::Yellow, ImgViewerStringId::YellowText, ToolStripItemVisual::ColorSwatch, PackColor(D2D1::ColorF(D2D1::ColorF::Yellow)), D2D1::ColorF(D2D1::ColorF::Yellow)},
    {ImgViewerAction::EditSetTextColor, ImgViewerStringId::Green, ImgViewerStringId::GreenText, ToolStripItemVisual::ColorSwatch, PackColor(D2D1::ColorF(D2D1::ColorF::Lime)), D2D1::ColorF(D2D1::ColorF::Lime)},
    {ImgViewerAction::EditSetTextColor, ImgViewerStringId::Cyan, ImgViewerStringId::CyanText, ToolStripItemVisual::ColorSwatch, PackColor(D2D1::ColorF(D2D1::ColorF::Cyan)), D2D1::ColorF(D2D1::ColorF::Cyan)},
    {ImgViewerAction::EditSetTextColor, ImgViewerStringId::Blue, ImgViewerStringId::BlueText, ToolStripItemVisual::ColorSwatch, PackColor(D2D1::ColorF(D2D1::ColorF::DodgerBlue)), D2D1::ColorF(D2D1::ColorF::DodgerBlue)},
    {ImgViewerAction::EditSetTextColor, ImgViewerStringId::Magenta, ImgViewerStringId::MagentaText, ToolStripItemVisual::ColorSwatch, PackColor(D2D1::ColorF(D2D1::ColorF::Magenta)), D2D1::ColorF(D2D1::ColorF::Magenta)},
    {ImgViewerAction::EditSetTextColor, ImgViewerStringId::White, ImgViewerStringId::WhiteText, ToolStripItemVisual::ColorSwatch, PackColor(D2D1::ColorF(D2D1::ColorF::White)), D2D1::ColorF(D2D1::ColorF::White)},
    {ImgViewerAction::EditSetTextColor, ImgViewerStringId::Black, ImgViewerStringId::BlackText, ToolStripItemVisual::ColorSwatch, PackColor(D2D1::ColorF(D2D1::ColorF::Black)), D2D1::ColorF(D2D1::ColorF::Black)},
    {ImgViewerAction::EditSetTextBackground, ImgViewerStringId::None, ImgViewerStringId::TransparentTextBackground, ToolStripItemVisual::ColorSwatch, 0, {}, 0.0f, ImgViewerShapeKind::Rectangle, L"", L"", nullptr, true},
    {ImgViewerAction::EditSetTextBackground, ImgViewerStringId::Yellow, ImgViewerStringId::YellowTextBackground, ToolStripItemVisual::ColorSwatch, (PackColor(D2D1::ColorF(D2D1::ColorF::Yellow, 0.82f)) << 8) | 1, D2D1::ColorF(D2D1::ColorF::Yellow, 0.82f)},
    {ImgViewerAction::EditSetTextBackground, ImgViewerStringId::White, ImgViewerStringId::WhiteTextBackground, ToolStripItemVisual::ColorSwatch, (PackColor(D2D1::ColorF(D2D1::ColorF::White, 0.82f)) << 8) | 1, D2D1::ColorF(D2D1::ColorF::White, 0.82f)},
    {ImgViewerAction::EditSetTextBackground, ImgViewerStringId::Black, ImgViewerStringId::BlackTextBackground, ToolStripItemVisual::ColorSwatch, (PackColor(D2D1::ColorF(D2D1::ColorF::Black, 0.82f)) << 8) | 1, D2D1::ColorF(D2D1::ColorF::Black, 0.82f)},
    {ImgViewerAction::EditSetTextBackground, ImgViewerStringId::Red, ImgViewerStringId::RedTextBackground, ToolStripItemVisual::ColorSwatch, (PackColor(D2D1::ColorF(D2D1::ColorF::Red, 0.82f)) << 8) | 1, D2D1::ColorF(D2D1::ColorF::Red, 0.82f)},
    {ImgViewerAction::EditSetTextBackground, ImgViewerStringId::Blue, ImgViewerStringId::BlueTextBackground, ToolStripItemVisual::ColorSwatch, (PackColor(D2D1::ColorF(D2D1::ColorF::DodgerBlue, 0.82f)) << 8) | 1, D2D1::ColorF(D2D1::ColorF::DodgerBlue, 0.82f)},
};

std::vector<ToolStripItemSpec> BuildSpecs()
{
    return {std::begin(kSpecs), std::end(kSpecs)};
}

} // namespace

ImgViewerUiTextToolstrip::ImgViewerUiTextToolstrip(UiElement& root)
{
    toolstrip_ = std::make_unique<ImgViewerUiToolStrip>(
        root, ImgViewerString(ImgViewerStringId::TextTools), BuildSpecs());
    toolstrip_->SetExtraWidth(kFontDropdownWidth);
    toolstrip_->SetExtraItemCount(1);

    font_names_.push_back(L"Segoe UI");
    font_options_.push_back(DropdownOption{font_names_[0].c_str(), UiActionFromImgViewerAction(ImgViewerAction::EditTextFontChanged)});
    font_dropdown_ = static_cast<Dropdown*>(toolstrip_->Panel()->AddItem(std::make_unique<Dropdown>(
        UiMetadata(
            UiElementRole::ComboBox,
            UiActionFromImgViewerAction(ImgViewerAction::EditTextFontChanged),
            ImgViewerString(ImgViewerStringId::Font),
            ImgViewerString(ImgViewerStringId::TextFont)),
        font_options_)));

    SetScalePercent(125);
    SetState(state_);
}

void ImgViewerUiTextToolstrip::SetScalePercent(int percent)
{
    toolstrip_->SetScalePercent(percent);
}

void ImgViewerUiTextToolstrip::SetState(ImgViewerUiTextToolstripState state)
{
    state_ = std::move(state);
    selected_font_family_ = state_.style.font_family.empty() ? L"Segoe UI" : state_.style.font_family;

    toolstrip_->SetVisible(state_.visible);
    if (font_dropdown_ != nullptr) {
        font_dropdown_->SetEnabled(state_.visible);
    }

    const auto& specs = toolstrip_->Specs();
    std::vector<bool> active(specs.size());
    for (size_t i = 0; i < specs.size(); ++i) {
        const ToolStripItemSpec& spec = specs[i];
        if (spec.width > 0.0f) {
            active[i] = std::abs(state_.style.font_size - spec.width) < 0.01f;
        } else if (spec.transparent) {
            active[i] = !state_.style.has_background;
        } else if (spec.color.a != 1.0f && spec.color.a != 0.0f) {
            active[i] = state_.style.has_background && math::NearlyEqual(state_.style.background_color, spec.color);
        } else {
            active[i] = math::NearlyEqual(state_.style.text_color, spec.color);
        }
    }
    toolstrip_->SetActiveStates(active);

    SyncFontSelection();
}

const std::wstring& ImgViewerUiTextToolstrip::SelectedFontFamily() const
{
    if (font_dropdown_ != nullptr && font_dropdown_->SelectedIndex() < font_names_.size()) {
        return font_names_[font_dropdown_->SelectedIndex()];
    }
    return selected_font_family_;
}

D2D1_RECT_F ImgViewerUiTextToolstrip::Rect() const
{
    return toolstrip_->Rect();
}

D2D1_SIZE_F ImgViewerUiTextToolstrip::Measure(const UiDrawContext& context, D2D1_SIZE_F)
{
    if (!state_.visible) {
        return D2D1::SizeF();
    }
    EnsureFontOptions(context.dwrite_factory);
    return toolstrip_->Measure(context, D2D1::SizeF());
}

void ImgViewerUiTextToolstrip::Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_toolbar_rect)
{
    toolstrip_->Arrange(final_rect, anchor_toolbar_rect);
}

void ImgViewerUiTextToolstrip::Render(const UiDrawContext& draw_context, UiRootState state)
{
    toolstrip_->Render(draw_context, state);
}

UiEventResult ImgViewerUiTextToolstrip::OnPointerEvent(const UiPointerEvent& event)
{
    return toolstrip_->OnPointerEvent(event);
}

void ImgViewerUiTextToolstrip::EnsureFontOptions(IDWriteFactory* dwrite_factory)
{
    if (fonts_loaded_) {
        return;
    }
    fonts_loaded_ = true;

    if (dwrite_factory != nullptr) {
        wil::com_ptr<IDWriteFontCollection> collection;
        if (SUCCEEDED(dwrite_factory->GetSystemFontCollection(collection.put())) && collection != nullptr) {
            std::vector<std::wstring> names;
            const UINT32 family_count = collection->GetFontFamilyCount();
            names.reserve(family_count + 1);
            for (UINT32 index = 0; index < family_count; ++index) {
                wil::com_ptr<IDWriteFontFamily> family;
                if (FAILED(collection->GetFontFamily(index, family.put()))) {
                    continue;
                }
                wil::com_ptr<IDWriteLocalizedStrings> localized_names;
                if (FAILED(family->GetFamilyNames(localized_names.put())) || localized_names == nullptr) {
                    continue;
                }

                UINT32 name_index = 0;
                BOOL exists = FALSE;
                if (FAILED(localized_names->FindLocaleName(L"en-us", &name_index, &exists)) || !exists) {
                    name_index = 0;
                }
                UINT32 length = 0;
                if (FAILED(localized_names->GetStringLength(name_index, &length))) {
                    continue;
                }
                std::wstring name(length + 1, L'\0');
                if (SUCCEEDED(localized_names->GetString(name_index, name.data(), length + 1))) {
                    name.resize(length);
                }
                if (!name.empty()) {
                    names.push_back(std::move(name));
                }
            }

            std::sort(names.begin(), names.end());
            names.erase(std::unique(names.begin(), names.end()), names.end());
            if (!names.empty()) {
                font_names_ = std::move(names);
            }
        }
    }

    if (std::find(font_names_.begin(), font_names_.end(), L"Segoe UI") == font_names_.end()) {
        font_names_.insert(font_names_.begin(), L"Segoe UI");
    }

    font_options_.clear();
    font_options_.reserve(font_names_.size());
    for (const std::wstring& name : font_names_) {
        font_options_.push_back(DropdownOption{name.c_str(), UiActionFromImgViewerAction(ImgViewerAction::EditTextFontChanged)});
    }
    font_dropdown_->SetOptions(font_options_);
    SyncFontSelection();
}

void ImgViewerUiTextToolstrip::SyncFontSelection()
{
    if (font_dropdown_ == nullptr || font_names_.empty()) {
        return;
    }

    auto it = std::find(font_names_.begin(), font_names_.end(), selected_font_family_);
    if (it == font_names_.end()) {
        selected_font_family_ = L"Segoe UI";
        it = std::find(font_names_.begin(), font_names_.end(), selected_font_family_);
    }
    font_dropdown_->SetSelectedIndex(it == font_names_.end() ? 0 : static_cast<size_t>(std::distance(font_names_.begin(), it)));
}
