#include "ui.table.hpp"

#include <algorithm>
#include <cwchar>
#include <utility>

#include <d2d1helper.h>

#include "math.hpp"
#include "ui.events.hpp"
#include "ui.theme.hpp"

namespace {

constexpr float kHeaderHeight = 20.0f;
constexpr float kMinimumRowHeight = 14.0f;
constexpr float kSeparatorOpacity = 0.55f;

D2D1_COLOR_F WithOpacity(D2D1_COLOR_F color, float opacity)
{
    color.a = opacity;
    return color;
}

} // namespace

Table::Table(UiElementMetadata metadata) : UiElement(metadata)
{
}

void Table::SetColumns(std::vector<TableColumn> columns)
{
    columns_ = std::move(columns);
}

void Table::SetRows(std::vector<TableRow> rows)
{
    rows_ = std::move(rows);
    if (!IsSelectableRow(selected_index_)) {
        selected_index_ = NextSelectable(0);
    }
    scroll_offset_ = std::clamp(scroll_offset_, 0.0f, MaxScrollOffset());
    EnsureSelectionVisible();
}

void Table::SetHeaderVisible(bool visible)
{
    header_visible_ = visible;
}

void Table::SetSelectionEnabled(bool enabled)
{
    selection_enabled_ = enabled;
    SetFocusable(enabled);
}

void Table::SetRowHeight(float row_height)
{
    row_height_ = (std::max)(kMinimumRowHeight, row_height);
}

void Table::SetCellPadding(float padding)
{
    cell_padding_ = (std::max)(0.0f, padding);
}

void Table::SetSeparatorsVisible(bool visible)
{
    separators_visible_ = visible;
}

size_t Table::SelectedIndex() const
{
    return selected_index_;
}

UiAction Table::SelectedAction() const
{
    return selected_index_ < rows_.size() ? rows_[selected_index_].action : kUiActionNone;
}

void Table::SetSelectedIndex(size_t index)
{
    selected_index_ = IsSelectableRow(index) ? index : NextSelectable(0);
    EnsureSelectionVisible();
}

D2D1_SIZE_F Table::Measure(const UiDrawContext&, D2D1_SIZE_F available_size) const
{
    const float header_height = header_visible_ ? kHeaderHeight : 0.0f;
    const float height = header_height + row_height_ * static_cast<float>(rows_.size());
    return D2D1::SizeF((std::max)(1.0f, available_size.width), height);
}

void Table::Render(const UiDrawContext& context, UiRootState state) const
{
    const UiDraw draw(context);
    const D2D1_RECT_F rect = Rect();
    const std::vector<float> widths = ColumnWidths(math::RectWidth(rect));
    const UiElementState element_state = VisualState(state);
    const D2D1_RECT_F body = BodyRect();

    if (header_visible_) {
        const D2D1_RECT_F header = HeaderRect();
        draw.FillRect(header, ui_theme::color::kButtonDefault);
        for (size_t column = 0; column < columns_.size(); ++column) {
            const D2D1_RECT_F cell = CellRect(header, column, widths);
            draw.DrawBodyText(
                columns_[column].header,
                static_cast<UINT32>(std::wcslen(columns_[column].header)),
                D2D1::RectF(cell.left + cell_padding_, cell.top + ui_theme::metrics::kTextRowTopOffset, cell.right - cell_padding_, cell.bottom),
                ui_theme::color::kMutedText,
                D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
        if (separators_visible_) {
            draw.DrawRect(D2D1::RectF(header.left, header.bottom - ui_theme::metrics::kHalfPixel, header.right, header.bottom), WithOpacity(ui_theme::color::kBorder, kSeparatorOpacity));
        }
    }

    if (context.d2d_context != nullptr) {
        context.d2d_context->PushAxisAlignedClip(body, D2D1_ANTIALIAS_MODE_ALIASED);
    }

    for (size_t row = 0; row < rows_.size(); ++row) {
        const D2D1_RECT_F row_rect = RowRect(row);
        if (row_rect.bottom < body.top || row_rect.top > body.bottom) {
            continue;
        }
        const bool selected = selection_enabled_ && row == selected_index_;
        const bool hovered = selection_enabled_ && state.hovered == Id() && row == hovered_index_;
        if (selected) {
            draw.FillRect(row_rect, ui_theme::color::kButtonPressed);
        } else if (hovered && rows_[row].enabled && !rows_[row].section) {
            draw.FillRect(row_rect, ui_theme::color::kButtonHovered);
        }

        const D2D1_COLOR_F text_color = !rows_[row].enabled
            ? ui_theme::color::kButtonDisabledContent
            : rows_[row].section ? ui_theme::color::kMutedText : ui_theme::color::kBodyText;
        for (size_t column = 0; column < columns_.size(); ++column) {
            const std::wstring empty;
            const std::wstring& text = column < rows_[row].cells.size() ? rows_[row].cells[column] : empty;
            const D2D1_RECT_F cell = CellRect(row_rect, column, widths);
            draw.DrawBodyText(
                text.c_str(),
                static_cast<UINT32>(text.size()),
                D2D1::RectF(cell.left + cell_padding_, cell.top + ui_theme::metrics::kTextRowTopOffset, cell.right - cell_padding_, cell.bottom),
                text_color,
                D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
        if (separators_visible_) {
            draw.DrawRect(D2D1::RectF(row_rect.left, row_rect.bottom - ui_theme::metrics::kHalfPixel, row_rect.right, row_rect.bottom), WithOpacity(ui_theme::color::kBorder, kSeparatorOpacity));
        }
    }

    if (context.d2d_context != nullptr) {
        context.d2d_context->PopAxisAlignedClip();
    }

    if (element_state.active && selection_enabled_) {
        draw.DrawRect(rect, ui_theme::color::kAccent, ui_theme::metrics::kActiveStrokeWidth);
    }
}

UiEventResult Table::OnPointerEvent(const UiPointerEvent& event)
{
    if (!selection_enabled_ || !IsEnabled()) {
        return {};
    }
    if (event.type == UiEventType::PointerWheel) {
        const float old_offset = scroll_offset_;
        scroll_offset_ = std::clamp(scroll_offset_ - static_cast<float>(event.wheel_delta) / static_cast<float>(WHEEL_DELTA) * row_height_ * 2.0f, 0.0f, MaxScrollOffset());
        return UiEventResult{.handled = true, .needs_render = old_offset != scroll_offset_};
    }
    if (event.button != UiPointerButton::Left &&
        (event.type == UiEventType::PointerDown || event.type == UiEventType::PointerUp)) {
        return {};
    }

    if (event.type == UiEventType::PointerMove) {
        const size_t old_hovered = hovered_index_;
        const size_t row = RowAt(event.point);
        hovered_index_ = IsSelectableRow(row) ? row : kNoSelection;
        return UiEventResult{.handled = true, .needs_render = old_hovered != hovered_index_};
    }

    if (event.type == UiEventType::PointerDown) {
        return UiEventResult{
            .handled = true,
            .needs_render = true,
            .capture = UiCaptureRequest::Capture,
            .focus = UiFocusRequest::FocusTarget,
            .focus_target = Id(),
        };
    }

    if (event.type == UiEventType::PointerUp && event.captured == Id()) {
        const size_t row = RowAt(event.point);
        if (event.target == Id() && IsSelectableRow(row)) {
            const bool changed = row != selected_index_;
            selected_index_ = row;
            EnsureSelectionVisible();
            return UiEventResult{
                .handled = true,
                .needs_render = true,
                .capture = UiCaptureRequest::Release,
                .value_changed = changed,
                .effect_target = Id(),
            };
        }
        return UiEventResult{.handled = true, .needs_render = true, .capture = UiCaptureRequest::Release};
    }

    return {};
}

UiEventResult Table::OnKeyEvent(const UiKeyEvent& event)
{
    if (!selection_enabled_ || !IsEnabled() || event.type != UiEventType::KeyDown) {
        return {};
    }

    size_t next = selected_index_;
    if (event.virtual_key == VK_HOME) {
        next = NextSelectable(0);
    } else if (event.virtual_key == VK_END) {
        next = PreviousSelectable(rows_.size());
    } else if (event.virtual_key == VK_DOWN) {
        next = NextSelectable(selected_index_ == kNoSelection ? 0 : selected_index_ + 1);
    } else if (event.virtual_key == VK_UP) {
        next = PreviousSelectable(selected_index_ == kNoSelection ? rows_.size() : selected_index_);
    } else {
        return {};
    }

    if (!IsSelectableRow(next)) {
        return UiEventResult{.handled = true};
    }
    const bool changed = next != selected_index_;
    selected_index_ = next;
    EnsureSelectionVisible();
    return UiEventResult{
        .handled = true,
        .needs_render = true,
        .value_changed = changed,
        .effect_target = Id(),
    };
}

D2D1_RECT_F Table::HeaderRect() const
{
    const D2D1_RECT_F rect = Rect();
    return D2D1::RectF(rect.left, rect.top, rect.right, rect.top + (header_visible_ ? kHeaderHeight : 0.0f));
}

D2D1_RECT_F Table::BodyRect() const
{
    const D2D1_RECT_F rect = Rect();
    const float top = rect.top + (header_visible_ ? kHeaderHeight : 0.0f);
    return D2D1::RectF(rect.left, top, rect.right, rect.bottom);
}

D2D1_RECT_F Table::RowRect(size_t index) const
{
    const D2D1_RECT_F rect = Rect();
    const float top = rect.top + (header_visible_ ? kHeaderHeight : 0.0f) + row_height_ * static_cast<float>(index) - EffectiveScrollOffset();
    return D2D1::RectF(rect.left, top, rect.right, top + row_height_);
}

D2D1_RECT_F Table::CellRect(D2D1_RECT_F row, size_t column_index, const std::vector<float>& widths) const
{
    float left = row.left;
    for (size_t index = 0; index < column_index && index < widths.size(); ++index) {
        left += widths[index];
    }
    const float width = column_index < widths.size() ? widths[column_index] : 0.0f;
    return D2D1::RectF(left, row.top, left + width, row.bottom);
}

size_t Table::RowAt(D2D1_POINT_2F point) const
{
    if (!math::Contains(Rect(), point)) {
        return rows_.size();
    }
    const float body_top = BodyRect().top;
    if (point.y < body_top) {
        return rows_.size();
    }
    const size_t index = static_cast<size_t>((point.y - body_top + EffectiveScrollOffset()) / row_height_);
    return index < rows_.size() ? index : rows_.size();
}

float Table::BodyContentHeight() const
{
    return row_height_ * static_cast<float>(rows_.size());
}

float Table::BodyViewportHeight() const
{
    return (std::max)(0.0f, math::RectHeight(BodyRect()));
}

float Table::MaxScrollOffset() const
{
    return (std::max)(0.0f, BodyContentHeight() - BodyViewportHeight());
}

float Table::EffectiveScrollOffset() const
{
    return std::clamp(scroll_offset_, 0.0f, MaxScrollOffset());
}

void Table::EnsureSelectionVisible()
{
    if (!IsSelectableRow(selected_index_)) {
        return;
    }
    const float viewport_height = BodyViewportHeight();
    if (viewport_height <= 0.0f) {
        return;
    }
    const float row_top = row_height_ * static_cast<float>(selected_index_);
    const float row_bottom = row_top + row_height_;
    if (row_top < scroll_offset_) {
        scroll_offset_ = row_top;
    } else if (row_bottom > scroll_offset_ + viewport_height) {
        scroll_offset_ = row_bottom - viewport_height;
    }
    scroll_offset_ = std::clamp(scroll_offset_, 0.0f, MaxScrollOffset());
}

bool Table::IsSelectableRow(size_t index) const
{
    return selection_enabled_ && index < rows_.size() && rows_[index].enabled && !rows_[index].section;
}

size_t Table::NextSelectable(size_t start) const
{
    for (size_t index = start; index < rows_.size(); ++index) {
        if (IsSelectableRow(index)) {
            return index;
        }
    }
    return kNoSelection;
}

size_t Table::PreviousSelectable(size_t start) const
{
    size_t index = (std::min)(start, rows_.size());
    while (index > 0) {
        --index;
        if (IsSelectableRow(index)) {
            return index;
        }
    }
    return kNoSelection;
}

std::vector<float> Table::ColumnWidths(float total_width) const
{
    std::vector<float> widths(columns_.size(), 0.0f);
    float fixed_width = 0.0f;
    size_t fill_count = 0;
    for (size_t index = 0; index < columns_.size(); ++index) {
        if (columns_[index].fill) {
            ++fill_count;
        } else {
            widths[index] = (std::max)(0.0f, columns_[index].width);
            fixed_width += widths[index];
        }
    }

    const float fill_width = fill_count > 0
        ? (std::max)(0.0f, total_width - fixed_width) / static_cast<float>(fill_count)
        : 0.0f;
    for (size_t index = 0; index < columns_.size(); ++index) {
        if (columns_[index].fill) {
            widths[index] = fill_width;
        }
    }
    return widths;
}
