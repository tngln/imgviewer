#include "ui.textbox.hpp"

#include <algorithm>
#include <cstring>
#include <cwchar>
#include <utility>
#include <vector>

#include <d2d1helper.h>
#include <wil/resource.h>

#include "math.hpp"
#include "ui.theme.hpp"

namespace {

constexpr float kTextPaddingX = 10.0f;
constexpr float kTextPaddingY = 5.0f;

bool IsPrintable(wchar_t ch)
{
    return ch >= L' ' && ch != 0x7f;
}

} // namespace

TextBox::TextBox(UiElementMetadata metadata, const wchar_t* placeholder) :
    UiElement(metadata),
    placeholder_(placeholder)
{
    SetFocusable(true);
}

const std::wstring& TextBox::Text() const
{
    return text_;
}

void TextBox::SetText(std::wstring text)
{
    text_ = std::move(text);
    caret_ = (std::min)(caret_, text_.size());
    anchor_ = (std::min)(anchor_, text_.size());
}

void TextBox::SetTextServices(IDWriteFactory* factory, IDWriteTextFormat* format)
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
    const bool has_selection = HasSelection();
    const bool can_paste = IsClipboardFormatAvailable(CF_UNICODETEXT) != FALSE;
    return std::vector<MenuItem>{
        {L"Copy", kUiActionTextCopy, false, false, has_selection},
        {L"Cut", kUiActionTextCut, false, false, has_selection},
        {L"Paste", kUiActionTextPaste, false, false, can_paste},
        {L"", kUiActionNone, true},
        {L"Select All", kUiActionTextSelectAll, false, false, !text_.empty()},
    };
}

void TextBox::Draw(const UiDrawContext& context, UiElementState state) const
{
    const_cast<TextBox*>(this)->SetTextServices(context.dwrite_factory, context.body_text_format);
    const UiDraw draw(context);
    const D2D1_RECT_F rect = Rect();
    draw.FillRoundedRect(
        D2D1::RoundedRect(rect, ui_theme::metrics::kButtonCornerRadius, ui_theme::metrics::kButtonCornerRadius),
        state.active || state.hovered ? ui_theme::color::kButtonDefault : ui_theme::color::kButtonDisabled);
    draw.DrawRoundedRect(
        D2D1::RoundedRect(rect, ui_theme::metrics::kButtonCornerRadius, ui_theme::metrics::kButtonCornerRadius),
        state.active ? ui_theme::color::kAccent : ui_theme::color::kBorder,
        state.active ? 1.5f : 1.0f);

    const D2D1_RECT_F text_rect = D2D1::RectF(
        rect.left + kTextPaddingX,
        rect.top + kTextPaddingY,
        rect.right - kTextPaddingX,
        rect.bottom - kTextPaddingY);
    const std::wstring display = DisplayText();
    const bool showing_placeholder = display.empty() && composition_.empty();
    const std::wstring draw_text = showing_placeholder ? std::wstring(placeholder_) : display;
    wil::com_ptr<IDWriteTextLayout> layout = CreateLayout(draw_text, math::RectWidth(text_rect));

    if (!showing_placeholder && HasSelection() && layout != nullptr) {
        const DWRITE_TEXT_RANGE range{
            static_cast<UINT32>(SelectionStart()),
            static_cast<UINT32>(SelectionEnd() - SelectionStart()),
        };
        UINT32 count = 0;
        layout->HitTestTextRange(range.startPosition, range.length, text_rect.left - horizontal_scroll_, text_rect.top, nullptr, 0, &count);
        std::vector<DWRITE_HIT_TEST_METRICS> metrics(count);
        if (!metrics.empty() &&
            SUCCEEDED(layout->HitTestTextRange(range.startPosition, range.length, text_rect.left - horizontal_scroll_, text_rect.top, metrics.data(), count, &count))) {
            for (const DWRITE_HIT_TEST_METRICS& metric : metrics) {
                draw.FillRect(
                    D2D1::RectF(metric.left, metric.top, metric.left + metric.width, metric.top + metric.height),
                    ui_theme::color::kButtonPressed);
            }
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

    if (state.active && caret_visible_ && !HasSelection() && layout != nullptr) {
        DWRITE_HIT_TEST_METRICS metric = {};
        FLOAT x = 0.0f;
        FLOAT y = 0.0f;
        layout->HitTestTextPosition(static_cast<UINT32>(caret_), FALSE, &x, &y, &metric);
        const float caret_x = text_rect.left - horizontal_scroll_ + x;
        const float caret_top = text_rect.top + 2.0f;
        const float caret_bottom = rect.bottom - kTextPaddingY - 2.0f;
        const_cast<TextBox*>(this)->caret_point_ = D2D1::Point2F(caret_x, caret_bottom);
        draw.FillRect(D2D1::RectF(caret_x, caret_top, caret_x + 1.5f, caret_bottom), ui_theme::color::kBodyText);
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
    case UiEventType::Timer:
        caret_visible_ = !caret_visible_;
        return UiEventResult{.handled = true, .needs_render = true};
    default:
        return {};
    }
}

UiEventResult TextBox::OnPointerEvent(const UiPointerEvent& event)
{
    if (event.type == UiEventType::PointerDown && event.button == UiPointerButton::Left) {
        const size_t index = HitTest(event.point);
        MoveCaret(index, event.modifiers.shift);
        dragging_ = true;
        return UiEventResult{
            .handled = true,
            .needs_render = true,
            .capture = UiCaptureRequest::Capture,
            .focus = UiFocusRequest::FocusTarget,
            .focus_target = Id(),
        };
    }

    if (event.type == UiEventType::PointerMove && dragging_) {
        MoveCaret(HitTest(event.point), true);
        return UiEventResult{.handled = true, .needs_render = true};
    }

    if (event.type == UiEventType::PointerUp && event.captured == Id()) {
        dragging_ = false;
        return UiEventResult{.handled = true, .needs_render = true, .capture = UiCaptureRequest::Release};
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
        anchor_ = 0;
        caret_ = text_.size();
        return UiEventResult{.handled = true, .needs_render = true};
    }
    if (ctrl && (event.virtual_key == 'C' || event.virtual_key == 'X' || event.virtual_key == 'V')) {
        return UiEventResult{.handled = true, .action = event.virtual_key == 'C' ? kUiActionTextCopy : event.virtual_key == 'X' ? kUiActionTextCut : kUiActionTextPaste};
    }

    switch (event.virtual_key) {
    case VK_LEFT:
        MoveCaret(caret_ == 0 ? 0 : caret_ - 1, event.modifiers.shift);
        return UiEventResult{.handled = true, .needs_render = true};
    case VK_RIGHT:
        MoveCaret((std::min)(caret_ + 1, text_.size()), event.modifiers.shift);
        return UiEventResult{.handled = true, .needs_render = true};
    case VK_HOME:
        MoveCaret(0, event.modifiers.shift);
        return UiEventResult{.handled = true, .needs_render = true};
    case VK_END:
        MoveCaret(text_.size(), event.modifiers.shift);
        return UiEventResult{.handled = true, .needs_render = true};
    case VK_BACK:
        if (HasSelection()) {
            DeleteSelection();
        } else if (caret_ > 0) {
            text_.erase(caret_ - 1, 1);
            --caret_;
            anchor_ = caret_;
        }
        return UiEventResult{.handled = true, .needs_render = true};
    case VK_DELETE:
        if (HasSelection()) {
            DeleteSelection();
        } else if (caret_ < text_.size()) {
            text_.erase(caret_, 1);
        }
        return UiEventResult{.handled = true, .needs_render = true};
    default:
        break;
    }

    return {};
}

UiEventResult TextBox::InsertCharacter(wchar_t ch)
{
    if (!IsPrintable(ch)) {
        return {};
    }
    InsertText(std::wstring(1, ch));
    return UiEventResult{.handled = true, .needs_render = true};
}

UiEventResult TextBox::UpdateImeComposition(std::wstring composition)
{
    composition_ = std::move(composition);
    return UiEventResult{.handled = true, .needs_render = true};
}

UiEventResult TextBox::EndImeComposition()
{
    composition_.clear();
    return UiEventResult{.handled = true, .needs_render = true};
}

UiEventResult TextBox::ExecuteEditAction(UiAction action, HWND hwnd)
{
    if (action == kUiActionTextCopy) {
        return UiEventResult{.handled = CopySelection(hwnd)};
    }
    if (action == kUiActionTextCut) {
        if (CopySelection(hwnd)) {
            DeleteSelection();
            return UiEventResult{.handled = true, .needs_render = true};
        }
        return {};
    }
    if (action == kUiActionTextPaste) {
        return UiEventResult{.handled = PasteClipboard(hwnd), .needs_render = true};
    }
    if (action == kUiActionTextSelectAll) {
        anchor_ = 0;
        caret_ = text_.size();
        return UiEventResult{.handled = true, .needs_render = true};
    }
    return {};
}

bool TextBox::HasSelection() const
{
    return caret_ != anchor_;
}

size_t TextBox::SelectionStart() const
{
    return (std::min)(caret_, anchor_);
}

size_t TextBox::SelectionEnd() const
{
    return (std::max)(caret_, anchor_);
}

void TextBox::DeleteSelection()
{
    if (!HasSelection()) {
        return;
    }
    const size_t start = SelectionStart();
    text_.erase(start, SelectionEnd() - start);
    caret_ = start;
    anchor_ = start;
}

void TextBox::InsertText(const std::wstring& text)
{
    DeleteSelection();
    text_.insert(caret_, text);
    caret_ += text.size();
    anchor_ = caret_;
}

void TextBox::MoveCaret(size_t index, bool extend_selection)
{
    caret_ = (std::min)(index, text_.size());
    if (!extend_selection) {
        anchor_ = caret_;
    }
    UpdateHorizontalScroll();
}

size_t TextBox::HitTest(D2D1_POINT_2F point) const
{
    const D2D1_RECT_F rect = Rect();
    const float width = (std::max)(1.0f, rect.right - rect.left - kTextPaddingX * 2.0f);
    wil::com_ptr<IDWriteTextLayout> layout = CreateLayout(DisplayText(), width);
    if (layout == nullptr) {
        return text_.size();
    }
    BOOL trailing = FALSE;
    BOOL inside = FALSE;
    DWRITE_HIT_TEST_METRICS metric = {};
    layout->HitTestPoint(
        point.x - rect.left - kTextPaddingX + horizontal_scroll_,
        point.y - rect.top - kTextPaddingY,
        &trailing,
        &inside,
        &metric);
    return (std::min<size_t>(metric.textPosition + (trailing ? 1 : 0), text_.size()));
}

wil::com_ptr<IDWriteTextLayout> TextBox::CreateLayout(const std::wstring& value, float width) const
{
    wil::com_ptr<IDWriteTextLayout> layout;
    if (dwrite_factory_ == nullptr || text_format_ == nullptr) {
        return layout;
    }
    dwrite_factory_->CreateTextLayout(
        value.c_str(),
        static_cast<UINT32>(value.size()),
        text_format_,
        (std::max)(1.0f, width + horizontal_scroll_ + 4096.0f),
        32.0f,
        layout.put());
    return layout;
}

std::wstring TextBox::DisplayText() const
{
    std::wstring value = text_;
    value.insert(caret_, composition_);
    return value;
}

void TextBox::UpdateHorizontalScroll()
{
    const D2D1_RECT_F rect = Rect();
    const float width = (std::max)(1.0f, rect.right - rect.left - kTextPaddingX * 2.0f);
    wil::com_ptr<IDWriteTextLayout> layout = CreateLayout(text_, width);
    if (layout == nullptr) {
        return;
    }
    FLOAT x = 0.0f;
    FLOAT y = 0.0f;
    DWRITE_HIT_TEST_METRICS metric = {};
    layout->HitTestTextPosition(static_cast<UINT32>(caret_), FALSE, &x, &y, &metric);
    if (x - horizontal_scroll_ > width) {
        horizontal_scroll_ = x - width + 4.0f;
    } else if (x < horizontal_scroll_) {
        horizontal_scroll_ = (std::max)(0.0f, x - 4.0f);
    }
}

std::wstring TextBox::SelectedText() const
{
    return HasSelection() ? text_.substr(SelectionStart(), SelectionEnd() - SelectionStart()) : std::wstring();
}

bool TextBox::CopySelection(HWND hwnd) const
{
    const std::wstring selected = SelectedText();
    if (selected.empty() || !OpenClipboard(hwnd)) {
        return false;
    }
    auto close_clipboard = wil::scope_exit([] { CloseClipboard(); });
    EmptyClipboard();
    const size_t bytes = (selected.size() + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (memory == nullptr) {
        return false;
    }
    void* target = GlobalLock(memory);
    if (target == nullptr) {
        GlobalFree(memory);
        return false;
    }
    memcpy(target, selected.c_str(), bytes);
    GlobalUnlock(memory);
    if (SetClipboardData(CF_UNICODETEXT, memory) == nullptr) {
        GlobalFree(memory);
        return false;
    }
    return true;
}

bool TextBox::PasteClipboard(HWND hwnd)
{
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT) || !OpenClipboard(hwnd)) {
        return false;
    }
    auto close_clipboard = wil::scope_exit([] { CloseClipboard(); });
    HGLOBAL memory = GetClipboardData(CF_UNICODETEXT);
    if (memory == nullptr) {
        return false;
    }
    const wchar_t* source = static_cast<const wchar_t*>(GlobalLock(memory));
    if (source == nullptr) {
        return false;
    }
    InsertText(source);
    GlobalUnlock(memory);
    return true;
}
