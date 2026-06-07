#include "ui.menu.hpp"

#include <algorithm>
#include <cmath>
#include <cwchar>
#include <utility>

#include <d2d1helper.h>

#include "math.hpp"
#include "ui.text.hpp"
#include "ui.theme.hpp"

namespace {

constexpr float kMenuMinWidth = 110.0f;
constexpr float kMenuItemHeight = 17.0f;
constexpr float kMenuSeparatorHeight = 4.5f;
constexpr float kMenuPadding = 3.0f;
constexpr float kMenuTextLeft = 17.0f;
constexpr float kMenuTextRight = 14.0f;
constexpr float kMenuChildMarkWidth = 10.0f;
constexpr float kMenuChildMarkGap = 5.0f;
constexpr float kMenuCheckmarkLeft = 5.5f;
constexpr float kMenuCheckmarkRight = 15.0f;
constexpr float kMenuTextTopOffset = 1.5f;
constexpr float kMenuCornerRadius = 3.0f;

float ItemHeight(const MenuItem& item)
{
    return item.separator ? kMenuSeparatorHeight : kMenuItemHeight;
}

float MenuItemPreferredWidth(const MenuItem& item, const UiDrawContext& context)
{
    if (item.separator) {
        return 0.0f;
    }

    const ui_text::TextMetrics metrics = ui_text::MeasureText(
        context.dwrite_factory,
        context.body_text_format,
        item.text,
        static_cast<UINT32>(wcslen(item.text)));
    float width = kMenuPadding * 2.0f + kMenuTextLeft + std::ceil(metrics.width) + kMenuTextRight;
    if (!item.children.empty()) {
        width += kMenuChildMarkGap + kMenuChildMarkWidth;
    }
    return width;
}

} // namespace

bool MenuOverlay::IsOpen() const
{
    return open_;
}

void MenuOverlay::Open(D2D1_POINT_2F origin, std::vector<MenuItem> items)
{
    origin_ = origin;
    items_ = std::move(items);
    open_ = true;
    selected_ = 0;
    MoveSelection(1);
}

void MenuOverlay::Close()
{
    open_ = false;
    items_.clear();
    preferred_width_ = 0.0f;
    selected_ = 0;
}

D2D1_POINT_2F MenuOverlay::Origin() const
{
    return origin_;
}

D2D1_SIZE_F MenuOverlay::MeasuredSize() const
{
    float height = kMenuPadding * 2.0f;
    for (const MenuItem& item : items_) {
        height += ItemHeight(item);
    }
    return D2D1::SizeF(PreferredWidth(), height);
}

D2D1_SIZE_F MenuOverlay::Measure(const UiDrawContext& context, D2D1_SIZE_F) const
{
    UpdatePreferredWidth(context);
    return MeasuredSize();
}

D2D1_RECT_F MenuOverlay::Bounds() const
{
    const D2D1_SIZE_F size = MeasuredSize();
    return D2D1::RectF(origin_.x, origin_.y, origin_.x + size.width, origin_.y + size.height);
}

const std::vector<MenuItem>& MenuOverlay::Items() const
{
    return items_;
}

void MenuOverlay::UpdatePreferredWidth(const UiDrawContext& context) const
{
    float width = kMenuMinWidth;
    for (const MenuItem& item : items_) {
        width = (std::max)(width, MenuItemPreferredWidth(item, context));
    }
    preferred_width_ = width;
}

float MenuOverlay::PreferredWidth() const
{
    return preferred_width_ > 0.0f ? preferred_width_ : kMenuMinWidth;
}

void MenuOverlay::Render(const UiDrawContext& context, UiRootState) const
{
    if (!open_) {
        return;
    }

    Measure(context, context.viewport_size);
    const UiDraw draw(context);
    const D2D1_RECT_F menu_rect = Bounds();
    draw.FillRoundedRect(D2D1::RoundedRect(menu_rect, kMenuCornerRadius, kMenuCornerRadius), ui_theme::color::kButtonDefault);
    draw.DrawRoundedRect(D2D1::RoundedRect(menu_rect, kMenuCornerRadius, kMenuCornerRadius), ui_theme::color::kBorder);

    for (size_t index = 0; index < items_.size(); ++index) {
        const MenuItem& item = items_[index];
        const D2D1_RECT_F rect = ItemRect(index);
        if (item.separator) {
            const float y = rect.top + rect.bottom;
            draw.DrawRect(D2D1::RectF(rect.left + ui_theme::metrics::kStandardGap, y * 0.5f, rect.right - ui_theme::metrics::kStandardGap, y * 0.5f + ui_theme::metrics::kHalfPixel), ui_theme::color::kBorder);
            continue;
        }
        if (index == selected_) {
            draw.FillRect(rect, ui_theme::color::kButtonHovered);
        }
        if (item.checked) {
            draw.DrawBodyText(L"\x2713", 1, D2D1::RectF(rect.left + kMenuCheckmarkLeft, rect.top + ui_theme::metrics::kHalfPixel, rect.left + kMenuCheckmarkRight, rect.bottom), ui_theme::color::kAccent);
        }
        const D2D1_COLOR_F text_color = item.enabled ? ui_theme::color::kBodyText : ui_theme::color::kButtonDisabledContent;
        const float text_right = item.children.empty()
            ? rect.right - kMenuTextRight
            : rect.right - kMenuTextRight - kMenuChildMarkWidth - kMenuChildMarkGap;
        draw.DrawBodyText(item.text, static_cast<UINT32>(wcslen(item.text)), D2D1::RectF(rect.left + kMenuTextLeft, rect.top + kMenuTextTopOffset, text_right, rect.bottom), text_color, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        if (!item.children.empty()) {
            draw.DrawBodyText(L">", 1, D2D1::RectF(rect.right - kMenuTextRight - kMenuChildMarkWidth, rect.top + kMenuTextTopOffset, rect.right, rect.bottom), text_color);
        }
    }
}

UiEventResult MenuOverlay::OnInputEvent(const UiInputEvent& event)
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
    case UiEventType::Cancel:
    case UiEventType::OwnerDeactivated:
        if (open_) {
            Close();
            return UiEventResult{.handled = true, .needs_render = true};
        }
        return {};
    default:
        return {};
    }
}

UiEventResult MenuOverlay::OnPointerEvent(const UiPointerEvent& event)
{
    if (!open_) {
        return {};
    }
    if (event.type == UiEventType::PointerMove) {
        const size_t item = ItemAt(event.point);
        if (item < items_.size() && item != selected_ && !items_[item].separator && items_[item].enabled) {
            selected_ = item;
            return UiEventResult{.handled = true, .needs_render = true};
        }
        return Contains(event.point) ? UiEventResult{.handled = true} : UiEventResult{};
    }
    if (event.type == UiEventType::PointerDown && !Contains(event.point)) {
        Close();
        return UiEventResult{.handled = true, .needs_render = true};
    }
    if (event.type == UiEventType::PointerUp && Contains(event.point)) {
        const size_t item = ItemAt(event.point);
        if (item < items_.size() && !items_[item].separator && items_[item].enabled) {
            const UiAction action = items_[item].action;
            Close();
            return UiEventResult{.handled = true, .needs_render = true, .action = action};
        }
        return UiEventResult{.handled = true};
    }
    return Contains(event.point) ? UiEventResult{.handled = true} : UiEventResult{};
}

UiEventResult MenuOverlay::OnKeyEvent(const UiKeyEvent& event)
{
    if (!open_ || event.type != UiEventType::KeyDown) {
        return {};
    }
    if (event.virtual_key == VK_ESCAPE) {
        Close();
        return UiEventResult{.handled = true, .needs_render = true};
    }
    if (event.virtual_key == VK_DOWN || event.virtual_key == VK_UP) {
        MoveSelection(event.virtual_key == VK_DOWN ? 1 : -1);
        return UiEventResult{.handled = true, .needs_render = true};
    }
    if (event.virtual_key == VK_RETURN || event.virtual_key == VK_SPACE) {
        if (selected_ < items_.size() && items_[selected_].enabled && !items_[selected_].separator) {
            const UiAction action = items_[selected_].action;
            Close();
            return UiEventResult{.handled = true, .needs_render = true, .action = action};
        }
    }
    return UiEventResult{.handled = true};
}

bool MenuOverlay::Contains(D2D1_POINT_2F point) const
{
    if (!open_) {
        return false;
    }
    return math::Contains(Bounds(), point);
}

size_t MenuOverlay::ItemAt(D2D1_POINT_2F point) const
{
    for (size_t index = 0; index < items_.size(); ++index) {
        if (math::Contains(ItemRect(index), point)) {
            return index;
        }
    }
    return items_.size();
}

D2D1_RECT_F MenuOverlay::ItemRect(size_t index) const
{
    float top = origin_.y + kMenuPadding;
    for (size_t current = 0; current < index && current < items_.size(); ++current) {
        top += ItemHeight(items_[current]);
    }
    const float height = index < items_.size() ? ItemHeight(items_[index]) : kMenuItemHeight;
    return D2D1::RectF(origin_.x + kMenuPadding, top, origin_.x + PreferredWidth() - kMenuPadding, top + height);
}

void MenuOverlay::MoveSelection(int delta)
{
    if (items_.empty()) {
        return;
    }
    size_t next = selected_;
    for (size_t tries = 0; tries < items_.size(); ++tries) {
        if (delta > 0) {
            next = (next + 1) % items_.size();
        } else {
            next = next == 0 ? items_.size() - 1 : next - 1;
        }
        if (!items_[next].separator && items_[next].enabled) {
            selected_ = next;
            return;
        }
    }
}
