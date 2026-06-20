/// <reference types="../types/imgviewer" />

import { contains, drawText, type Rect } from "./common";

type MenuPanel = { items: PopupMenuItem[]; x: number; y: number; width: number; depth: number };
type MenuLayout = { panels: MenuPanel[]; width: number; height: number };

const menu = {
  selectedPath: [] as number[],
  lastKind: "",
};

const dropdown = {
  hoverIndex: -1,
};

const row = {
  menu: 22,
  separator: 7,
  padding: 4,
  minWidth: 128,
  dropdown: 22,
};

const colors = {
  fill: "#FFFFFFFF",
  stroke: "#FFD8DEE8",
  hover: "#FFEAF1FF",
  text: "#FF172033",
  muted: "#FF697386",
  disabled: "#FF9AA4B2",
  accent: "#FF2D6CDF",
};

function resetFor(state: PopupState): void {
  if (menu.lastKind === state.kind) return;
  menu.lastKind = state.kind;
  menu.selectedPath = [];
  dropdown.hoverIndex = -1;
}

function itemHeight(item: PopupMenuItem): number {
  return item.separator ? row.separator : row.menu;
}

function preferredWidth(items: PopupMenuItem[]): number {
  let width = row.minWidth;
  for (const item of items) {
    if (item.separator) continue;
    width = Math.max(width, 28 + item.text.length * 7 + (item.children.length > 0 ? 18 : 0));
  }
  return width;
}

function menuHeight(items: PopupMenuItem[]): number {
  return row.padding * 2 + items.reduce((sum, item) => sum + itemHeight(item), 0);
}

function firstEnabled(items: PopupMenuItem[]): number {
  const found = items.findIndex((item) => !item.separator && item.enabled);
  return found >= 0 ? found : 0;
}

function normalizeSelection(items: PopupMenuItem[]): void {
  if (items.length === 0) {
    menu.selectedPath = [];
    return;
  }
  if (menu.selectedPath.length === 0 || menu.selectedPath[0] >= items.length || items[menu.selectedPath[0]].separator || !items[menu.selectedPath[0]].enabled) {
    menu.selectedPath = [firstEnabled(items)];
  }
}

function childItemsAt(items: PopupMenuItem[], depth: number): PopupMenuItem[] | undefined {
  let current = items;
  for (let d = 0; d < depth; ++d) {
    const index = menu.selectedPath[d];
    if (index === undefined || index >= current.length) return undefined;
    current = current[index].children;
  }
  return current;
}

function layoutMenu(items: PopupMenuItem[]): MenuLayout {
  normalizeSelection(items);
  const panels: MenuPanel[] = [];
  let current: PopupMenuItem[] | undefined = items;
  let x = 0;
  let y = 0;
  let width = 0;
  let height = 0;
  for (let depth = 0; current && current.length > 0; ++depth) {
    const panelWidth = preferredWidth(current);
    panels.push({ items: current, x, y, width: panelWidth, depth });
    width = Math.max(width, x + panelWidth);
    height = Math.max(height, y + menuHeight(current));
    const selected = menu.selectedPath[depth];
    if (selected === undefined || selected >= current.length || current[selected].children.length === 0) break;
    let childY = y + row.padding;
    for (let index = 0; index < selected; ++index) childY += itemHeight(current[index]);
    current = current[selected].children;
    x += panelWidth - row.padding;
    y = childY - row.padding;
    if (menu.selectedPath[depth + 1] === undefined) menu.selectedPath[depth + 1] = firstEnabled(current);
  }
  return { panels, width: Math.max(1, width), height: Math.max(1, height) };
}

function itemRect(panel: MenuPanel, index: number): Rect {
  let top = panel.y + row.padding;
  for (let i = 0; i < index; ++i) top += itemHeight(panel.items[i]);
  return { x: panel.x + row.padding, y: top, width: panel.width - row.padding * 2, height: itemHeight(panel.items[index]) };
}

function panelAt(layout: MenuLayout, x: number, y: number): MenuPanel | undefined {
  for (let i = layout.panels.length - 1; i >= 0; --i) {
    const panel = layout.panels[i];
    if (contains({ x: panel.x, y: panel.y, width: panel.width, height: menuHeight(panel.items) }, x, y)) return panel;
  }
  return undefined;
}

function indexAt(panel: MenuPanel, x: number, y: number): number {
  for (let index = 0; index < panel.items.length; ++index) {
    if (contains(itemRect(panel, index), x, y)) return index;
  }
  return -1;
}

function activateMenuItem(item: PopupMenuItem): PopupEventResult {
  return {
    handled: true,
    close: true,
    action: item.action,
    actionValue: item.actionValue,
    actionArg: item.actionArg,
  };
}

function moveSelection(items: PopupMenuItem[], delta: number): void {
  const current = childItemsAt(items, Math.max(0, menu.selectedPath.length - 1));
  if (!current || current.length === 0) return;
  const depth = Math.max(0, menu.selectedPath.length - 1);
  let next = menu.selectedPath[depth] ?? 0;
  for (let tries = 0; tries < current.length; ++tries) {
    next = delta > 0 ? (next + 1) % current.length : next === 0 ? current.length - 1 : next - 1;
    if (!current[next].separator && current[next].enabled) {
      menu.selectedPath = menu.selectedPath.slice(0, depth + 1);
      menu.selectedPath[depth] = next;
      return;
    }
  }
}

function selectedMenuItem(items: PopupMenuItem[]): PopupMenuItem | undefined {
  const current = childItemsAt(items, Math.max(0, menu.selectedPath.length - 1));
  const index = menu.selectedPath[Math.max(0, menu.selectedPath.length - 1)];
  return current && index !== undefined ? current[index] : undefined;
}

function measure(state: PopupState): PopupMeasureResult {
  resetFor(state);
  if (state.kind === "menu") {
    const layout = layoutMenu(state.items);
    return { width: layout.width, height: layout.height };
  }
  if (state.kind === "dropdown") {
    return { width: Math.max(1, state.width), height: Math.max(1, state.options.length * row.dropdown) };
  }
  return { width: 1, height: 1 };
}

function renderMenu(canvas: CanvasApi, state: Extract<PopupState, { kind: "menu" }>): void {
  const layout = layoutMenu(state.items);
  for (const panel of layout.panels) {
    canvas.fillRect(panel.x, panel.y, panel.width, menuHeight(panel.items), colors.fill);
    canvas.strokeRect(panel.x, panel.y, panel.width, menuHeight(panel.items), colors.stroke, 1);
    for (let index = 0; index < panel.items.length; ++index) {
      const item = panel.items[index];
      const rect = itemRect(panel, index);
      if (item.separator) {
        canvas.strokeLine(rect.x + 6, rect.y + rect.height / 2, rect.x + rect.width - 6, rect.y + rect.height / 2, colors.stroke, 1);
        continue;
      }
      if (menu.selectedPath[panel.depth] === index) {
        canvas.fillRect(rect.x, rect.y, rect.width, rect.height, colors.hover);
      }
      if (item.checked) drawText(canvas, "x", rect.x + 5, rect.y + 4, 12, colors.accent, 16);
      drawText(canvas, item.text, rect.x + 20, rect.y + 4, rect.width - 34, item.enabled ? colors.text : colors.disabled, 16);
      if (item.children.length > 0) drawText(canvas, ">", rect.x + rect.width - 14, rect.y + 4, 10, item.enabled ? colors.muted : colors.disabled, 16);
    }
  }
}

function renderDropdown(canvas: CanvasApi, state: Extract<PopupState, { kind: "dropdown" }>): void {
  canvas.fillRect(0, 0, state.width, Math.max(1, state.options.length * row.dropdown), colors.fill);
  canvas.strokeRect(0, 0, state.width, Math.max(1, state.options.length * row.dropdown), colors.stroke, 1);
  for (let index = 0; index < state.options.length; ++index) {
    const y = index * row.dropdown;
    if (index === dropdown.hoverIndex || index === state.selectedIndex) {
      canvas.fillRect(0, y, state.width, row.dropdown, colors.hover);
    }
    drawText(canvas, state.options[index].text, 7, y + 4, state.width - 14, colors.text, 16);
  }
}

function render(canvas: CanvasApi, _env: RenderEnvironment, state: PopupState): void {
  resetFor(state);
  if (state.kind === "menu") renderMenu(canvas, state);
  else if (state.kind === "dropdown") renderDropdown(canvas, state);
}

function pointer(event: UiPointerEvent, state: PopupState): PopupEventResult | void {
  resetFor(state);
  if (state.kind === "menu") {
    const layout = layoutMenu(state.items);
    const panel = panelAt(layout, event.x, event.y);
    if (!panel) return { handled: false };
    const index = indexAt(panel, event.x, event.y);
    if (index < 0) return { handled: true };
    const item = panel.items[index];
    if (item.separator || !item.enabled) return { handled: true };
    if (event.type === "move") {
      menu.selectedPath = menu.selectedPath.slice(0, panel.depth + 1);
      menu.selectedPath[panel.depth] = index;
      if (item.children.length > 0) menu.selectedPath[panel.depth + 1] = firstEnabled(item.children);
      return { handled: true, invalidate: true };
    }
    if (event.type === "up" || event.type === "down") {
      menu.selectedPath = menu.selectedPath.slice(0, panel.depth + 1);
      menu.selectedPath[panel.depth] = index;
      if (item.children.length > 0) {
        menu.selectedPath[panel.depth + 1] = firstEnabled(item.children);
        return { handled: true, invalidate: true };
      }
      return activateMenuItem(item);
    }
    return { handled: true };
  }
  if (state.kind === "dropdown") {
    const index = Math.floor(event.y / row.dropdown);
    if (index < 0 || index >= state.options.length) return { handled: true };
    if (event.type === "move") {
      dropdown.hoverIndex = index;
      return { handled: true, invalidate: true };
    }
    if (event.type === "up" || event.type === "down") {
      const option = state.options[index];
      return { handled: true, close: true, selectedIndex: index, action: option.action, actionValue: option.actionValue, actionArg: option.actionArg };
    }
    return { handled: true };
  }
}

function key(event: UiKeyEvent, state: PopupState): PopupEventResult | void {
  resetFor(state);
  if (event.type !== "down") return;
  if (state.kind === "menu") {
    layoutMenu(state.items);
    if (event.virtualKey === 40) {
      moveSelection(state.items, 1);
      return { handled: true, invalidate: true };
    }
    if (event.virtualKey === 38) {
      moveSelection(state.items, -1);
      return { handled: true, invalidate: true };
    }
    if (event.virtualKey === 37) {
      if (menu.selectedPath.length > 1) menu.selectedPath.pop();
      return { handled: true, invalidate: true };
    }
    if (event.virtualKey === 39) {
      const item = selectedMenuItem(state.items);
      if (item && item.children.length > 0) menu.selectedPath.push(firstEnabled(item.children));
      return { handled: true, invalidate: true };
    }
    if (event.virtualKey === 13 || event.virtualKey === 32) {
      const item = selectedMenuItem(state.items);
      if (!item || item.separator || !item.enabled) return { handled: true };
      if (item.children.length > 0) {
        menu.selectedPath.push(firstEnabled(item.children));
        return { handled: true, invalidate: true };
      }
      return activateMenuItem(item);
    }
    return { handled: true };
  }
  if (state.kind === "dropdown") {
    let index = state.selectedIndex;
    if (event.virtualKey === 40) index = Math.min(state.options.length - 1, index + 1);
    else if (event.virtualKey === 38) index = Math.max(0, index - 1);
    else if (event.virtualKey !== 13 && event.virtualKey !== 32) return { handled: true };
    const option = state.options[index];
    return {
      handled: true,
      close: event.virtualKey === 13 || event.virtualKey === 32,
      selectedIndex: index,
      action: option?.action,
      actionValue: option?.actionValue,
      actionArg: option?.actionArg,
      invalidate: true,
    };
  }
}

globalThis.imgviewerPopupUi = { measure, render, pointer, key };
