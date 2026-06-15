/// <reference types="../types/imgviewer" />

type Point = { x: number; y: number };

export type SvgIconInput = {
  id?: string;
  viewBox: [number, number, number, number];
  paths?: string[];
  lines?: Array<[number, number, number, number]>;
  polylines?: Point[][];
  polygons?: Point[][];
  rects?: Array<[number, number, number, number]>;
  circles?: Array<[number, number, number]>;
  ellipses?: Array<[number, number, number, number]>;
};

function command(verb: "M" | "L", point: Point): VectorPathCommand {
  return [verb, point.x, point.y];
}

function cubic(a: Point, b: Point, end: Point): VectorPathCommand {
  return ["C", a.x, a.y, b.x, b.y, end.x, end.y];
}

function tokenizePath(path: string): string[] {
  return [...path.matchAll(/[a-zA-Z]|[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?/g)].map((match) => match[0]);
}

function readNumber(tokens: string[], index: number): number {
  if (index >= tokens.length || /[a-zA-Z]/.test(tokens[index])) {
    throw new Error("Expected number in SVG path data");
  }
  const value = Number.parseFloat(tokens[index]);
  if (!Number.isFinite(value)) {
    throw new Error(`Invalid number in SVG path data: ${tokens[index]}`);
  }
  return value;
}

function parsePathData(path: string): VectorPathCommand[] {
  const tokens = tokenizePath(path);
  const result: VectorPathCommand[] = [];
  let index = 0;
  let op = "";
  let current: Point = { x: 0, y: 0 };
  let start: Point = { x: 0, y: 0 };

  const hasNumber = () => index < tokens.length && !/[a-zA-Z]/.test(tokens[index]);
  const point = (relative: boolean): Point => {
    const x = readNumber(tokens, index++);
    const y = readNumber(tokens, index++);
    return relative ? { x: current.x + x, y: current.y + y } : { x, y };
  };

  while (index < tokens.length) {
    if (/[a-zA-Z]/.test(tokens[index])) {
      op = tokens[index++];
    } else if (!op) {
      throw new Error("SVG path data must start with a command");
    }

    const relative = op === op.toLowerCase();
    switch (op.toUpperCase()) {
      case "M":
        current = point(relative);
        start = current;
        result.push(command("M", current));
        while (hasNumber()) {
          current = point(relative);
          result.push(command("L", current));
        }
        break;
      case "L":
        while (hasNumber()) {
          current = point(relative);
          result.push(command("L", current));
        }
        break;
      case "H":
        while (hasNumber()) {
          const x = readNumber(tokens, index++);
          current = { x: relative ? current.x + x : x, y: current.y };
          result.push(command("L", current));
        }
        break;
      case "V":
        while (hasNumber()) {
          const y = readNumber(tokens, index++);
          current = { x: current.x, y: relative ? current.y + y : y };
          result.push(command("L", current));
        }
        break;
      case "C":
        while (hasNumber()) {
          const a = point(relative);
          const b = point(relative);
          current = point(relative);
          result.push(cubic(a, b, current));
        }
        break;
      case "Q":
        while (hasNumber()) {
          const control = point(relative);
          const end = point(relative);
          const a = {
            x: current.x + (2 / 3) * (control.x - current.x),
            y: current.y + (2 / 3) * (control.y - current.y),
          };
          const b = {
            x: end.x + (2 / 3) * (control.x - end.x),
            y: end.y + (2 / 3) * (control.y - end.y),
          };
          current = end;
          result.push(cubic(a, b, current));
        }
        break;
      case "Z":
        result.push(["Z"]);
        current = start;
        break;
      case "A":
      case "S":
      case "T":
        throw new Error(`SVG path command ${op} is not supported; convert it to cubic curves first`);
      default:
        throw new Error(`Unsupported SVG path command: ${op}`);
    }
  }

  return result;
}

function appendPoints(commands: VectorPathCommand[], points: Point[], closed: boolean): void {
  for (const [index, point] of points.entries()) {
    commands.push(command(index === 0 ? "M" : "L", point));
  }
  if (closed && points.length > 0) {
    commands.push(["Z"]);
  }
}

function appendEllipse(commands: VectorPathCommand[], cx: number, cy: number, rx: number, ry: number): void {
  const k = 0.552284749831;
  commands.push(command("M", { x: cx + rx, y: cy }));
  commands.push(cubic({ x: cx + rx, y: cy + k * ry }, { x: cx + k * rx, y: cy + ry }, { x: cx, y: cy + ry }));
  commands.push(cubic({ x: cx - k * rx, y: cy + ry }, { x: cx - rx, y: cy + k * ry }, { x: cx - rx, y: cy }));
  commands.push(cubic({ x: cx - rx, y: cy - k * ry }, { x: cx - k * rx, y: cy - ry }, { x: cx, y: cy - ry }));
  commands.push(cubic({ x: cx + k * rx, y: cy - ry }, { x: cx + rx, y: cy - k * ry }, { x: cx + rx, y: cy }));
  commands.push(["Z"]);
}

export function svgIcon(input: SvgIconInput): VectorIcon {
  const commands: VectorPathCommand[] = [];
  for (const path of input.paths ?? []) commands.push(...parsePathData(path));
  for (const [x1, y1, x2, y2] of input.lines ?? []) {
    commands.push(command("M", { x: x1, y: y1 }), command("L", { x: x2, y: y2 }));
  }
  for (const points of input.polylines ?? []) appendPoints(commands, points, false);
  for (const points of input.polygons ?? []) appendPoints(commands, points, true);
  for (const [x, y, width, height] of input.rects ?? []) {
    appendPoints(commands, [{ x, y }, { x: x + width, y }, { x: x + width, y: y + height }, { x, y: y + height }], true);
  }
  for (const [cx, cy, r] of input.circles ?? []) appendEllipse(commands, cx, cy, r, r);
  for (const [cx, cy, rx, ry] of input.ellipses ?? []) appendEllipse(commands, cx, cy, rx, ry);
  if (commands.length === 0) {
    throw new Error("Vector icon has no geometry");
  }
  return input.id ? { id: input.id, viewBox: input.viewBox, commands } : { viewBox: input.viewBox, commands };
}
