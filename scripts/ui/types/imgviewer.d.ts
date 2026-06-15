export type PointerEventType = "move" | "down" | "up" | "leave" | "wheel";
export type KeyEventType = "down" | "up";

export interface CanvasApi {
  clear(color: string): void;
  fillRect(x: number, y: number, width: number, height: number, color: string): void;
  strokeRect(x: number, y: number, width: number, height: number, color: string, strokeWidth?: number): void;
  fillText(text: string, x: number, y: number, width: number, height: number, color: string): void;
  strokeLine(x1: number, y1: number, x2: number, y2: number, color: string, strokeWidth?: number): void;
}

export interface HostApi {
  invalidate(): void;
  reload(): void;
  close(): void;
  log(text: string): void;
}

export interface SignalsApi {
  get(name: string): number | undefined;
  set(name: string, value: number): boolean;
  subscribe(name: string, callback: (value: number) => void): number;
  unsubscribe(id: number): boolean;
}

export interface RenderEnvironment {
  width: number;
  height: number;
  dpiScale: number;
  hovered: boolean;
  pressed: boolean;
  focused: boolean;
}

export interface DeveloperPointerEvent {
  type: PointerEventType;
  x: number;
  y: number;
  button: "none" | "left" | "right" | "middle";
  wheelDelta: number;
  ctrl: boolean;
  shift: boolean;
  alt: boolean;
}

export interface DeveloperKeyEvent {
  type: KeyEventType;
  virtualKey: number;
  ctrl: boolean;
  shift: boolean;
  alt: boolean;
  repeat: boolean;
}

export interface DeveloperEventResult {
  handled?: boolean;
  capture?: boolean;
  invalidate?: boolean;
  imeCaret?: ImeCaretPoint;
}

export interface DeveloperUiApp {
  render(canvas: CanvasApi, env: RenderEnvironment): void;
  pointer(event: DeveloperPointerEvent): DeveloperEventResult | void;
  key(event: DeveloperKeyEvent): DeveloperEventResult | void;
}

export type SettingKey =
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

export interface SettingsActionRow {
  name: string;
  shortcut: string;
}

export interface SettingsApi {
  get(name: SettingKey): number | boolean | undefined;
  set(name: SettingKey, value: number | boolean): boolean;
  save(): void;
  resetKeyBindings(): void;
  actionRows(filter: string): SettingsActionRow[];
}

export interface SettingsTextEvent {
  text: string;
}

export type NativeInputEvent =
  | { kind: "text"; text: string }
  | { kind: "imeStart" }
  | { kind: "imeComposition"; text: string }
  | { kind: "imeEnd" };

export interface ImeCaretPoint {
  x: number;
  y: number;
}

export interface InputEventResult extends DeveloperEventResult {
  imeCaret?: ImeCaretPoint;
}

export type MainActionName = string;

export interface OverlayHostApi {
  action(name: MainActionName, actionArg?: number): MainEventResult | false;
  openMenu(): MainEventResult;
  invalidate(): void;
}

export interface MainOverlayRows {
  label: string;
  value: string;
}

export interface MainOverlayColorSample {
  red: number;
  green: number;
  blue: number;
}

export interface MainOverlayState {
  title: string;
  topMost: boolean;
  maximized: boolean;
  editMode: boolean;
  toolbarScalePercent: number;
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

export type MainPointerEvent = DeveloperPointerEvent;
export type MainKeyEvent = DeveloperKeyEvent;
export type MainEventResult = InputEventResult & { action?: MainActionName; actionArg?: number };

export interface MainUiApp {
  render(canvas: CanvasApi, env: RenderEnvironment, state: MainOverlayState): void;
  pointer(event: MainPointerEvent): MainEventResult | void;
  key?(event: MainKeyEvent): MainEventResult | void;
  input?(event: NativeInputEvent): MainEventResult | void;
}

export type SettingsPointerEvent = DeveloperPointerEvent;
export type SettingsKeyEvent = DeveloperKeyEvent;
export type SettingsEventResult = InputEventResult;

export interface SettingsUiApp {
  render(canvas: CanvasApi, env: RenderEnvironment): void;
  pointer(event: SettingsPointerEvent): SettingsEventResult | void;
  key(event: SettingsKeyEvent): SettingsEventResult | void;
  input?(event: NativeInputEvent): InputEventResult | void;
  text(event: SettingsTextEvent): SettingsEventResult | void;
}

declare global {
  const host: HostApi;
  const overlay: OverlayHostApi;
  const signals: SignalsApi;
  const settings: SettingsApi;

  var imgviewerDeveloperUi: DeveloperUiApp;
  var imgviewerSettingsUi: SettingsUiApp;
  var imgviewerMainUi: MainUiApp;
}
