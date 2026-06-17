/// <reference types="../types/imgviewer" />

import { clamp, contains, drawButton, drawText, type Rect } from "./common";
import { svgIcon } from "./vector_icon";

type Hit = Rect & { id: string; action?: string; actionArg?: number; enabled?: boolean; kind?: string };

const edgeClickDragCancelDistance = 6;

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

const actionIcons: Record<string, VectorIcon> = {
  previousImage: svgIcon({ id: "previousImage", viewBox: [0, 0, 24, 24], paths: ["M14 6 L8 12 L14 18", "M18 6 L12 12 L18 18"] }),
  nextImage: svgIcon({ id: "nextImage", viewBox: [0, 0, 24, 24], paths: ["M10 6 L16 12 L10 18", "M6 6 L12 12 L6 18"] }),
  captureRegion: svgIcon({
    id: "captureRegion",
    viewBox: [0, 0, 24, 24],
    paths: ["M6 6 L18 6 L18 18 L6 18 Z", "M3 9 L3 5 M3 5 L7 5 M21 9 L21 5 M21 5 L17 5 M3 15 L3 19 M3 19 L7 19 M21 15 L21 19 M21 19 L17 19"],
  }),
  zoomIn: svgIcon({ id: "zoomIn", viewBox: [0, 0, 24, 24], circles: [[10, 10, 5]], paths: ["M10 7 L10 13 M7 10 L13 10 M14 14 L20 20"] }),
  zoomOut: svgIcon({ id: "zoomOut", viewBox: [0, 0, 24, 24], circles: [[10, 10, 5]], paths: ["M7 10 L13 10 M14 14 L20 20"] }),
  fitWindow: svgIcon({
    id: "fitWindow",
    viewBox: [0, 0, 24, 24],
    paths: ["M4 4 L20 4 L20 20 L4 20 Z", "M8 6 L6 6 L6 8 M16 6 L18 6 L18 8 M8 18 L6 18 L6 16 M16 18 L18 18 L18 16 M6 6 L10 10 M18 6 L14 10 M6 18 L10 14 M18 18 L14 14"],
  }),
  actualSize: svgIcon({
    id: "actualSize",
    viewBox: [0, 0, 24, 24],
    paths: ["M5 5 L19 5 L19 19 L5 19 Z", "M9.6667 5 L9.6667 19 M14.3333 5 L14.3333 19 M5 9.6667 L19 9.6667 M5 14.3333 L19 14.3333 M9.6667 9.6667 L14.3333 9.6667 L14.3333 14.3333 L9.6667 14.3333 Z"],
  }),
  rotateClockwise: svgIcon({ id: "rotateClockwise", viewBox: [0, 0, 24, 24], paths: ["M17 8 L20 8 L20 5", "M19 8 C17.8 5.6 15.2 4 12.2 4 C8.2 4 5 7.2 5 11.2 C5 15.2 8.2 18.4 12.2 18.4 C15.1 18.4 17.6 16.7 18.8 14.2"] }),
  flipHorizontal: svgIcon({ id: "flipHorizontal", viewBox: [0, 0, 24, 24], paths: ["M4 5 L10 12 L4 19 L4 5", "M20 5 L14 12 L20 19 L20 5", "M12 3.5 L12 20.5"] }),
  flipVertical: svgIcon({ id: "flipVertical", viewBox: [0, 0, 24, 24], paths: ["M5 4 L12 10 L19 4 L5 4", "M5 20 L12 14 L19 20 L5 20", "M3.5 12 L20.5 12"] }),
  resetView: svgIcon({ id: "resetView", viewBox: [0, 0, 24, 24], paths: ["M7 7 C8.4 5.7 10.1 5 12 5 C15.9 5 19 8.1 19 12 C19 15.9 15.9 19 12 19 C8.8 19 6.1 16.9 5.3 14", "M7 7 L7 3 M7 7 L3 7"] }),
  toggleColorPicker: svgIcon({ id: "toggleColorPicker", viewBox: [0, 0, 24, 24], paths: ["M15.2 3.8 C16.4 2.6 18.3 2.6 19.5 3.8 C20.7 5 20.7 6.9 19.5 8.1 L18 9.6 L13.7 5.3 L15.2 3.8 Z", "M13 6 L17.3 10.3 L8 19.6 L4.7 20.7 L5.8 17.4 L15.1 8.1", "M11.4 9.8 L13.5 11.9"] }),
};

const editActions: Array<[string, string]> = [
  ["select", "editSelect"],
  ["pixelSelect", "editPixelSelect"],
  ["pen", "editPen"],
  ["shape", "editShape"],
  ["text", "editText"],
  ["crop", "editCrop"],
];
const editCommandActions: Array<[string, string]> = [
  ["Save", "saveImageAs"],
  ["Close", "toggleEditMode"],
];

const editIcons: Record<string, VectorIcon> = {
  editSelect: svgIcon({ id: "editSelect", viewBox: [0, 0, 24, 24], paths: ["M6 4 L17 15 L12 16 L10 21 L6 4 Z"] }),
  editPixelSelect: actionIcons.captureRegion,
  editPen: svgIcon({ id: "editPen", viewBox: [0, 0, 24, 24], paths: ["M15.5 4.5 L19.5 8.5 L8 20 L4 20 L4 16 L15.5 4.5 Z", "M13.5 6.5 L17.5 10.5"] }),
  editShape: svgIcon({ id: "editShape", viewBox: [0, 0, 24, 24], rects: [[4, 6, 8, 8]], circles: [[16.5, 15.5, 3.5]] }),
  editText: svgIcon({ id: "editText", viewBox: [0, 0, 24, 24], paths: ["M5 6 L19 6 M12 6 L12 19 M9 19 L15 19"] }),
  editCrop: svgIcon({ id: "editCrop", viewBox: [0, 0, 24, 24], paths: ["M7 3 L7 17 L21 17", "M3 7 L17 7 L17 21"] }),
  editUndo: svgIcon({ id: "editUndo", viewBox: [0, 0, 24, 24], paths: ["M8 8 L4 12 L8 16", "M5 12 L15 12 C18 12 20 14 20 17"] }),
  editRedo: svgIcon({ id: "editRedo", viewBox: [0, 0, 24, 24], paths: ["M16 8 L20 12 L16 16", "M19 12 L9 12 C6 12 4 14 4 17"] }),
  saveImageAs: svgIcon({ id: "saveImageAs", viewBox: [0, 0, 24, 24], paths: ["M5 4 L17 4 L20 7 L20 20 L5 20 Z", "M8 4 L8 10 L16 10 L16 4", "M8 20 L8 14 L17 14 L17 20"] }),
  toggleEditMode: svgIcon({ id: "toggleEditMode", viewBox: [0, 0, 24, 24], paths: ["M6 6 L18 18 M18 6 L6 18"] }),
};

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
let edgePress: { id: string; action: string; x: number; y: number } | undefined;
let toolbar = { x: 0, y: 0, ready: false, dragging: false, dx: 0, dy: 0 };
let infoScroll = 0;

function actionLabel(state: MainOverlayState, action: string): string {
  return state.actionLabels[action] || action;
}

function enabled(state: MainOverlayState, action: string): boolean {
  return state.actionEnabled[action] !== false;
}

function menuItem(state: MainOverlayState, action: string, checked = false, text?: string, forceEnabled?: boolean): PopupMenuItem {
  return {
    text: text || actionLabel(state, action),
    action,
    separator: false,
    checked,
    enabled: forceEnabled !== undefined ? forceEnabled : enabled(state, action),
    children: [],
  };
}

function menuSeparator(): PopupMenuItem {
  return { text: "", separator: true, checked: false, enabled: false, children: [] };
}

function menuParent(text: string, checked: boolean, enabled: boolean, children: PopupMenuItem[]): PopupMenuItem {
  return { text, separator: false, checked, enabled, children };
}

function mainMenuState(state: MainOverlayState): PopupState {
  const editEnabled = state.editToolbar.visible;
  const items: PopupMenuItem[] = [
    menuItem(state, "openImage"),
    menuItem(state, "captureRegion"),
    menuItem(state, "saveImageAs"),
    menuItem(state, "showInFileExplorer"),
    menuSeparator(),
    menuItem(state, "openSettings"),
  ];
  if (state.developerEnabled) {
    items.push(menuItem(state, "openDeveloper"));
  }
  items.push(
    menuItem(state, "openAbout"),
    menuSeparator(),
    menuItem(state, "toggleInfoPanel", state.infoPanel.visible),
    menuItem(state, "toggleAnimationLoop", state.animation.loop),
    menuItem(state, "toggleAnimationPlayback", false, state.animation.playing ? "Pause Animation" : "Play Animation"),
    menuItem(state, "previousAnimationFrame"),
    menuItem(state, "nextAnimationFrame"),
    menuSeparator(),
    menuItem(state, "zoomIn"),
    menuItem(state, "zoomOut"),
    menuItem(state, "fitWindow"),
    menuItem(state, "actualSize"),
    menuItem(state, "resetView"),
    menuItem(state, "toggleColorPicker", state.colorPickerActive),
    menuSeparator(),
    menuParent(actionLabel(state, "toggleEditMode"), state.editToolbar.visible, enabled(state, "toggleEditMode"), [
      menuItem(state, "toggleEditMode", state.editToolbar.visible),
      menuSeparator(),
      menuParent("Tool", false, editEnabled, [
        menuItem(state, "editSelect", state.editToolbar.tool === "select", undefined, editEnabled && enabled(state, "editSelect")),
        menuItem(state, "editPixelSelect", state.editToolbar.tool === "pixelSelect", undefined, editEnabled && enabled(state, "editPixelSelect")),
        menuItem(state, "editPen", state.editToolbar.tool === "pen", undefined, editEnabled && enabled(state, "editPen")),
        menuItem(state, "editShape", state.editToolbar.tool === "shape", undefined, editEnabled && enabled(state, "editShape")),
        menuItem(state, "editText", state.editToolbar.tool === "text", undefined, editEnabled && enabled(state, "editText")),
        menuItem(state, "editCrop", state.editToolbar.tool === "crop", undefined, editEnabled && enabled(state, "editCrop")),
      ]),
    ]),
    menuSeparator(),
    menuItem(state, "rotateClockwise"),
    menuItem(state, "flipHorizontal"),
    menuItem(state, "flipVertical"),
    menuSeparator(),
    menuItem(state, "close"),
  );
  return { kind: "menu", items };
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

function button(canvas: CanvasApi, hit: Hit, label: string, active = false, icon?: VectorIcon): void {
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
    icon,
  );
}

function renderEdgeClickHits(env: RenderEnvironment, state: MainOverlayState): void {
  if (!state.edgeClickNavigation) return;

  const zonePercent = clamp(state.edgeClickNavigationZonePercent || 10, 5, 40);
  const zoneWidth = Math.max(1, (env.width * zonePercent) / 100);
  if (enabled(state, "previousImage")) {
    addHit({ id: "edge:previousImage", x: 0, y: 0, width: zoneWidth, height: env.height, kind: "edge", action: "previousImage", enabled: true });
  }
  if (enabled(state, "nextImage")) {
    addHit({ id: "edge:nextImage", x: env.width - zoneWidth, y: 0, width: zoneWidth, height: env.height, kind: "edge", action: "nextImage", enabled: true });
  }
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

function renderTitlebar(canvas: CanvasApi, env: RenderEnvironment, state: MainOverlayState): MainRenderRect[] {
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
  return [{ x: 56, y: 0, width: Math.max(0, env.width - 206), height: 42 }];
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
    button(canvas, hit, shortLabel(actionLabel(state, action)), action === "toggleColorPicker" && state.colorPickerActive, actionIcons[action]);
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
  const width =
    editActions.length * buttonSize +
    2 * buttonSize +
    editCommandActions.length * 56 +
    gap * (editActions.length + editCommandActions.length + 1) +
    16;
  const rect = { x: anchor.x + (anchor.width - width) / 2, y: anchor.y - 48, width, height: 40 };
  canvas.fillRect(rect.x, rect.y, rect.width, rect.height, "#EFFFFFFF");
  canvas.strokeRect(rect.x, rect.y, rect.width, rect.height, colors.stroke, 1);
  let x = rect.x + 8;
  for (const [tool, action] of editActions) {
    const hit: Hit = { id: `edit:${action}`, x, y: rect.y + 4, width: buttonSize, height: buttonSize, action, enabled: enabled(state, action) };
    addHit(hit);
    button(canvas, hit, toolLabel(tool), state.editToolbar.tool === tool, editIcons[action]);
    x += buttonSize + gap;
  }
  for (const action of ["editUndo", "editRedo"]) {
    const hit: Hit = { id: `edit:${action}`, x, y: rect.y + 4, width: buttonSize, height: buttonSize, action, enabled: enabled(state, action) };
    addHit(hit);
    button(canvas, hit, action === "editUndo" ? "Undo" : "Redo", false, editIcons[action]);
    x += buttonSize + gap;
  }
  for (const [label, action] of editCommandActions) {
    const hit: Hit = { id: `edit:${action}`, x, y: rect.y + 4, width: 56, height: buttonSize, action, enabled: enabled(state, action) };
    addHit(hit);
    button(canvas, hit, label, false, editIcons[action]);
    x += 56 + gap;
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

function render(canvas: CanvasApi, env: RenderEnvironment, state: MainOverlayState): MainRenderResult {
  lastState = state;
  lastEnv = env;
  hits = [];
  renderEdgeClickHits(env, state);
  const captionDragRects = renderTitlebar(canvas, env, state);
  const mainToolbar = renderToolbar(canvas, env, state);
  const editAnchor = renderEditToolbar(canvas, state, mainToolbar);
  renderToolstrip(canvas, state, editAnchor);
  renderAnimation(canvas, env, state, mainToolbar);
  renderInfoPanel(canvas, env, state);
  renderToast(canvas, env, state);
  return { captionDragRects };
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
    if (edgePress) {
      const cancelDistance = edgeClickDragCancelDistance / Math.max(1, lastEnv.dpiScale || 1);
      const dx = event.x - edgePress.x;
      const dy = event.y - edgePress.y;
      if (dx * dx + dy * dy > cancelDistance * cancelDistance) {
        edgePress = undefined;
        return { handled: true, capture: false };
      }
      return { handled: true };
    }
    const hoverHit = hit?.kind === "edge" ? undefined : hit;
    const nextHover = hoverHit?.id || "";
    if (nextHover !== hoverId) {
      hoverId = nextHover;
      return { handled: false, invalidate: true };
    }
    return hoverHit ? { handled: hoverHit.kind !== "panel" } : undefined;
  }
  if (event.type === "leave") {
    hoverId = "";
    pressedId = "";
    edgePress = undefined;
    toolbar.dragging = false;
    return { handled: false, capture: false, invalidate: true };
  }
  if (event.type === "wheel" && hit?.id === "infoPanel") {
    infoScroll = Math.max(0, infoScroll - event.wheelDelta / 4);
    return { handled: true, invalidate: true };
  }
  if (event.type === "down" && event.button === "left" && hit) {
    if (hit.kind === "edge" && hit.action && hit.enabled !== false) {
      edgePress = { id: hit.id, action: hit.action, x: event.x, y: event.y };
      pressedId = "";
      return { handled: true, capture: true };
    }
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
    if (edgePress) {
      const pressed = edgePress;
      edgePress = undefined;
      if (hit && hit.id === pressed.id && hit.action === pressed.action && hit.enabled !== false) {
        const result = overlay.action(pressed.action);
        return result ? { ...result, handled: true, capture: false } : { handled: true, capture: false };
      }
      return { handled: true, capture: false };
    }
    const pressed = pressedId;
    pressedId = "";
    if (toolbar.dragging) {
      toolbar.dragging = false;
      return { handled: true, capture: false, invalidate: true };
    }
    if (hit && pressed === hit.id && hit.enabled !== false) {
      if (hit.action === "openMenu" && lastState) return overlay.popup(3, 42, mainMenuState(lastState));
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
