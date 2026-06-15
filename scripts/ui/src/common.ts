/// <reference types="../types/imgviewer" />

export type Rect = { x: number; y: number; width: number; height: number };

export type ButtonColors = {
  fill: string;
  hoverFill: string;
  activeFill: string;
  stroke: string;
  hoverStroke: string;
  text: string;
};

export function clamp(value: number, min: number, max: number): number {
  return Math.max(min, Math.min(max, value));
}

export function contains(rect: Rect, x: number, y: number): boolean {
  return x >= rect.x && y >= rect.y && x <= rect.x + rect.width && y <= rect.y + rect.height;
}

export function hitTestRecord<T extends Rect>(items: Record<string, T>, x: number, y: number): string {
  for (const [name, rect] of Object.entries(items)) {
    if (contains(rect, x, y)) {
      return name;
    }
  }
  return "";
}

export function hitTestReverse<T extends Rect>(items: T[], x: number, y: number): T | undefined {
  for (let index = items.length - 1; index >= 0; --index) {
    if (contains(items[index], x, y)) {
      return items[index];
    }
  }
  return undefined;
}

export function drawText(canvas: CanvasApi, text: string, x: number, y: number, width: number, color = "#FF172033", height = 22): void {
  canvas.fillText(text, x, y, width, height, color);
}

export function drawButton(
  canvas: CanvasApi,
  rect: Rect,
  label: string,
  state: { hover: boolean; active: boolean },
  colors: ButtonColors,
): void {
  const fill = state.active ? colors.activeFill : state.hover ? colors.hoverFill : colors.fill;
  const stroke = state.hover ? colors.hoverStroke : colors.stroke;
  canvas.fillRect(rect.x, rect.y, rect.width, rect.height, fill);
  canvas.strokeRect(rect.x, rect.y, rect.width, rect.height, stroke, 1);
  drawText(canvas, label, rect.x + 12, rect.y + 8, rect.width - 24, colors.text, rect.height - 8);
}
