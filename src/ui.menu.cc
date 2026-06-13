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

float MenuHeight(const std::vector<MenuItem>& items)
{
    float height = kMenuPadding * 2.0f;
    for (const MenuItem& item : items) {
        height += ItemHeight(item);
    }
    return height;
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
    preferred_widths_.clear();
    selected_path_.clear();
    selected_path_.push_back(items_.empty() ? 0 : items_.size() - 1);
    MoveSelection(1);
}

void MenuOverlay::Close()
{
    open_ = false;
    items_.clear();
    preferred_widths_.clear();
    selected_path_.clear();
}

D2D1_POINT_2F MenuOverlay::Origin() const
{
    return origin_;
}

D2D1_SIZE_F MenuOverlay::MeasuredSize() const
{
    if (!open_) {
        return D2D1::SizeF();
    }

    D2D1_RECT_F bounds = PanelRect(0);
    for (size_t panel = 1; panel < Panels().size(); ++panel) {
        const D2D1_RECT_F rect = PanelRect(panel);
        bounds.left = (std::min)(bounds.left, rect.left);
        bounds.top = (std::min)(bounds.top, rect.top);
        bounds.right = (std::max)(bounds.right, rect.right);
        bounds.bottom = (std::max)(bounds.bottom, rect.bottom);
    }
    return D2D1::SizeF(bounds.right - origin_.x, bounds.bottom - origin_.y);
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
    preferred_widths_.clear();
    for (size_t depth = 0;; ++depth) {
        const std::vector<MenuItem>* items = ItemsAtDepth(depth);
        if (items == nullptr) {
            break;
        }
        float width = kMenuMinWidth;
        for (const MenuItem& item : *items) {
            width = (std::max)(width, MenuItemPreferredWidth(item, context));
        }
        preferred_widths_.push_back(width);
    }
}

float MenuOverlay::PreferredWidth() const
{
    return preferred_widths_.empty() ? kMenuMinWidth : preferred_widths_[0];
}

void MenuOverlay::Render(const UiDrawContext& context, UiRootState) const
{
    if (!open_) {
        return;
    }

    Measure(context, context.viewport_size);
    const UiDraw draw(context);

    for (size_t panel = 0; panel < Panels().size(); ++panel) {
        const std::vector<MenuItem>* items = ItemsAtDepth(panel);
        if (items == nullptr) {
            continue;
        }

        const D2D1_RECT_F menu_rect = PanelRect(panel);
        draw.FillRoundedRect(D2D1::RoundedRect(menu_rect, kMenuCornerRadius, kMenuCornerRadius), ui_theme::color::kButtonDefault);
        draw.DrawRoundedRect(D2D1::RoundedRect(menu_rect, kMenuCornerRadius, kMenuCornerRadius), ui_theme::color::kBorder);

        for (size_t index = 0; index < items->size(); ++index) {
            const MenuItem& item = (*items)[index];
            const D2D1_RECT_F rect = ItemRect(panel, index);
            if (item.separator) {
                const float y = rect.top + rect.bottom;
                draw.DrawRect(
                    D2D1::RectF(
                        rect.left + ui_theme::metrics::kStandardGap,
                        y * 0.5f,
                        rect.right - ui_theme::metrics::kStandardGap,
                        y * 0.5f + ui_theme::metrics::kHalfPixel),
                    ui_theme::color::kBorder);
                continue;
            }
            if (panel < selected_path_.size() && index == selected_path_[panel]) {
                draw.FillRect(rect, ui_theme::color::kButtonHovered);
            }
            if (item.checked) {
                draw.DrawBodyText(
                    L"\x2713",
                    D2D1::RectF(
                        rect.left + kMenuCheckmarkLeft,
                        rect.top + ui_theme::metrics::kHalfPixel,
                        rect.left + kMenuCheckmarkRight,
                        rect.bottom),
                    ui_theme::color::kAccent);
            }
            const D2D1_COLOR_F text_color = item.enabled ? ui_theme::color::kBodyText : ui_theme::color::kButtonDisabledContent;
            const float text_right = item.children.empty()
                ? rect.right - kMenuTextRight
                : rect.right - kMenuTextRight - kMenuChildMarkWidth - kMenuChildMarkGap;
            draw.DrawBodyText(
                item.text,
                D2D1::RectF(rect.left + kMenuTextLeft, rect.top + kMenuTextTopOffset, text_right, rect.bottom),
                text_color,
                D2D1_DRAW_TEXT_OPTIONS_CLIP);
            if (!item.children.empty()) {
                draw.DrawBodyText(
                    L">",
                    D2D1::RectF(rect.right - kMenuTextRight - kMenuChildMarkWidth, rect.top + kMenuTextTopOffset, rect.right, rect.bottom),
                    text_color);
            }
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
            return UiEventResult{.handled = true};
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
        const size_t panel = PanelAt(event.point);
        const size_t item = ItemAt(panel, event.point);
        const std::vector<MenuItem>* items = ItemsAtDepth(panel);
        if (items != nullptr && item < items->size() && !(*items)[item].separator && (*items)[item].enabled) {
            const bool has_children = !(*items)[item].children.empty();
            const bool child_open = selected_path_.size() > panel + 1;
            const bool changed = panel >= selected_path_.size() ||
                selected_path_[panel] != item ||
                (has_children ? !child_open : selected_path_.size() != panel + 1);
            selected_path_.resize(panel + 1);
            selected_path_[panel] = item;
            if (has_children) {
                OpenChild(panel);
            }
            return UiEventResult{.handled = true};
        }
        return Contains(event.point) ? UiEventResult{.handled = true} : UiEventResult{};
    }
    if (event.type == UiEventType::PointerDown && !Contains(event.point)) {
        Close();
        return UiEventResult{.handled = true};
    }
    if (event.type == UiEventType::PointerUp && Contains(event.point)) {
        const size_t panel = PanelAt(event.point);
        const size_t item = ItemAt(panel, event.point);
        const std::vector<MenuItem>* items = ItemsAtDepth(panel);
        if (items != nullptr && item < items->size() && !(*items)[item].separator && (*items)[item].enabled) {
            selected_path_.resize(panel + 1);
            selected_path_[panel] = item;
            if (!(*items)[item].children.empty()) {
                OpenChild(panel);
                return UiEventResult{.handled = true};
            }
            const UiAction action = (*items)[item].action;
            Close();
            return UiEventResult{.handled = true, .action = action};
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
        return UiEventResult{.handled = true};
    }
    if (event.virtual_key == VK_DOWN || event.virtual_key == VK_UP) {
        MoveSelection(event.virtual_key == VK_DOWN ? 1 : -1);
        return UiEventResult{.handled = true};
    }
    if (event.virtual_key == VK_LEFT) {
        if (selected_path_.size() > 1) {
            TrimToDepth(selected_path_.size() - 2);
            return UiEventResult{.handled = true};
        }
        return UiEventResult{.handled = true};
    }
    if (event.virtual_key == VK_RIGHT) {
        const size_t depth = selected_path_.empty() ? 0 : selected_path_.size() - 1;
        const MenuItem* item = SelectedItem(depth);
        if (item != nullptr && item->enabled && !item->children.empty()) {
            OpenChild(depth);
            return UiEventResult{.handled = true};
        }
        return UiEventResult{.handled = true};
    }
    if (event.virtual_key == VK_RETURN || event.virtual_key == VK_SPACE) {
        const size_t depth = selected_path_.empty() ? 0 : selected_path_.size() - 1;
        const MenuItem* item = SelectedItem(depth);
        if (item != nullptr && item->enabled && !item->separator) {
            if (!item->children.empty()) {
                OpenChild(depth);
                return UiEventResult{.handled = true};
            }
            const UiAction action = item->action;
            Close();
            return UiEventResult{.handled = true, .action = action};
        }
    }
    return UiEventResult{.handled = true};
}

bool MenuOverlay::Contains(D2D1_POINT_2F point) const
{
    if (!open_) {
        return false;
    }
    for (size_t panel = 0; panel < Panels().size(); ++panel) {
        if (math::Contains(PanelRect(panel), point)) {
            return true;
        }
    }
    return false;
}

std::vector<MenuOverlay::Panel> MenuOverlay::Panels() const
{
    std::vector<Panel> panels;
    const std::vector<MenuItem>* items = &items_;
    D2D1_POINT_2F panel_origin = origin_;
    for (size_t depth = 0; items != nullptr; ++depth) {
        const float width = depth < preferred_widths_.size() ? preferred_widths_[depth] : kMenuMinWidth;
        panels.push_back(Panel{.items = items, .origin = panel_origin, .width = width});

        if (depth >= selected_path_.size()) {
            break;
        }
        const size_t selected = selected_path_[depth];
        if (selected >= items->size() || (*items)[selected].children.empty()) {
            break;
        }
        float item_top = panel_origin.y + kMenuPadding;
        for (size_t index = 0; index < selected; ++index) {
            item_top += ItemHeight((*items)[index]);
        }
        items = &(*items)[selected].children;
        panel_origin = D2D1::Point2F(panel_origin.x + width - kMenuPadding, item_top - kMenuPadding);
    }
    return panels;
}

const std::vector<MenuItem>* MenuOverlay::ItemsAtDepth(size_t depth) const
{
    const std::vector<MenuItem>* items = &items_;
    for (size_t current = 0; current < depth; ++current) {
        if (current >= selected_path_.size()) {
            return nullptr;
        }
        const size_t selected = selected_path_[current];
        if (selected >= items->size() || (*items)[selected].children.empty()) {
            return nullptr;
        }
        items = &(*items)[selected].children;
    }
    return items;
}

const MenuItem* MenuOverlay::SelectedItem(size_t depth) const
{
    const std::vector<MenuItem>* items = ItemsAtDepth(depth);
    if (items == nullptr || depth >= selected_path_.size() || selected_path_[depth] >= items->size()) {
        return nullptr;
    }
    return &(*items)[selected_path_[depth]];
}

size_t MenuOverlay::PanelAt(D2D1_POINT_2F point) const
{
    for (size_t panel = Panels().size(); panel > 0; --panel) {
        if (math::Contains(PanelRect(panel - 1), point)) {
            return panel - 1;
        }
    }
    return Panels().size();
}

size_t MenuOverlay::ItemAt(size_t panel, D2D1_POINT_2F point) const
{
    const std::vector<MenuItem>* items = ItemsAtDepth(panel);
    if (items == nullptr) {
        return 0;
    }
    for (size_t index = 0; index < items->size(); ++index) {
        if (math::Contains(ItemRect(panel, index), point)) {
            return index;
        }
    }
    return items->size();
}

D2D1_RECT_F MenuOverlay::PanelRect(size_t panel) const
{
    const std::vector<Panel> panels = Panels();
    if (panel >= panels.size() || panels[panel].items == nullptr) {
        return D2D1::RectF(origin_.x, origin_.y, origin_.x, origin_.y);
    }
    const Panel& info = panels[panel];
    return D2D1::RectF(
        info.origin.x,
        info.origin.y,
        info.origin.x + info.width,
        info.origin.y + MenuHeight(*info.items));
}

D2D1_RECT_F MenuOverlay::ItemRect(size_t panel, size_t index) const
{
    const std::vector<Panel> panels = Panels();
    if (panel >= panels.size() || panels[panel].items == nullptr) {
        return D2D1::RectF(origin_.x, origin_.y, origin_.x, origin_.y);
    }
    const Panel& info = panels[panel];
    float top = info.origin.y + kMenuPadding;
    for (size_t current = 0; current < index && current < info.items->size(); ++current) {
        top += ItemHeight((*info.items)[current]);
    }
    const float height = index < info.items->size() ? ItemHeight((*info.items)[index]) : kMenuItemHeight;
    return D2D1::RectF(info.origin.x + kMenuPadding, top, info.origin.x + info.width - kMenuPadding, top + height);
}

void MenuOverlay::OpenChild(size_t depth)
{
    const MenuItem* item = SelectedItem(depth);
    if (item == nullptr || item->children.empty()) {
        TrimToDepth(depth);
        return;
    }
    selected_path_.resize(depth + 2);
    selected_path_[depth + 1] = item->children.empty() ? 0 : item->children.size() - 1;
    MoveSelection(1);
}

void MenuOverlay::TrimToDepth(size_t depth)
{
    if (selected_path_.empty()) {
        return;
    }
    selected_path_.resize((std::min)(depth + 1, selected_path_.size()));
}

void MenuOverlay::MoveSelection(int delta)
{
    if (items_.empty() || selected_path_.empty()) {
        return;
    }
    const size_t depth = selected_path_.size() - 1;
    const std::vector<MenuItem>* items = ItemsAtDepth(depth);
    if (items == nullptr || items->empty()) {
        return;
    }
    size_t next = selected_path_[depth] < items->size() ? selected_path_[depth] : items->size() - 1;
    for (size_t tries = 0; tries < items->size(); ++tries) {
        if (delta > 0) {
            next = (next + 1) % items->size();
        } else {
            next = next == 0 ? items->size() - 1 : next - 1;
        }
        if (!(*items)[next].separator && (*items)[next].enabled) {
            selected_path_[depth] = next;
            selected_path_.resize(depth + 1);
            return;
        }
    }
}
