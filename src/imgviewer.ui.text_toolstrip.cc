#include "imgviewer.ui.text_toolstrip.hpp"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <memory>

#include <d2d1helper.h>
#include <wil/com.h>

#include "imgviewer.action.hpp"
#include "imgviewer.config.hpp"
#include "imgviewer.ui.action.hpp"
#include "ui.theme.hpp"

namespace {

constexpr float kToolbarGapAboveAnchor = 6.0f;
constexpr float kFontDropdownWidth = 180.0f;

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
    return UiEventResult{.handled = true, .needs_render = true, .action = button.Action()};
}

class TextSizeButton final : public UiElement {
public:
    TextSizeButton(UiElementMetadata metadata, const wchar_t* label) : UiElement(metadata), label_(label)
    {
        SetFocusable(true);
    }

    void Render(const UiDrawContext& context, UiRootState root_state) const override
    {
        const UiElementState state = VisualState(root_state);
        const D2D1_RECT_F rect = Rect();
        const UiDraw draw(context);
        draw.FillRect(rect, ui_theme::WidgetFillColor(state));
        draw.DrawBodyText(
            label_,
            static_cast<UINT32>(wcslen(label_)),
            D2D1::RectF(rect.left + 3.0f, rect.top + ui_theme::metrics::kTextTopOffset, rect.right - 3.0f, rect.bottom),
            state.enabled ? ui_theme::color::kBodyText : ui_theme::color::kButtonDisabledContent,
            D2D1_DRAW_TEXT_OPTIONS_CLIP);
        if (state.active) {
            draw.DrawRoundedRect(
                D2D1::RoundedRect(D2D1::RectF(rect.left + 2.0f, rect.top + 2.0f, rect.right - 2.0f, rect.bottom - 2.0f), 3.0f, 3.0f),
                ui_theme::color::kAccent,
                ui_theme::metrics::kActiveStrokeWidth);
        }
    }

    UiEventResult OnPointerEvent(const UiPointerEvent& event) override { return ToolButtonPointerEvent(*this, event); }
    UiEventResult OnKeyEvent(const UiKeyEvent& event) override { return ToolButtonKeyEvent(*this, event); }

private:
    const wchar_t* label_ = L"";
};

class TextColorButton final : public UiElement {
public:
    TextColorButton(UiElementMetadata metadata, D2D1_COLOR_F color, bool transparent) :
        UiElement(metadata),
        color_(color),
        transparent_(transparent)
    {
        SetFocusable(true);
    }

    void Render(const UiDrawContext& context, UiRootState root_state) const override
    {
        const UiElementState state = VisualState(root_state);
        const D2D1_RECT_F rect = Rect();
        const UiDraw draw(context);
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
        if (state.active) {
            draw.DrawRoundedRect(
                D2D1::RoundedRect(D2D1::RectF(rect.left + 2.0f, rect.top + 2.0f, rect.right - 2.0f, rect.bottom - 2.0f), 3.0f, 3.0f),
                ui_theme::color::kAccent,
                ui_theme::metrics::kActiveStrokeWidth);
        }
    }

    UiEventResult OnPointerEvent(const UiPointerEvent& event) override { return ToolButtonPointerEvent(*this, event); }
    UiEventResult OnKeyEvent(const UiKeyEvent& event) override { return ToolButtonKeyEvent(*this, event); }

private:
    D2D1_COLOR_F color_ = D2D1::ColorF(D2D1::ColorF::Black);
    bool transparent_ = false;
};

struct ButtonSpec final {
    ImgViewerUiTextToolstrip::ButtonKey button = ImgViewerUiTextToolstrip::ButtonKey::Size20;
    ImgViewerAction action = ImgViewerAction::None;
    const wchar_t* name = L"";
    const wchar_t* tooltip = L"";
    const wchar_t* automation_id = L"";
    D2D1_COLOR_F color = {};
    float size = 0.0f;
    bool background = false;
    bool transparent = false;
};

const std::array<ButtonSpec, ImgViewerUiTextToolstrip::kButtonCount> kButtonSpecs{{
    {ImgViewerUiTextToolstrip::ButtonKey::Size12, ImgViewerAction::EditTextSize12, L"12", L"12 px text", L"edit-text-size-12", {}, 12.0f},
    {ImgViewerUiTextToolstrip::ButtonKey::Size16, ImgViewerAction::EditTextSize16, L"16", L"16 px text", L"edit-text-size-16", {}, 16.0f},
    {ImgViewerUiTextToolstrip::ButtonKey::Size20, ImgViewerAction::EditTextSize20, L"20", L"20 px text", L"edit-text-size-20", {}, 20.0f},
    {ImgViewerUiTextToolstrip::ButtonKey::Size28, ImgViewerAction::EditTextSize28, L"28", L"28 px text", L"edit-text-size-28", {}, 28.0f},
    {ImgViewerUiTextToolstrip::ButtonKey::Size36, ImgViewerAction::EditTextSize36, L"36", L"36 px text", L"edit-text-size-36", {}, 36.0f},
    {ImgViewerUiTextToolstrip::ButtonKey::TextRed, ImgViewerAction::EditTextColorRed, L"Red", L"Red text", L"edit-text-red", D2D1::ColorF(D2D1::ColorF::Red)},
    {ImgViewerUiTextToolstrip::ButtonKey::TextYellow, ImgViewerAction::EditTextColorYellow, L"Yellow", L"Yellow text", L"edit-text-yellow", D2D1::ColorF(D2D1::ColorF::Yellow)},
    {ImgViewerUiTextToolstrip::ButtonKey::TextGreen, ImgViewerAction::EditTextColorGreen, L"Green", L"Green text", L"edit-text-green", D2D1::ColorF(D2D1::ColorF::Lime)},
    {ImgViewerUiTextToolstrip::ButtonKey::TextCyan, ImgViewerAction::EditTextColorCyan, L"Cyan", L"Cyan text", L"edit-text-cyan", D2D1::ColorF(D2D1::ColorF::Cyan)},
    {ImgViewerUiTextToolstrip::ButtonKey::TextBlue, ImgViewerAction::EditTextColorBlue, L"Blue", L"Blue text", L"edit-text-blue", D2D1::ColorF(D2D1::ColorF::DodgerBlue)},
    {ImgViewerUiTextToolstrip::ButtonKey::TextMagenta, ImgViewerAction::EditTextColorMagenta, L"Magenta", L"Magenta text", L"edit-text-magenta", D2D1::ColorF(D2D1::ColorF::Magenta)},
    {ImgViewerUiTextToolstrip::ButtonKey::TextWhite, ImgViewerAction::EditTextColorWhite, L"White", L"White text", L"edit-text-white", D2D1::ColorF(D2D1::ColorF::White)},
    {ImgViewerUiTextToolstrip::ButtonKey::TextBlack, ImgViewerAction::EditTextColorBlack, L"Black", L"Black text", L"edit-text-black", D2D1::ColorF(D2D1::ColorF::Black)},
    {ImgViewerUiTextToolstrip::ButtonKey::BackgroundTransparent, ImgViewerAction::EditTextBackgroundTransparent, L"None", L"Transparent text background", L"edit-text-bg-transparent", {}, 0.0f, true, true},
    {ImgViewerUiTextToolstrip::ButtonKey::BackgroundYellow, ImgViewerAction::EditTextBackgroundYellow, L"Yellow", L"Yellow text background", L"edit-text-bg-yellow", D2D1::ColorF(D2D1::ColorF::Yellow, 0.82f), 0.0f, true},
    {ImgViewerUiTextToolstrip::ButtonKey::BackgroundWhite, ImgViewerAction::EditTextBackgroundWhite, L"White", L"White text background", L"edit-text-bg-white", D2D1::ColorF(D2D1::ColorF::White, 0.82f), 0.0f, true},
    {ImgViewerUiTextToolstrip::ButtonKey::BackgroundBlack, ImgViewerAction::EditTextBackgroundBlack, L"Black", L"Black text background", L"edit-text-bg-black", D2D1::ColorF(D2D1::ColorF::Black, 0.82f), 0.0f, true},
    {ImgViewerUiTextToolstrip::ButtonKey::BackgroundRed, ImgViewerAction::EditTextBackgroundRed, L"Red", L"Red text background", L"edit-text-bg-red", D2D1::ColorF(D2D1::ColorF::Red, 0.82f), 0.0f, true},
    {ImgViewerUiTextToolstrip::ButtonKey::BackgroundBlue, ImgViewerAction::EditTextBackgroundBlue, L"Blue", L"Blue text background", L"edit-text-bg-blue", D2D1::ColorF(D2D1::ColorF::DodgerBlue, 0.82f), 0.0f, true},
}};

} // namespace

constexpr size_t ImgViewerUiTextToolstrip::ButtonIndex(ButtonKey button)
{
    return static_cast<size_t>(button);
}

ImgViewerUiTextToolstrip::ImgViewerUiTextToolstrip(UiElement& root)
{
    toolbar_ = std::make_unique<ImgViewerFloatingToolbar>(root, L"Text tools", L"text-toolstrip");

    for (const ButtonSpec& spec : kButtonSpecs) {
        ButtonInstance& button = buttons_[ButtonIndex(spec.button)];
        UiElementMetadata metadata = UiMetadata(
            UiElementRole::Button,
            UiActionFromImgViewerAction(spec.action),
            spec.name,
            spec.tooltip,
            spec.automation_id);
        std::unique_ptr<UiElement> element;
        if (spec.size > 0.0f) {
            element = std::make_unique<TextSizeButton>(metadata, spec.name);
        } else {
            element = std::make_unique<TextColorButton>(metadata, spec.color, spec.transparent);
        }
        button.element = toolbar_->Panel()->AddItem(std::move(element), ui_theme::metrics::kToolbarButtonSize);
        button.id = button.element->Id();
    }

    font_names_.push_back(L"Segoe UI");
    font_options_.push_back(DropdownOption{font_names_[0].c_str(), UiActionFromImgViewerAction(ImgViewerAction::EditTextFontChanged)});
    font_dropdown_ = static_cast<Dropdown*>(toolbar_->Panel()->AddItem(std::make_unique<Dropdown>(
        UiMetadata(
            UiElementRole::ComboBox,
            UiActionFromImgViewerAction(ImgViewerAction::EditTextFontChanged),
            L"Font",
            L"Text font",
            L"edit-text-font"),
        font_options_)));

    SetScalePercent(scale_percent_);
    SetState(state_);
}

void ImgViewerUiTextToolstrip::SetScalePercent(int percent)
{
    scale_percent_ = ClampToolbarScalePercent(percent);
    toolbar_->SetScalePercent(scale_percent_);
}

void ImgViewerUiTextToolstrip::SetState(ImgViewerUiTextToolstripState state)
{
    state_ = std::move(state);
    selected_font_family_ = state_.style.font_family.empty() ? L"Segoe UI" : state_.style.font_family;
    SyncFontSelection();
    UpdateVisualState();
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
    return toolbar_->Rect();
}

D2D1_SIZE_F ImgViewerUiTextToolstrip::Measure(const UiDrawContext& context, D2D1_SIZE_F)
{
    if (!state_.visible) {
        return D2D1::SizeF();
    }
    EnsureFontOptions(context.dwrite_factory);
    return toolbar_->Measure(kButtonSpecs.size(), toolbar_->ScaledValue(kFontDropdownWidth), 1);
}

void ImgViewerUiTextToolstrip::Arrange(D2D1_RECT_F final_rect, D2D1_RECT_F anchor_toolbar_rect)
{
    if (!state_.visible) {
        toolbar_->ArrangeHidden();
        return;
    }

    toolbar_->ArrangeAboveAnchor(
        final_rect,
        anchor_toolbar_rect,
        kButtonSpecs.size(),
        toolbar_->ScaledValue(kFontDropdownWidth),
        1,
        kToolbarGapAboveAnchor);
}

void ImgViewerUiTextToolstrip::Render(const UiDrawContext& draw_context, UiRootState state)
{
    if (!state_.visible) {
        return;
    }

    toolbar_->RenderBackground(draw_context, ui_theme::color::kBorder, ui_theme::metrics::kStrokeWidth);
    toolbar_->Panel()->Render(draw_context, state);
}

UiEventResult ImgViewerUiTextToolstrip::OnPointerEvent(const UiPointerEvent&)
{
    return {};
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

void ImgViewerUiTextToolstrip::UpdateVisualState()
{
    toolbar_->Panel()->SetEnabled(state_.visible);
    if (font_dropdown_ != nullptr) {
        font_dropdown_->SetEnabled(state_.visible);
    }
    for (size_t index = 0; index < kButtonSpecs.size(); ++index) {
        UiElement* element = buttons_[index].element;
        if (element == nullptr) {
            continue;
        }
        element->SetEnabled(state_.visible);
        const ButtonSpec& spec = kButtonSpecs[index];
        bool active = false;
        if (spec.size > 0.0f) {
            active = std::abs(state_.style.font_size - spec.size) < 0.01f;
        } else if (spec.background) {
            active = spec.transparent
                ? !state_.style.has_background
                : state_.style.has_background && SameColor(state_.style.background_color, spec.color);
        } else {
            active = SameColor(state_.style.text_color, spec.color);
        }
        element->SetVisualActive(state_.visible && active);
    }
}
