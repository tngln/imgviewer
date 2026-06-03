#include "ui.menu.hpp"

#include <algorithm>
#include <cwchar>
#include <utility>

#include <d2d1helper.h>

#include "math.hpp"
#include "ui.theme.hpp"

namespace {

constexpr float kMenuWidth = 220.0f;
constexpr float kMenuItemHeight = 34.0f;
constexpr float kMenuSeparatorHeight = 9.0f;
constexpr float kMenuPadding = 6.0f;
constexpr float kMenuTextLeft = 34.0f;
constexpr float kMenuChildMarkLeft = 198.0f;

float ItemHeight(const MenuItem& item)
{
    return item.separator ? kMenuSeparatorHeight : kMenuItemHeight;
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
    selected_ = 0;
}

D2D1_POINT_2F MenuOverlay::Origin() const
{
    return origin_;
}

D2D1_SIZE_F MenuOverlay::DesiredSize() const
{
    float height = kMenuPadding * 2.0f;
    for (const MenuItem& item : items_) {
        height += ItemHeight(item);
    }
    return D2D1::SizeF(kMenuWidth, height);
}

D2D1_RECT_F MenuOverlay::Bounds() const
{
    const D2D1_SIZE_F size = DesiredSize();
    return D2D1::RectF(origin_.x, origin_.y, origin_.x + size.width, origin_.y + size.height);
}

const std::vector<MenuItem>& MenuOverlay::Items() const
{
    return items_;
}

void MenuOverlay::Draw(const UiDrawContext& context, UiElementState) const
{
    if (!open_) {
        return;
    }

    const UiDraw draw(context);
    const D2D1_RECT_F menu_rect = Bounds();
    draw.FillRoundedRect(D2D1::RoundedRect(menu_rect, 6.0f, 6.0f), ui_theme::color::kButtonDefault);
    draw.DrawRoundedRect(D2D1::RoundedRect(menu_rect, 6.0f, 6.0f), ui_theme::color::kBorder);

    for (size_t index = 0; index < items_.size(); ++index) {
        const MenuItem& item = items_[index];
        const D2D1_RECT_F rect = ItemRect(index);
        if (item.separator) {
            const float y = rect.top + rect.bottom;
            draw.DrawRect(D2D1::RectF(rect.left + 12.0f, y * 0.5f, rect.right - 12.0f, y * 0.5f + 0.5f), ui_theme::color::kBorder);
            continue;
        }
        if (index == selected_) {
            draw.FillRect(rect, ui_theme::color::kButtonHovered);
        }
        if (item.checked) {
            draw.DrawBodyText(L"\x2713", 1, D2D1::RectF(rect.left + 11.0f, rect.top + 1.0f, rect.left + 30.0f, rect.bottom), ui_theme::color::kAccent);
        }
        const D2D1_COLOR_F text_color = item.enabled ? ui_theme::color::kBodyText : ui_theme::color::kButtonDisabledContent;
        draw.DrawBodyText(item.text, static_cast<UINT32>(wcslen(item.text)), D2D1::RectF(rect.left + kMenuTextLeft, rect.top + 3.0f, rect.right - 22.0f, rect.bottom), text_color, D2D1_DRAW_TEXT_OPTIONS_CLIP);
        if (!item.children.empty()) {
            draw.DrawBodyText(L">", 1, D2D1::RectF(rect.left + kMenuChildMarkLeft, rect.top + 3.0f, rect.right, rect.bottom), text_color);
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
            const ImgViewerAction action = items_[item].action;
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
            const ImgViewerAction action = items_[selected_].action;
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
    return D2D1::RectF(origin_.x + kMenuPadding, top, origin_.x + kMenuWidth - kMenuPadding, top + height);
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
