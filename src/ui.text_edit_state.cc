#include "ui.text_edit_state.hpp"

#include <algorithm>
#include <utility>

namespace {

bool IsPrintable(wchar_t ch)
{
    return ch >= L' ' && ch != 0x7f;
}

} // namespace

const std::wstring& TextEditState::Text() const
{
    return text_;
}

void TextEditState::SetText(std::wstring text)
{
    text_ = std::move(text);
    caret_ = (std::min)(caret_, text_.size());
    anchor_ = (std::min)(anchor_, text_.size());
}

const std::wstring& TextEditState::Composition() const
{
    return composition_;
}

void TextEditState::SetComposition(std::wstring composition)
{
    composition_ = std::move(composition);
}

void TextEditState::ClearComposition()
{
    composition_.clear();
}

size_t TextEditState::Caret() const
{
    return caret_;
}

size_t TextEditState::Anchor() const
{
    return anchor_;
}

bool TextEditState::HasSelection() const
{
    return caret_ != anchor_;
}

size_t TextEditState::SelectionStart() const
{
    return (std::min)(caret_, anchor_);
}

size_t TextEditState::SelectionEnd() const
{
    return (std::max)(caret_, anchor_);
}

std::wstring TextEditState::SelectedText() const
{
    return HasSelection() ? text_.substr(SelectionStart(), SelectionEnd() - SelectionStart()) : std::wstring();
}

std::wstring TextEditState::DisplayText() const
{
    std::wstring value = text_;
    value.insert(caret_, composition_);
    return value;
}

bool TextEditState::InsertCharacter(wchar_t ch)
{
    if (!IsPrintable(ch)) {
        return false;
    }
    return InsertText(std::wstring(1, ch));
}

bool TextEditState::InsertText(const std::wstring& text)
{
    if (text.empty()) {
        return false;
    }
    DeleteSelection();
    text_.insert(caret_, text);
    caret_ += text.size();
    anchor_ = caret_;
    return true;
}

bool TextEditState::Backspace()
{
    if (HasSelection()) {
        DeleteSelection();
        return true;
    }
    if (caret_ == 0) {
        return false;
    }
    text_.erase(caret_ - 1, 1);
    --caret_;
    anchor_ = caret_;
    return true;
}

bool TextEditState::Delete()
{
    if (HasSelection()) {
        DeleteSelection();
        return true;
    }
    if (caret_ >= text_.size()) {
        return false;
    }
    text_.erase(caret_, 1);
    return true;
}

bool TextEditState::SelectAll()
{
    anchor_ = 0;
    caret_ = text_.size();
    return !text_.empty();
}

bool TextEditState::MoveCaret(size_t index, bool extend_selection)
{
    const size_t previous_caret = caret_;
    const size_t previous_anchor = anchor_;
    caret_ = (std::min)(index, text_.size());
    if (!extend_selection) {
        anchor_ = caret_;
    }
    return previous_caret != caret_ || previous_anchor != anchor_;
}

bool TextEditState::MoveLeft(bool extend_selection)
{
    return MoveCaret(caret_ == 0 ? 0 : caret_ - 1, extend_selection);
}

bool TextEditState::MoveRight(bool extend_selection)
{
    return MoveCaret((std::min)(caret_ + 1, text_.size()), extend_selection);
}

bool TextEditState::MoveHome(bool extend_selection)
{
    return MoveCaret(0, extend_selection);
}

bool TextEditState::MoveEnd(bool extend_selection)
{
    return MoveCaret(text_.size(), extend_selection);
}

size_t TextEditState::HitTest(
    IDWriteFactory* factory,
    IDWriteTextFormat* format,
    D2D1_POINT_2F local_point,
    float width,
    float layout_height) const
{
    wil::com_ptr<IDWriteTextLayout> layout = CreateLayout(factory, format, DisplayText(), width, layout_height);
    if (layout == nullptr) {
        return text_.size();
    }

    BOOL trailing = FALSE;
    BOOL inside = FALSE;
    DWRITE_HIT_TEST_METRICS metric = {};
    layout->HitTestPoint(local_point.x, local_point.y, &trailing, &inside, &metric);
    return (std::min<size_t>(metric.textPosition + (trailing ? 1 : 0), text_.size()));
}

wil::com_ptr<IDWriteTextLayout> TextEditState::CreateLayout(
    IDWriteFactory* factory,
    IDWriteTextFormat* format,
    const std::wstring& value,
    float width,
    float layout_height) const
{
    wil::com_ptr<IDWriteTextLayout> layout;
    if (factory == nullptr || format == nullptr) {
        return layout;
    }
    factory->CreateTextLayout(
        value.c_str(),
        static_cast<UINT32>(value.size()),
        format,
        (std::max)(1.0f, width),
        (std::max)(1.0f, layout_height),
        layout.put());
    return layout;
}

bool TextEditState::CaretMetrics(IDWriteTextLayout* layout, D2D1_POINT_2F origin, D2D1_POINT_2F* top, D2D1_POINT_2F* bottom) const
{
    if (layout == nullptr || top == nullptr || bottom == nullptr) {
        return false;
    }

    FLOAT x = 0.0f;
    FLOAT y = 0.0f;
    DWRITE_HIT_TEST_METRICS metric = {};
    if (FAILED(layout->HitTestTextPosition(static_cast<UINT32>(caret_), FALSE, &x, &y, &metric))) {
        return false;
    }
    *top = D2D1::Point2F(origin.x + x, origin.y + y);
    *bottom = D2D1::Point2F(origin.x + x, origin.y + y + metric.height);
    return true;
}

std::vector<DWRITE_HIT_TEST_METRICS> TextEditState::SelectionMetrics(IDWriteTextLayout* layout, D2D1_POINT_2F origin) const
{
    std::vector<DWRITE_HIT_TEST_METRICS> metrics;
    if (layout == nullptr || !HasSelection()) {
        return metrics;
    }

    UINT32 count = 0;
    const UINT32 start = static_cast<UINT32>(SelectionStart());
    const UINT32 length = static_cast<UINT32>(SelectionEnd() - SelectionStart());
    layout->HitTestTextRange(start, length, origin.x, origin.y, nullptr, 0, &count);
    metrics.resize(count);
    if (metrics.empty() ||
        FAILED(layout->HitTestTextRange(start, length, origin.x, origin.y, metrics.data(), count, &count))) {
        metrics.clear();
        return metrics;
    }
    metrics.resize(count);
    return metrics;
}

void TextEditState::DeleteSelection()
{
    if (!HasSelection()) {
        return;
    }
    const size_t start = SelectionStart();
    text_.erase(start, SelectionEnd() - start);
    caret_ = start;
    anchor_ = start;
}
