/// <reference types="../types/imgviewer" />

import { clamp, contains, drawButton, drawText, type Rect } from "./common";

type Hit = Rect & { id: string; action?: string; actionArg?: number; enabled?: boolean; kind?: string };

const colors = {
  title: "#EFFFFFFF",
  panel: "#F4FFFFFF",
  panelSoft: "#EAF7F9FC",
  stroke: "#FFD8DEE8",
  text: "#FF172033",
  muted: "#FF697386",
  accent: "#FF2D6CDF",
  hover: "#FFEAF1FF",
  active: "#FFD9E8FF",
  disabled: "#FF9AA4B2",
  danger: "#FFB42318",
};

const mainActions = [
  "previousImage",
  "nextImage",
  "captureRegion",
  "zoomIn",
  "zoomOut",
  "fitWindow",
  "actualSize",
  "rotateClockwise",
  "flipHorizontal",
  "flipVertical",
  "resetView",
  "toggleColorPicker",
];

const editActions: Array<[string, string]> = [
  ["select", "editSelect"],
  ["pixelSelect", "editPixelSelect"],
  ["pen", "editPen"],
  ["shape", "editShape"],
  ["text", "editText"],
  ["crop", "editCrop"],
];

const penWidths = [2, 4, 8, 12];
const palette = [
  { label: "Red", color: "#FFFF0000" },
  { label: "Yellow", color: "#FFFFFF00" },
  { label: "Green", color: "#FF00FF00" },
  { label: "Cyan", color: "#FF00FFFF" },
  { label: "Blue", color: "#FF1E90FF" },
  { label: "Magenta", color: "#FFFF00FF" },
  { label: "White", color: "#FFFFFFFF" },
  { label: "Black", color: "#FF000000" },
];
const textSizes = [12, 16, 20, 28, 36];
const textBackgrounds = [
  { label: "None", color: "#00FFFF00", hasBackground: false },
  { label: "Yellow", color: "#D1FFFF00", hasBackground: true },
  { label: "White", color: "#D1FFFFFF", hasBackground: true },
  { label: "Black", color: "#D1000000", hasBackground: true },
  { label: "Red", color: "#D1FF0000", hasBackground: true },
  { label: "Blue", color: "#D11E90FF", hasBackground: true },
];
const shapes = ["rectangle", "ellipse", "line", "arrow"];

let lastState: MainOverlayState | undefined;
let lastEnv: RenderEnvironment | undefined;
let hits: Hit[] = [];
let hoverId = "";
let pressedId = "";
let toolbar = { x: 0, y: 0, ready: false, dragging: false, dx: 0, dy: 0 };
let infoScroll = 0;

function actionLabel(state: MainOverlayState, action: string): string {
  return state.actionLabels[action] || action;
}

function enabled(state: MainOverlayState, action: string): boolean {
  return state.actionEnabled[action] !== false;
}

function addHit(hit: Hit): void {
  hits.push(hit);
}

function hitAt(x: number, y: number): Hit | undefined {
  for (let i = hits.length - 1; i >= 0; --i) {
    if (contains(hits[i], x, y)) {
      return hits[i];
    }
  }
  return undefined;
}

function button(canvas: CanvasApi, hit: Hit, label: string, active = false): void {
  const disabled = hit.enabled === false;
  drawButton(
    canvas,
    hit,
    label,
    { hover: hoverId === hit.id && !disabled, active: active || pressedId === hit.id },
    {
      fill: disabled ? "#FFE9EDF3" : colors.panel,
      hoverFill: colors.hover,
      activeFill: colors.active,
      stroke: active ? colors.accent : colors.stroke,
      hoverStroke: colors.accent,
      text: disabled ? colors.disabled : colors.text,
    },
  );
}

function packFloat(value: number): number {
  return Math.floor(value * 100 + 0.5);
}

function packColor(color: string): number {
  const hex = color.startsWith("#") ? color.slice(1) : color;
  const a = hex.length === 8 ? Number.parseInt(hex.slice(0, 2), 16) : 255;
  const r = Number.parseInt(hex.slice(hex.length === 8 ? 2 : 0, hex.length === 8 ? 4 : 2), 16);
  const g = Number.parseInt(hex.slice(hex.length === 8 ? 4 : 2, hex.length === 8 ? 6 : 4), 16);
  const b = Number.parseInt(hex.slice(hex.length === 8 ? 6 : 4, hex.length === 8 ? 8 : 6), 16);
  return ((r << 24) | (g << 16) | (b << 8) | a) | 0;
}

function shapeArg(shape: string): number {
  return { rectangle: 0, ellipse: 1, line: 2, arrow: 3 }[shape] ?? 0;
}

function backgroundArg(color: string, hasBackground: boolean): number {
  return hasBackground ? ((packColor(color) << 8) | 1) | 0 : 0;
}

function renderTitlebar(canvas: CanvasApi, env: RenderEnvironment, state: MainOverlayState): void {
  canvas.fillRect(0, 0, env.width, 42, colors.title);
  canvas.strokeLine(0, 41.5, env.width, 41.5, colors.stroke, 1);

  const menu: Hit = { id: "menu", x: 8, y: 7, width: 34, height: 28, action: "openMenu", enabled: true };
  addHit(menu);
  button(canvas, menu, "Menu", false);

  drawText(canvas, state.title || "ImgViewer", 54, 11, Math.max(60, env.width - 220), colors.text, 22);
  if (state.topMost) {
    drawText(canvas, "Top", env.width - 144, 11, 36, colors.accent, 22);
  }

  const min: Hit = { id: "minimize", x: env.width - 108, y: 7, width: 30, height: 28, action: "minimize", enabled: enabled(state, "minimize") };
  const max: Hit = { id: "toggleMaximize", x: env.width - 74, y: 7, width: 30, height: 28, action: "toggleMaximize", enabled: enabled(state, "toggleMaximize") };
  const close: Hit = { id: "close", x: env.width - 40, y: 7, width: 30, height: 28, action: "close", enabled: enabled(state, "close") };
  addHit(min);
  addHit(max);
  addHit(close);
  button(canvas, min, "Min");
  button(canvas, max, state.maximized ? "Rest" : "Max");
  button(canvas, close, "X");
}

function toolbarSize(state: MainOverlayState): { button: number; gap: number; pad: number; handle: number; width: number; height: number } {
  const scale = clamp(state.toolbarScalePercent || 125, 75, 200) / 125;
  const buttonSize = 34 * scale;
  const gap = 5 * scale;
  const pad = 7 * scale;
  const handle = 15 * scale;
  return {
    button: buttonSize,
    gap,
    pad,
    handle,
    width: pad * 2 + handle + mainActions.length * buttonSize + (mainActions.length - 1) * gap,
    height: pad * 2 + buttonSize,
  };
}

function renderToolbar(canvas: CanvasApi, env: RenderEnvironment, state: MainOverlayState): Rect {
  const size = toolbarSize(state);
  if (!toolbar.ready) {
    toolbar.x = Math.max(0, (env.width - size.width) / 2);
    toolbar.y = Math.max(48, env.height - size.height - 18);
    toolbar.ready = true;
  }
  toolbar.x = clamp(toolbar.x, 0, Math.max(0, env.width - size.width));
  toolbar.y = clamp(toolbar.y, 48, Math.max(48, env.height - size.height));

  const rect = { x: toolbar.x, y: toolbar.y, width: size.width, height: size.height };
  canvas.fillRect(rect.x, rect.y, rect.width, rect.height, "#EFFFFFFF");
  canvas.strokeRect(rect.x, rect.y, rect.width, rect.height, colors.stroke, 1);

  const drag: Hit = { id: "toolbarDrag", x: rect.x, y: rect.y, width: size.pad + size.handle, height: rect.height, kind: "drag", enabled: true };
  addHit(drag);
  canvas.strokeLine(drag.x + 8, drag.y + 11, drag.x + 8, drag.y + drag.height - 11, colors.muted, 1);
  canvas.strokeLine(drag.x + 12, drag.y + 11, drag.x + 12, drag.y + drag.height - 11, colors.muted, 1);

  let x = rect.x + size.pad + size.handle;
  for (const action of mainActions) {
    const hit: Hit = { id: `main:${action}`, x, y: rect.y + size.pad, width: size.button, height: size.button, action, enabled: enabled(state, action) };
    addHit(hit);
    button(canvas, hit, shortLabel(actionLabel(state, action)), action === "toggleColorPicker" && state.colorPickerActive);
    x += size.button + size.gap;
  }
  return rect;
}

function shortLabel(label: string): string {
  const compact: Record<string, string> = {
    "Previous Image": "Prev",
    "Next Image": "Next",
    "Capture Region": "Cap",
    "Zoom In": "+",
    "Zoom Out": "-",
    "Fit Window": "Fit",
    "Actual Size": "1:1",
    "Rotate Clockwise": "Rot",
    "Flip Horizontal": "Flip H",
    "Flip Vertical": "Flip V",
    "Reset View": "Reset",
    "Color Picker": "Color",
  };
  return compact[label] || label;
}

function renderEditToolbar(canvas: CanvasApi, state: MainOverlayState, anchor: Rect): Rect {
  if (!state.editToolbar.visible) {
    return anchor;
  }
  const buttonSize = 32;
  const gap = 5;
  const width = editActions.length * buttonSize + 2 * buttonSize + gap * (editActions.length + 1) + 16;
  const rect = { x: anchor.x + (anchor.width - width) / 2, y: anchor.y - 48, width, height: 40 };
  canvas.fillRect(rect.x, rect.y, rect.width, rect.height, "#EFFFFFFF");
  canvas.strokeRect(rect.x, rect.y, rect.width, rect.height, colors.stroke, 1);
  let x = rect.x + 8;
  for (const [tool, action] of editActions) {
    const hit: Hit = { id: `edit:${action}`, x, y: rect.y + 4, width: buttonSize, height: buttonSize, action, enabled: enabled(state, action) };
    addHit(hit);
    button(canvas, hit, toolLabel(tool), state.editToolbar.tool === tool);
    x += buttonSize + gap;
  }
  for (const action of ["editUndo", "editRedo"]) {
    const hit: Hit = { id: `edit:${action}`, x, y: rect.y + 4, width: buttonSize, height: buttonSize, action, enabled: enabled(state, action) };
    addHit(hit);
    button(canvas, hit, action === "editUndo" ? "Undo" : "Redo");
    x += buttonSize + gap;
  }
  return rect;
}

function toolLabel(tool: string): string {
  return { select: "Sel", pixelSelect: "Pix", pen: "Pen", shape: "Shape", text: "Text", crop: "Crop" }[tool] || tool;
}

function renderToolstrip(canvas: CanvasApi, state: MainOverlayState, anchor: Rect): void {
  const items: Array<{ id: string; label: string; action?: string; actionArg?: number; active?: boolean; enabled?: boolean }> = [];
  if (state.colorPickerToolstrip.visible) {
    items.push({ id: "copyColorPickerValue", label: state.colorPickerToolstrip.hasSample ? state.colorPickerToolstrip.hexText : "No sample", action: "copyColorPickerValue", enabled: state.colorPickerToolstrip.hasSample });
  } else if (state.penToolstrip.visible) {
    for (const width of penWidths) items.push({ id: `penWidth:${width}`, label: `${width}px`, action: "editSetPenWidth", actionArg: packFloat(width), active: Math.abs(state.penToolstrip.width - width) < 0.1 });
    for (const item of palette) items.push({ id: `penColor:${item.color}`, label: item.label, action: "editSetPenColor", actionArg: packColor(item.color), active: state.penToolstrip.color.toUpperCase() === item.color.toUpperCase() });
  } else if (state.shapeToolstrip.visible) {
    for (const shape of shapes) items.push({ id: `shape:${shape}`, label: shape, action: "editSetShapeKind", actionArg: shapeArg(shape), active: state.shapeToolstrip.kind === shape });
    for (const item of palette) items.push({ id: `shapeColor:${item.color}`, label: item.label, action: "editSetPenColor", actionArg: packColor(item.color), active: state.shapeToolstrip.color.toUpperCase() === item.color.toUpperCase() });
  } else if (state.textToolstrip.visible) {
    items.push({ id: "font", label: state.textToolstrip.fontFamily });
    for (const size of textSizes) items.push({ id: `fontSize:${size}`, label: `${size}`, action: "editSetTextFontSize", actionArg: packFloat(size), active: Math.abs(state.textToolstrip.fontSize - size) < 0.1 });
    for (const item of palette) items.push({ id: `textColor:${item.color}`, label: item.label, action: "editSetTextColor", actionArg: packColor(item.color), active: state.textToolstrip.textColor.toUpperCase() === item.color.toUpperCase() });
    for (const item of textBackgrounds) {
      items.push({
        id: `textBg:${item.label}`,
        label: `Bg ${item.label}`,
        action: "editSetTextBackground",
        actionArg: backgroundArg(item.color, item.hasBackground),
        active: state.textToolstrip.hasBackground === item.hasBackground && (!item.hasBackground || state.textToolstrip.backgroundColor.toUpperCase() === item.color.toUpperCase()),
      });
    }
  } else if (state.selectionToolstrip.visible) {
    items.push({ id: "copy", label: "Copy", action: "editCopySelection" });
    items.push({ id: "mosaic", label: "Mosaic", action: "editMosaicSelection" });
    items.push({ id: "delete", label: "Delete", action: "editDeleteSelection" });
  }
  if (items.length === 0) return;

  const width = Math.min(520, items.length * 62 + 16);
  const rect = { x: anchor.x + (anchor.width - width) / 2, y: anchor.y - 46, width, height: 38 };
  canvas.fillRect(rect.x, rect.y, rect.width, rect.height, "#F4FFFFFF");
  canvas.strokeRect(rect.x, rect.y, rect.width, rect.height, colors.stroke, 1);
  let x = rect.x + 8;
  for (const item of items) {
    const itemWidth = Math.max(48, Math.min(112, item.label.length * 7 + 22));
    const hit: Hit = { id: `tool:${item.id}`, x, y: rect.y + 4, width: itemWidth, height: 30, action: item.action, actionArg: item.actionArg, enabled: item.enabled !== false };
    addHit(hit);
    button(canvas, hit, item.label, item.active);
    x += itemWidth + 5;
  }
}

function renderAnimation(canvas: CanvasApi, env: RenderEnvironment, state: MainOverlayState, anchor: Rect): void {
  if (!state.animation.available) return;
  const rect = { x: anchor.x + (anchor.width - 280) / 2, y: anchor.y - 92, width: 280, height: 38 };
  canvas.fillRect(rect.x, rect.y, rect.width, rect.height, "#F4FFFFFF");
  canvas.strokeRect(rect.x, rect.y, rect.width, rect.height, colors.stroke, 1);
  const actions = ["toggleAnimationLoop", "previousAnimationFrame", "toggleAnimationPlayback", "nextAnimationFrame"];
  let x = rect.x + 8;
  for (const action of actions) {
    const hit: Hit = { id: `anim:${action}`, x, y: rect.y + 4, width: 44, height: 30, action, enabled: enabled(state, action) };
    addHit(hit);
    const label = action === "toggleAnimationLoop" ? (state.animation.loop ? "Loop" : "Once") : action === "toggleAnimationPlayback" ? (state.animation.playing ? "Pause" : "Play") : action === "previousAnimationFrame" ? "Prev" : "Next";
    button(canvas, hit, label, action === "toggleAnimationLoop" && state.animation.loop);
    x += 48;
  }
  drawText(canvas, `${state.animation.currentFrame + 1} / ${state.animation.totalFrames}`, x + 6, rect.y + 10, env.width - x - 10, colors.muted, 18);
}

function renderInfoPanel(canvas: CanvasApi, env: RenderEnvironment, state: MainOverlayState): void {
  if (!state.infoPanel.visible) return;
  const width = Math.min(320, Math.max(120, env.width - 24));
  const rect = { x: env.width - width - 12, y: 54, width, height: Math.max(180, env.height - 66) };
  addHit({ id: "infoPanel", ...rect, kind: "panel", enabled: true });
  canvas.fillRect(rect.x, rect.y, rect.width, rect.height, colors.panel);
  canvas.strokeRect(rect.x, rect.y, rect.width, rect.height, colors.stroke, 1);
  drawText(canvas, "Info", rect.x + 12, rect.y + 10, rect.width - 54, colors.text, 20);
  const close: Hit = { id: "infoClose", x: rect.x + rect.width - 38, y: rect.y + 8, width: 26, height: 24, action: "toggleInfoPanel", enabled: enabled(state, "toggleInfoPanel") };
  addHit(close);
  button(canvas, close, "X");

  let y = rect.y + 42 - infoScroll;
  const rows: MainOverlayRows[] = [
    { label: "Name", value: state.infoPanel.name },
    { label: "Path", value: state.infoPanel.path },
    { label: "Dimensions", value: state.infoPanel.dimensions },
    { label: "Type", value: state.infoPanel.type },
    { label: "File Size", value: state.infoPanel.fileSize },
    { label: "Modified", value: state.infoPanel.modifiedTime },
  ];
  y = renderRows(canvas, rows, rect, y);
  y = renderSection(canvas, "Color / HDR", state.infoPanel.colorRows, rect, y);
  y = renderSection(canvas, "Exif", state.infoPanel.exifRows, rect, y);
  y += 8;
  drawText(canvas, "Histogram summary", rect.x + 12, y, rect.width - 24, colors.muted, 18);
  y += 22;
  canvas.fillRect(rect.x + 12, y, rect.width - 24, 42, colors.panelSoft);
  canvas.strokeRect(rect.x + 12, y, rect.width - 24, 42, colors.stroke, 1);
  drawText(canvas, state.infoPanel.hasAnalysis ? `${state.infoPanel.analysis.sampledPixels} sampled pixels` : "Unavailable", rect.x + 20, y + 12, rect.width - 40, colors.text, 18);
  y += 56;
  if (state.infoPanel.hasAnalysis) {
    renderChip(canvas, rect.x + 12, y, "Avg", state.infoPanel.analysis.average);
    renderChip(canvas, rect.x + 108, y, "Dark", state.infoPanel.analysis.darkest);
    renderChip(canvas, rect.x + 204, y, "Bright", state.infoPanel.analysis.brightest);
  }
  const maxScroll = Math.max(0, y + 70 - (rect.y + rect.height));
  infoScroll = clamp(infoScroll, 0, maxScroll);
}

function renderRows(canvas: CanvasApi, rows: MainOverlayRows[], rect: Rect, y: number): number {
  for (const row of rows) {
    drawText(canvas, row.label, rect.x + 12, y, 94, colors.muted, 18);
    drawText(canvas, row.value || "-", rect.x + 108, y, rect.width - 120, colors.text, 18);
    y += 22;
  }
  return y + 8;
}

function renderSection(canvas: CanvasApi, title: string, rows: MainOverlayRows[], rect: Rect, y: number): number {
  if (rows.length === 0) return y;
  drawText(canvas, title, rect.x + 12, y, rect.width - 24, colors.muted, 18);
  return renderRows(canvas, rows, rect, y + 22);
}

function renderChip(canvas: CanvasApi, x: number, y: number, label: string, color: MainOverlayColorSample): void {
  const hex = `#FF${color.red.toString(16).padStart(2, "0")}${color.green.toString(16).padStart(2, "0")}${color.blue.toString(16).padStart(2, "0")}`.toUpperCase();
  canvas.fillRect(x, y, 84, 18, hex);
  canvas.strokeRect(x, y, 84, 18, colors.stroke, 1);
  drawText(canvas, label, x, y + 22, 84, colors.muted, 16);
  drawText(canvas, hex.slice(3), x, y + 40, 84, colors.text, 16);
}

function renderToast(canvas: CanvasApi, env: RenderEnvironment, state: MainOverlayState): void {
  if (!state.toast.visible || !state.toast.text) return;
  const width = Math.min(env.width - 40, Math.max(220, state.toast.text.length * 8 + 36));
  const rect = { x: (env.width - width) / 2, y: 58, width, height: 38 };
  canvas.fillRect(rect.x, rect.y, rect.width, rect.height, "#F6FFFFFF");
  canvas.strokeRect(rect.x, rect.y, rect.width, rect.height, colors.accent, 1);
  drawText(canvas, state.toast.text, rect.x + 16, rect.y + 10, rect.width - 32, colors.text, 20);
}

function render(canvas: CanvasApi, env: RenderEnvironment, state: MainOverlayState): void {
  lastState = state;
  lastEnv = env;
  hits = [];
  renderTitlebar(canvas, env, state);
  const mainToolbar = renderToolbar(canvas, env, state);
  const editAnchor = renderEditToolbar(canvas, state, mainToolbar);
  renderToolstrip(canvas, state, editAnchor);
  renderAnimation(canvas, env, state, mainToolbar);
  renderInfoPanel(canvas, env, state);
  renderToast(canvas, env, state);
}

function pointer(event: MainPointerEvent): MainEventResult | void {
  if (!lastState || !lastEnv) return;
  const hit = hitAt(event.x, event.y);
  if (event.type === "move") {
    if (toolbar.dragging) {
      toolbar.x = event.x - toolbar.dx;
      toolbar.y = event.y - toolbar.dy;
      return { handled: true, invalidate: true };
    }
    const nextHover = hit?.id || "";
    if (nextHover !== hoverId) {
      hoverId = nextHover;
      return { handled: false, invalidate: true };
    }
    return hit ? { handled: hit.kind !== "panel" } : undefined;
  }
  if (event.type === "leave") {
    hoverId = "";
    pressedId = "";
    toolbar.dragging = false;
    return { handled: false, capture: false, invalidate: true };
  }
  if (event.type === "wheel" && hit?.id === "infoPanel") {
    infoScroll = Math.max(0, infoScroll - event.wheelDelta / 4);
    return { handled: true, invalidate: true };
  }
  if (event.type === "down" && event.button === "left" && hit) {
    pressedId = hit.id;
    if (hit.kind === "drag") {
      toolbar.dragging = true;
      toolbar.dx = event.x - toolbar.x;
      toolbar.dy = event.y - toolbar.y;
      return { handled: true, capture: true, invalidate: true };
    }
    return { handled: true, invalidate: true };
  }
  if (event.type === "up" && event.button === "left") {
    const pressed = pressedId;
    pressedId = "";
    if (toolbar.dragging) {
      toolbar.dragging = false;
      return { handled: true, capture: false, invalidate: true };
    }
    if (hit && pressed === hit.id && hit.enabled !== false) {
      if (hit.action === "openMenu") return overlay.openMenu();
      if (hit.action) return overlay.action(hit.action, hit.actionArg) || { handled: true };
    }
    return pressed ? { handled: true, invalidate: true } : undefined;
  }
}

function key(event: MainKeyEvent): MainEventResult | void {
  if (event.type === "down" && event.virtualKey === 116) {
    host.reload();
    return { handled: true, invalidate: true };
  }
}

globalThis.imgviewerMainUi = { render, pointer, key };
