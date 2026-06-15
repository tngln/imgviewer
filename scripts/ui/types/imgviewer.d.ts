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

export type SettingsPointerEvent = DeveloperPointerEvent;
export type SettingsKeyEvent = DeveloperKeyEvent;
export type SettingsEventResult = DeveloperEventResult;

export interface SettingsUiApp {
  render(canvas: CanvasApi, env: RenderEnvironment): void;
  pointer(event: SettingsPointerEvent): SettingsEventResult | void;
  key(event: SettingsKeyEvent): SettingsEventResult | void;
  text(event: SettingsTextEvent): SettingsEventResult | void;
}

declare global {
  const host: HostApi;
  const signals: SignalsApi;
  const settings: SettingsApi;

  var imgviewerDeveloperUi: DeveloperUiApp;
  var imgviewerSettingsUi: SettingsUiApp;
}
