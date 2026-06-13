#include "ui.textbox.hpp"

#include <algorithm>
#include <cwchar>
#include <utility>
#include <vector>

#include <d2d1helper.h>

#include "imgviewer.strings.hpp"
#include "math.hpp"
#include "ui.popup.hpp"
#include "ui.theme.hpp"
#include "win32.clipboard.hpp"

namespace {

constexpr float kTextPaddingX = 5.0f;
constexpr float kCaretVerticalInset = 1.0f;
constexpr float kCaretWidth = 1.5f;
constexpr float kScrollOvershootMargin = 2.0f;
constexpr float kTextBoxLayoutMaxHeight = 16.0f;

} // namespace

TextBox::TextBox(UiElementMetadata metadata, const wchar_t* placeholder) :
    UiElement(metadata),
    placeholder_(placeholder)
{
    SetFocusable(true);
}

const std::wstring& TextBox::Text() const
{
    return edit_.Text();
}

const wchar_t* TextBox::AccessibilityValue() const
{
    return edit_.Text().c_str();
}

bool TextBox::AccessibilityIsReadOnly() const
{
    return false;
}

void TextBox::SetText(std::wstring text)
{
    edit_.SetText(std::move(text));
    edit_.MoveHome(false);
    horizontal_scroll_ = 0.0f;
}

void TextBox::SelectAll()
{
    edit_.SelectAll();
    horizontal_scroll_ = 0.0f;
}

void TextBox::SetTextServices(IDWriteFactory* factory, IDWriteTextFormat* format) const
{
    dwrite_factory_ = factory;
    text_format_ = format;
}

void TextBox::SetCaretVisible(bool visible)
{
    caret_visible_ = visible;
}

bool TextBox::IsEditing() const
{
    return true;
}

D2D1_POINT_2F TextBox::CaretPoint() const
{
    return caret_point_;
}

std::vector<MenuItem> TextBox::ContextMenuItems() const
{
    const bool has_selection = edit_.HasSelection();
    const bool can_paste = win32::IsClipboardTextAvailable();
    return std::vector<MenuItem>{
        {ImgViewerString(ImgViewerStringId::Copy), kUiActionTextCopy, false, false, has_selection},
        {ImgViewerString(ImgViewerStringId::Cut), kUiActionTextCut, false, false, has_selection},
        {ImgViewerString(ImgViewerStringId::Paste), kUiActionTextPaste, false, false, can_paste},
        {L"", kUiActionNone, true},
        {ImgViewerString(ImgViewerStringId::SelectAll), kUiActionTextSelectAll, false, false, !edit_.Text().empty()},
    };
}

D2D1_SIZE_F TextBox::Measure(const UiDrawContext&, D2D1_SIZE_F available_size) const
{
    return D2D1::SizeF((std::max)(1.0f, available_size.width), ui_theme::metrics::kInputHeight);
}

void TextBox::Render(const UiDrawContext& context, UiRootState root_state) const
{
    const UiElementState state = VisualState(root_state);
    SetTextServices(context.dwrite_factory, context.body_text_format);
    const UiDraw draw(context);
    const D2D1_RECT_F rect = Rect();
    draw.FillRoundedRect(
        D2D1::RoundedRect(rect, ui_theme::metrics::kButtonCornerRadius, ui_theme::metrics::kButtonCornerRadius),
        state.active || state.hovered ? ui_theme::color::kButtonDefault : ui_theme::color::kButtonDisabled);
    draw.DrawRoundedRect(
        D2D1::RoundedRect(rect, ui_theme::metrics::kButtonCornerRadius, ui_theme::metrics::kButtonCornerRadius),
        state.active ? ui_theme::color::kAccent : ui_theme::color::kBorder,
        state.active ? ui_theme::metrics::kActiveStrokeWidth : ui_theme::metrics::kStrokeWidth);

    const D2D1_RECT_F text_rect = D2D1::RectF(
        rect.left + kTextPaddingX,
        rect.top + ui_theme::metrics::kTextTopOffset,
        rect.right - kTextPaddingX,
        rect.bottom - ui_theme::metrics::kTextTopOffset);
    const std::wstring display = edit_.DisplayText();
    const bool showing_placeholder = display.empty() && edit_.Composition().empty();
    const std::wstring draw_text = showing_placeholder ? std::wstring(placeholder_) : display;
    wil::com_ptr<IDWriteTextLayout> layout = CreateLayout(draw_text, math::RectWidth(text_rect));

    if (!showing_placeholder && edit_.HasSelection() && layout != nullptr) {
        for (const DWRITE_HIT_TEST_METRICS& metric : edit_.SelectionMetrics(layout.get(), D2D1::Point2F(text_rect.left - horizontal_scroll_, text_rect.top))) {
            draw.FillRect(
                D2D1::RectF(metric.left, metric.top, metric.left + metric.width, metric.top + metric.height),
                ui_theme::color::kButtonPressed);
        }
    }

    if (layout != nullptr && context.d2d_context != nullptr) {
        wil::com_ptr<ID2D1SolidColorBrush> brush;
        const D2D1_COLOR_F color = showing_placeholder ? ui_theme::color::kMutedText : ui_theme::color::kBodyText;
        if (SUCCEEDED(context.d2d_context->CreateSolidColorBrush(color, brush.put()))) {
            context.d2d_context->PushAxisAlignedClip(text_rect, D2D1_ANTIALIAS_MODE_ALIASED);
            context.d2d_context->DrawTextLayout(
                D2D1::Point2F(text_rect.left - horizontal_scroll_, text_rect.top),
                layout.get(),
                brush.get());
            context.d2d_context->PopAxisAlignedClip();
        }
    }

    if (state.active && caret_visible_ && !edit_.HasSelection() && layout != nullptr) {
        D2D1_POINT_2F caret_top_point = {};
        D2D1_POINT_2F caret_bottom_point = {};
        edit_.CaretMetrics(layout.get(), D2D1::Point2F(text_rect.left - horizontal_scroll_, text_rect.top), &caret_top_point, &caret_bottom_point);
        const float caret_x = caret_top_point.x;
        const float caret_top = text_rect.top + kCaretVerticalInset;
        const float caret_bottom = rect.bottom - ui_theme::metrics::kTextTopOffset - kCaretVerticalInset;
        caret_point_ = D2D1::Point2F(caret_x, caret_bottom);
        draw.FillRect(D2D1::RectF(caret_x, caret_top, caret_x + kCaretWidth, caret_bottom), ui_theme::color::kBodyText);
    }
}

UiEventResult TextBox::OnInputEvent(const UiInputEvent& event)
{
    switch (event.type) {
    case UiEventType::PointerMove:
    case UiEventType::PointerDown:
    case UiEventType::PointerUp:
    case UiEventType::PointerLeave:
    case UiEventType::PointerWheel:
        return OnPointerEvent(event.pointer);
    case UiEventType::KeyDown:
    case UiEventType::KeyUp:
        return OnKeyEvent(event.key);
    case UiEventType::TextChar:
        return InsertCharacter(event.character);
    case UiEventType::ImeStartComposition:
        return UiEventResult{.handled = true, .wants_ime_position = true};
    case UiEventType::ImeComposition:
        return UpdateImeComposition(event.text);
    case UiEventType::ImeEndComposition:
        return EndImeComposition();
    case UiEventType::ContextMenu:
        if (event.popup_host != nullptr) {
            SetFocus(event.hwnd);
            event.popup_host->OpenMenu(event.point, ContextMenuItems());
        }
        return UiEventResult{
            .handled = true,
            .focus = UiFocusRequest::FocusTarget,
            .focus_target = Id(),
        };
    case UiEventType::Timer:
        caret_visible_ = !caret_visible_;
        return UiEventResult{.handled = true};
    default:
        return {};
    }
}

UiEventResult TextBox::OnPointerEvent(const UiPointerEvent& event)
{
    if (event.type == UiEventType::PointerDown && event.button == UiPointerButton::Left) {
        const size_t index = HitTest(event.point);
        edit_.MoveCaret(index, event.modifiers.shift);
        UpdateHorizontalScroll();
        dragging_ = true;
        return UiEventResult{
            .handled = true,
            .capture = UiCaptureRequest::Capture,
            .focus = UiFocusRequest::FocusTarget,
            .focus_target = Id(),
        };
    }

    if (event.type == UiEventType::PointerMove && dragging_) {
        edit_.MoveCaret(HitTest(event.point), true);
        UpdateHorizontalScroll();
        return UiEventResult{.handled = true};
    }

    if (event.type == UiEventType::PointerUp && event.captured == Id()) {
        dragging_ = false;
        return UiEventResult{.handled = true, .capture = UiCaptureRequest::Release};
    }

    return {};
}

UiEventResult TextBox::OnKeyEvent(const UiKeyEvent& event)
{
    if (event.type != UiEventType::KeyDown || !IsEnabled()) {
        return {};
    }

    const bool ctrl = event.modifiers.ctrl;
    if (ctrl && event.virtual_key == 'A') {
        edit_.SelectAll();
        return UiEventResult{.handled = true};
    }
    if (ctrl && (event.virtual_key == 'C' || event.virtual_key == 'X' || event.virtual_key == 'V')) {
        return UiEventResult{.handled = true, .action = event.virtual_key == 'C' ? kUiActionTextCopy : event.virtual_key == 'X' ? kUiActionTextCut : kUiActionTextPaste};
    }

    switch (event.virtual_key) {
    case VK_LEFT:
        edit_.MoveLeft(event.modifiers.shift);
        UpdateHorizontalScroll();
        return UiEventResult{.handled = true};
    case VK_RIGHT:
        edit_.MoveRight(event.modifiers.shift);
        UpdateHorizontalScroll();
        return UiEventResult{.handled = true};
    case VK_HOME:
        edit_.MoveHome(event.modifiers.shift);
        UpdateHorizontalScroll();
        return UiEventResult{.handled = true};
    case VK_END:
        edit_.MoveEnd(event.modifiers.shift);
        UpdateHorizontalScroll();
        return UiEventResult{.handled = true};
    case VK_BACK:
        edit_.Backspace();
        UpdateHorizontalScroll();
        return UiEventResult{.handled = true};
    case VK_DELETE:
        edit_.Delete();
        UpdateHorizontalScroll();
        return UiEventResult{.handled = true};
    default:
        break;
    }

    return {};
}

UiEventResult TextBox::InsertCharacter(wchar_t ch)
{
    if (!edit_.InsertCharacter(ch)) {
        return {};
    }
    UpdateHorizontalScroll();
    return UiEventResult{.handled = true};
}

UiEventResult TextBox::UpdateImeComposition(std::wstring composition)
{
    edit_.SetComposition(std::move(composition));
    return UiEventResult{.handled = true};
}

UiEventResult TextBox::EndImeComposition()
{
    edit_.ClearComposition();
    return UiEventResult{.handled = true};
}

UiEventResult TextBox::ExecuteEditAction(UiAction action, HWND hwnd)
{
    if (action == kUiActionTextCopy) {
        return UiEventResult{.handled = CopySelection(hwnd)};
    }
    if (action == kUiActionTextCut) {
        if (CopySelection(hwnd)) {
            edit_.Delete();
            UpdateHorizontalScroll();
            return UiEventResult{.handled = true};
        }
        return {};
    }
    if (action == kUiActionTextPaste) {
        return UiEventResult{.handled = PasteClipboard(hwnd)};
    }
    if (action == kUiActionTextSelectAll) {
        edit_.SelectAll();
        return UiEventResult{.handled = true};
    }
    return {};
}

size_t TextBox::HitTest(D2D1_POINT_2F point) const
{
    const D2D1_RECT_F rect = Rect();
    const float width = (std::max)(1.0f, rect.right - rect.left - kTextPaddingX * 2.0f);
    return edit_.HitTest(
        dwrite_factory_,
        text_format_,
        D2D1::Point2F(point.x - rect.left - kTextPaddingX + horizontal_scroll_, point.y - rect.top - ui_theme::metrics::kTextTopOffset),
        width,
        kTextBoxLayoutMaxHeight);
}

wil::com_ptr<IDWriteTextLayout> TextBox::CreateLayout(const std::wstring& value, float width) const
{
    return edit_.CreateLayout(dwrite_factory_, text_format_, value, width + horizontal_scroll_ + 4096.0f, kTextBoxLayoutMaxHeight);
}

void TextBox::UpdateHorizontalScroll()
{
    const D2D1_RECT_F rect = Rect();
    const float width = (std::max)(1.0f, rect.right - rect.left - kTextPaddingX * 2.0f);
    wil::com_ptr<IDWriteTextLayout> layout = CreateLayout(edit_.Text(), width);
    if (layout == nullptr) {
        return;
    }
    D2D1_POINT_2F top = {};
    D2D1_POINT_2F bottom = {};
    if (!edit_.CaretMetrics(layout.get(), D2D1::Point2F(), &top, &bottom)) {
        return;
    }
    const float x = top.x;
    if (x - horizontal_scroll_ > width) {
        horizontal_scroll_ = x - width + kScrollOvershootMargin;
    } else if (x < horizontal_scroll_) {
        horizontal_scroll_ = (std::max)(0.0f, x - kScrollOvershootMargin);
    }
}

bool TextBox::CopySelection(HWND hwnd) const
{
    const std::wstring selected = edit_.SelectedText();
    return !selected.empty() && win32::CopyTextToClipboard(hwnd, selected.c_str());
}

bool TextBox::PasteClipboard(HWND hwnd)
{
    std::wstring text;
    if (!win32::ReadClipboardText(hwnd, &text)) {
        return false;
    }
    if (!edit_.InsertText(text)) {
        return false;
    }
    UpdateHorizontalScroll();
    return true;
}
