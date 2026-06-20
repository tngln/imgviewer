type PointerEventType = "move" | "down" | "up" | "leave" | "wheel";
type KeyEventType = "down" | "up";

type VectorPathCommand =
  | ["M", number, number]
  | ["L", number, number]
  | ["C", number, number, number, number, number, number]
  | ["Z"];

interface VectorIcon {
  id?: string;
  viewBox: [number, number, number, number];
  commands: VectorPathCommand[];
}

type FontWeightValue = number;
type FontStyleValue = number;
type FontStretchValue = number;

interface TypeFace {
  family: string;
  size: number;
  weight: FontWeightValue;
  style: FontStyleValue;
  stretch: FontStretchValue;
}

declare const FontWeight: {
  Thin: FontWeightValue;
  ExtraLight: FontWeightValue;
  Light: FontWeightValue;
  Normal: FontWeightValue;
  Medium: FontWeightValue;
  Semibold: FontWeightValue;
  Bold: FontWeightValue;
  ExtraBold: FontWeightValue;
  Black: FontWeightValue;
};

declare const FontStyle: {
  Normal: FontStyleValue;
  Oblique: FontStyleValue;
  Italic: FontStyleValue;
};

declare const FontStretch: {
  Normal: FontStretchValue;
  Condensed: FontStretchValue;
  Expanded: FontStretchValue;
};

declare function createTypeFace(
  family: string,
  size: number,
  weight?: FontWeightValue,
  style?: FontStyleValue,
  stretch?: FontStretchValue,
): TypeFace;

interface CanvasApi {
  clear(color: string): void;
  fillRect(x: number, y: number, width: number, height: number, color: string): void;
  strokeRect(x: number, y: number, width: number, height: number, color: string, strokeWidth?: number): void;
  fillText(text: string, typeFace: TypeFace, x: number, y: number, width: number, height: number, color: string): void;
  strokeLine(x1: number, y1: number, x2: number, y2: number, color: string, strokeWidth?: number): void;
  drawIcon(icon: VectorIcon, x: number, y: number, width: number, height: number, color: string, strokeWidth?: number): void;
}

interface HostApi {
  invalidate(): void;
  reload(): void;
  close(): void;
  log(text: string): void;
  systemPreferences: SystemPreferences;
}

interface SystemPreferences {
  caretBlinkTime: number;
  doubleClickTime: number;
  accentColor: string;
  accentColorOpaqueBlend: boolean;
  prefersDarkTheme: boolean;
  preferredLanguage: string;
  preferredLanguages: string[];
  highContrast: boolean;
  clientAreaAnimationEnabled: boolean;
}

interface SignalsApi {
  get(name: string): number | undefined;
  set(name: string, value: number): boolean;
  subscribe(name: string, callback: (value: number) => void): number;
  unsubscribe(id: number): boolean;
}

interface RenderEnvironment {
  width: number;
  height: number;
  dpiScale: number;
}

interface UiPointerEvent {
  type: PointerEventType;
  x: number;
  y: number;
  button: "none" | "left" | "right" | "middle";
  wheelDelta: number;
  ctrl: boolean;
  shift: boolean;
  alt: boolean;
}

interface UiKeyEvent {
  type: KeyEventType;
  virtualKey: number;
  ctrl: boolean;
  shift: boolean;
  alt: boolean;
  repeat: boolean;
}

interface UiEventResult {
  handled?: boolean;
  capture?: boolean;
  invalidate?: boolean;
  imeCaret?: ImeCaretPoint;
}

type SettingKey =
  | "language"
  | "initialImageViewMode"
  | "rememberWindowSize"
  | "pixelatedSampling"
  | "checkerboardBackground"
  | "borderlessWindow"
  | "edgeClickNavigation"
  | "windowOpacityPercent"
  | "toolbarScalePercent"
  | "edgeClickNavigationZonePercent";

interface SettingsActionRow {
  name: string;
  shortcut: string;
}

interface SettingsApi {
  get(name: SettingKey): number | boolean | undefined;
  set(name: SettingKey, value: number | boolean): boolean;
  save(): void;
  resetKeyBindings(): void;
  actionRows(filter: string): SettingsActionRow[];
}

interface SettingsTextEvent {
  text: string;
}

type NativeInputEvent =
  | { kind: "text"; text: string }
  | { kind: "imeStart" }
  | { kind: "imeComposition"; text: string }
  | { kind: "imeEnd" };

interface ImeCaretPoint {
  x: number;
  y: number;
}

interface InputEventResult extends UiEventResult {
  imeCaret?: ImeCaretPoint;
}

type MainActionName = string;

interface OverlayHostApi {
  action(name: MainActionName, actionArg?: number): MainEventResult | false;
  popup(x: number, y: number, state: PopupState): MainEventResult;
  invalidate(): void;
}

interface MainOverlayRows {
  label: string;
  value: string;
}

interface MainOverlayColorSample {
  red: number;
  green: number;
  blue: number;
}

interface MainRenderRect {
  x: number;
  y: number;
  width: number;
  height: number;
}

interface MainRenderResult {
  captionDragRects?: MainRenderRect[];
}

interface MainOverlayState {
  title: string;
  topMost: boolean;
  maximized: boolean;
  editMode: boolean;
  toolbarScalePercent: number;
  edgeClickNavigation: boolean;
  edgeClickNavigationZonePercent: number;
  colorPickerActive: boolean;
  actionEnabled: Record<MainActionName, boolean>;
  actionLabels: Record<MainActionName, string>;
  editToolbar: { visible: boolean; tool: string; dirty: boolean; canUndo: boolean; canRedo: boolean };
  colorPickerToolstrip: { visible: boolean; hasSample: boolean; hexText: string };
  penToolstrip: { visible: boolean; color: string; width: number };
  shapeToolstrip: { visible: boolean; kind: string; color: string };
  textToolstrip: { visible: boolean; fontFamily: string; fontSize: number; textColor: string; backgroundColor: string; hasBackground: boolean };
  selectionToolstrip: { visible: boolean };
  animation: { available: boolean; playing: boolean; loop: boolean; currentFrame: number; totalFrames: number };
  infoPanel: {
    visible: boolean;
    hasAnalysis: boolean;
    analysisUnavailable: boolean;
    name: string;
    path: string;
    dimensions: string;
    type: string;
    fileSize: string;
    modifiedTime: string;
    colorRows: MainOverlayRows[];
    exifRows: MainOverlayRows[];
    analysis: {
      sampledPixels: number;
      downsampled: boolean;
      average: MainOverlayColorSample;
      darkest: MainOverlayColorSample;
      brightest: MainOverlayColorSample;
    };
  };
  toast: { visible: boolean; text: string };
}

type MainPointerEvent = UiPointerEvent;
type MainKeyEvent = UiKeyEvent;
type MainEventResult = InputEventResult & {
  action?: MainActionName;
  actionArg?: number;
  popup?: { x: number; y: number; state: PopupState };
};

interface MainUiApp {
  render(canvas: CanvasApi, env: RenderEnvironment, state: MainOverlayState): MainRenderResult | void;
  pointer(event: MainPointerEvent): MainEventResult | void;
  key?(event: MainKeyEvent): MainEventResult | void;
  input?(event: NativeInputEvent): MainEventResult | void;
}

interface PopupActionPayload {
  action?: string;
  actionValue?: number;
  actionArg?: number;
}

interface PopupMenuItem extends PopupActionPayload {
  text: string;
  separator: boolean;
  checked: boolean;
  enabled: boolean;
  children: PopupMenuItem[];
}

interface PopupDropdownOption extends PopupActionPayload {
  text: string;
}

type PopupState =
  | { kind: "menu"; items: PopupMenuItem[] }
  | { kind: "dropdown"; width: number; selectedIndex: number; options: PopupDropdownOption[] }
  | { kind: "none" };

interface PopupMeasureResult {
  width: number;
  height: number;
}

interface PopupEventResult extends PopupActionPayload {
  handled?: boolean;
  invalidate?: boolean;
  close?: boolean;
  selectedIndex?: number;
}

interface PopupUiApp {
  measure(state: PopupState): PopupMeasureResult;
  render(canvas: CanvasApi, env: RenderEnvironment, state: PopupState): void;
  pointer(event: UiPointerEvent, state: PopupState): PopupEventResult | void;
  key(event: UiKeyEvent, state: PopupState): PopupEventResult | void;
}

type AboutKeyEvent = UiKeyEvent;
type AboutEventResult = InputEventResult & { action?: string };

interface AboutUiApp {
  render(canvas: CanvasApi, env: RenderEnvironment): void;
  key?(event: AboutKeyEvent): AboutEventResult | void;
}

type SettingsPointerEvent = UiPointerEvent;
type SettingsKeyEvent = UiKeyEvent;
type SettingsEventResult = InputEventResult;

interface SettingsUiApp {
  render(canvas: CanvasApi, env: RenderEnvironment): void;
  pointer(event: SettingsPointerEvent): SettingsEventResult | void;
  key(event: SettingsKeyEvent): SettingsEventResult | void;
  input?(event: NativeInputEvent): InputEventResult | void;
  text(event: SettingsTextEvent): SettingsEventResult | void;
}

declare const host: HostApi;
declare const overlay: OverlayHostApi;
declare const signals: SignalsApi;
declare const settings: SettingsApi;

declare var imgviewerSettingsUi: SettingsUiApp;
declare var imgviewerMainUi: MainUiApp;
declare var imgviewerPopupUi: PopupUiApp;
declare var imgviewerAboutUi: AboutUiApp;
