/// <reference types="../types/imgviewer" />

type Rect = { x: number; y: number; width: number; height: number };
type ControlKind = "button" | "toggle" | "radio" | "slider" | "filter";
type Control = Rect & {
  id: string;
  kind: ControlKind;
  key?: SettingKey;
  value?: number | boolean;
  min?: number;
  max?: number;
};

const colors = {
  bg: "#FFF8FAFC",
  text: "#FF172033",
  muted: "#FF5F6B7A",
  line: "#FFD6DCE5",
  panel: "#FFFFFFFF",
  hover: "#FFEAF2FF",
  active: "#FFD8E9FF",
  accent: "#FF2563EB",
  accentText: "#FFFFFFFF",
};

let hover = "";
let active = "";
let scrollY = 0;
let maxScrollY = 0;
let filter = "";
let draggingSlider = "";
let controls: Control[] = [];

function n(key: SettingKey): number {
  return Number(settings.get(key) ?? 0);
}

function b(key: SettingKey): boolean {
  return Boolean(settings.get(key));
}

function contains(rect: Rect, x: number, y: number): boolean {
  return x >= rect.x && y >= rect.y && x <= rect.x + rect.width && y <= rect.y + rect.height;
}

function hitTest(x: number, y: number): Control | undefined {
  for (let index = controls.length - 1; index >= 0; --index) {
    if (contains(controls[index], x, y)) {
      return controls[index];
    }
  }
  return undefined;
}

function contentY(y: number): number {
  return y - scrollY;
}

function addControl(control: Control): Control {
  controls.push(control);
  return control;
}

function drawText(canvas: CanvasApi, text: string, x: number, y: number, w: number, color = colors.text): void {
  canvas.fillText(text, x, y, w, 22, color);
}

function section(canvas: CanvasApi, title: string, x: number, y: number, w: number): number {
  canvas.fillRect(x, y, w, 1, colors.line);
  drawText(canvas, title, x, y + 14, w, colors.muted);
  return y + 44;
}

function button(canvas: CanvasApi, id: string, label: string, x: number, y: number, w: number, primary = false): void {
  const fill = active === id ? colors.active : hover === id ? colors.hover : primary ? colors.accent : colors.panel;
  const text = primary ? colors.accentText : colors.text;
  addControl({ id, kind: "button", x, y, width: w, height: 34 });
  canvas.fillRect(x, y, w, 34, fill);
  canvas.strokeRect(x, y, w, 34, primary ? colors.accent : colors.line, 1);
  drawText(canvas, label, x + 12, y + 8, w - 24, text);
}

function toggle(canvas: CanvasApi, id: string, key: SettingKey, label: string, x: number, y: number, w: number): void {
  const checked = b(key);
  addControl({ id, kind: "toggle", key, x, y, width: w, height: 30 });
  canvas.fillRect(x, y, w, 30, hover === id ? colors.hover : colors.bg);
  canvas.strokeRect(x, y + 6, 18, 18, checked ? colors.accent : colors.line, 1);
  if (checked) {
    canvas.fillRect(x + 4, y + 10, 10, 10, colors.accent);
  }
  drawText(canvas, label, x + 28, y + 6, w - 28);
}

function radio(canvas: CanvasApi, id: string, key: SettingKey, value: number, label: string, x: number, y: number, w: number): void {
  const checked = n(key) === value;
  addControl({ id, kind: "radio", key, value, x, y, width: w, height: 28 });
  canvas.fillRect(x, y, w, 28, hover === id ? colors.hover : colors.bg);
  canvas.strokeRect(x, y + 6, 16, 16, checked ? colors.accent : colors.line, 1);
  if (checked) {
    canvas.fillRect(x + 5, y + 11, 6, 6, colors.accent);
  }
  drawText(canvas, label, x + 28, y + 5, w - 28);
}

function slider(canvas: CanvasApi, id: string, key: SettingKey, label: string, min: number, max: number, x: number, y: number, w: number): void {
  const value = n(key);
  const clamped = Math.max(min, Math.min(max, value));
  const pct = (clamped - min) / (max - min);
  const trackX = x + 170;
  const trackW = Math.max(80, w - 245);
  addControl({ id, kind: "slider", key, min, max, x, y, width: w, height: 34 });
  drawText(canvas, label, x, y + 7, 158);
  canvas.fillRect(trackX, y + 15, trackW, 4, colors.line);
  canvas.fillRect(trackX, y + 15, trackW * pct, 4, colors.accent);
  canvas.fillRect(trackX + trackW * pct - 5, y + 9, 10, 16, colors.accent);
  drawText(canvas, `${clamped}%`, x + w - 56, y + 7, 54, colors.muted);
}

function setSliderFromPointer(control: Control, x: number): void {
  if (!control.key || control.min === undefined || control.max === undefined) {
    return;
  }
  const trackX = control.x + 170;
  const trackW = Math.max(80, control.width - 245);
  const pct = Math.max(0, Math.min(1, (x - trackX) / trackW));
  const value = Math.round(control.min + pct * (control.max - control.min));
  settings.set(control.key, value);
}

function activate(control: Control): void {
  if (control.kind === "button") {
    if (control.id === "save") settings.save();
    if (control.id === "cancel") host.close();
    if (control.id === "reset") settings.resetKeyBindings();
  } else if (control.kind === "toggle" && control.key) {
    settings.set(control.key, !b(control.key));
  } else if (control.kind === "radio" && control.key && typeof control.value === "number") {
    settings.set(control.key, control.value);
  }
}

globalThis.imgviewerSettingsUi = {
  render(canvas, env) {
    controls = [];
    canvas.clear(colors.bg);
    const width = Math.max(360, env.width);
    const left = 24;
    const right = width - 24;
    const contentWidth = right - left;
    let y = 22 - scrollY;

    canvas.fillText("Settings", left, y, contentWidth, 30, colors.text);
    y += 48;

    y = section(canvas, "Language", left, y, contentWidth);
    radio(canvas, "lang-en", "language", 0, "English", left, y, contentWidth / 2 - 8);
    radio(canvas, "lang-zh", "language", 1, "Simplified Chinese", left + contentWidth / 2, y, contentWidth / 2);
    y += 42;

    y = section(canvas, "Window", left, y, contentWidth);
    toggle(canvas, "remember", "rememberWindowSize", "Remember window size", left, y, contentWidth);
    y += 34;
    toggle(canvas, "borderless", "borderlessWindow", "Borderless window", left, y, contentWidth);
    y += 40;
    slider(canvas, "opacity", "windowOpacityPercent", "Opacity", 10, 100, left, y, contentWidth);
    y += 40;
    slider(canvas, "toolbar", "toolbarScalePercent", "Toolbar size", 80, 160, left, y, contentWidth);
    y += 48;

    y = section(canvas, "Image Rendering", left, y, contentWidth);
    radio(canvas, "fit", "initialImageViewMode", 0, "Fit window", left, y, contentWidth / 2 - 8);
    radio(canvas, "actual", "initialImageViewMode", 1, "Actual size", left + contentWidth / 2, y, contentWidth / 2);
    y += 36;
    toggle(canvas, "pixel", "pixelatedSampling", "Pixelated sampling", left, y, contentWidth);
    y += 34;
    toggle(canvas, "checker", "checkerboardBackground", "Checkerboard background", left, y, contentWidth);
    y += 44;

    y = section(canvas, "Navigation", left, y, contentWidth);
    toggle(canvas, "edge", "edgeClickNavigation", "Edge click navigation", left, y, contentWidth);
    y += 38;
    slider(canvas, "edge-zone", "edgeClickNavigationZonePercent", "Edge click zone", 5, 40, left, y, contentWidth);
    y += 50;

    y = section(canvas, "Action Shortcuts", left, y, contentWidth);
    addControl({ id: "filter", kind: "filter", x: left, y, width: contentWidth, height: 32 });
    canvas.fillRect(left, y, contentWidth, 32, hover === "filter" ? colors.hover : colors.panel);
    canvas.strokeRect(left, y, contentWidth, 32, colors.line, 1);
    drawText(canvas, filter === "" ? "Type to filter actions..." : filter, left + 10, y + 8, contentWidth - 20, filter === "" ? colors.muted : colors.text);
    y += 42;
    const rows = settings.actionRows(filter);
    for (const row of rows.slice(0, 18)) {
      canvas.fillRect(left, y, contentWidth, 24, colors.panel);
      drawText(canvas, row.name, left + 6, y + 4, Math.min(300, contentWidth * 0.42));
      drawText(canvas, row.shortcut, left + Math.min(320, contentWidth * 0.45), y + 4, contentWidth * 0.55 - 10, colors.muted);
      y += 25;
    }
    if (rows.length === 0) {
      drawText(canvas, "No matches", left + 6, y + 4, contentWidth - 12, colors.muted);
      y += 25;
    }

    maxScrollY = Math.max(0, y + 64 - env.height);
    scrollY = Math.max(0, Math.min(maxScrollY, scrollY));
    const footerY = env.height - 52;
    canvas.fillRect(0, footerY - 12, env.width, 64, "#F8F8FAFC");
    canvas.strokeLine(0, footerY - 12, env.width, footerY - 12, colors.line, 1);
    button(canvas, "reset", "Reset shortcuts", left, footerY, 132, false);
    button(canvas, "save", "Save", right - 160, footerY, 72, true);
    button(canvas, "cancel", "Cancel", right - 80, footerY, 80, false);
  },

  pointer(event) {
    if (event.type === "wheel") {
      scrollY = Math.max(0, Math.min(maxScrollY, scrollY - event.wheelDelta / 2));
      return { handled: true, invalidate: true };
    }
    const target = hitTest(event.x, event.y);
    if (event.type === "move") {
      const previous = hover;
      hover = target?.id ?? "";
      if (draggingSlider && active) {
        const control = controls.find(item => item.id === active);
        if (control) setSliderFromPointer(control, event.x);
        return { handled: true, capture: true, invalidate: true };
      }
      return { handled: hover !== "", invalidate: previous !== hover };
    }
    if (event.type === "down" && event.button === "left") {
      active = target?.id ?? "";
      if (target?.kind === "slider") {
        draggingSlider = target.id;
        setSliderFromPointer(target, event.x);
      }
      return { handled: active !== "", capture: active !== "", invalidate: active !== "" };
    }
    if (event.type === "up" && event.button === "left") {
      const wasActive = active;
      if (target && wasActive === target.id && !draggingSlider) {
        activate(target);
      }
      active = "";
      draggingSlider = "";
      return { handled: wasActive !== "", capture: false, invalidate: wasActive !== "" };
    }
    if (event.type === "leave") {
      hover = "";
      active = "";
      draggingSlider = "";
      return { handled: true, capture: false, invalidate: true };
    }
    return { handled: false };
  },

  key(event) {
    if (event.type !== "down") return { handled: false };
    if (event.ctrl && event.virtualKey === 0x53) {
      settings.save();
      return { handled: true };
    }
    if (event.ctrl && event.virtualKey === 0x52) {
      host.reload();
      return { handled: true };
    }
    if (event.virtualKey === 0x08 && filter.length > 0) {
      filter = filter.slice(0, -1);
      return { handled: true, invalidate: true };
    }
    return { handled: false };
  },

  text(event) {
    if (event.text >= " " && event.text !== "\u007f") {
      filter += event.text;
      return { handled: true, invalidate: true };
    }
    return { handled: false };
  },
};
