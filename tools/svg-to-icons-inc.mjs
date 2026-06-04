#!/usr/bin/env node

import fs from "node:fs";
import path from "node:path";

const usage = `Usage: node tools/svg-to-icons-inc.mjs --out src/icons.inc tools/icons/*.svg

Converts simple outline SVGs into the Direct2D PathCommand data used by imgviewer.
Supported SVG elements: path, line, polyline, polygon, rect, circle, ellipse.
Supported path commands: M, L, H, V, C, Q, Z and their relative forms. Arcs are rejected.`;

function fail(message) {
  console.error(message);
  process.exit(1);
}

function parseArgs(argv) {
  const inputs = [];
  let out = "";
  for (let i = 0; i < argv.length; ++i) {
    const arg = argv[i];
    if (arg === "--help" || arg === "-h") {
      console.log(usage);
      process.exit(0);
    }
    if (arg === "--out") {
      out = argv[++i] ?? "";
      continue;
    }
    inputs.push(arg);
  }
  if (!out || inputs.length === 0) {
    fail(usage);
  }
  return { out, inputs };
}

function attrs(text) {
  const values = new Map();
  for (const match of text.matchAll(/([:\w-]+)\s*=\s*("([^"]*)"|'([^']*)')/g)) {
    values.set(match[1], match[3] ?? match[4] ?? "");
  }
  return values;
}

function numberAttr(values, name, fallback = undefined) {
    const value = values.get(name);
    if (value == null) {
        return fallback;
  }
  const parsed = Number.parseFloat(value);
  if (!Number.isFinite(parsed)) {
    fail(`Invalid numeric attribute ${name}="${value}"`);
    }
    return parsed;
}

function requiredNumberAttr(values, name, file) {
  const value = numberAttr(values, name);
  if (value == null) {
    fail(`${file}: missing numeric attribute ${name}`);
  }
  return value;
}

function parseViewBox(svg, file) {
  const svgMatch = svg.match(/<svg\b([^>]*)>/i);
  if (!svgMatch) {
    fail(`${file}: missing <svg> root`);
  }
  const values = attrs(svgMatch[1]);
  const viewBox = values.get("viewBox");
  if (viewBox) {
    const parts = viewBox.trim().split(/[\s,]+/).map(Number);
    if (parts.length === 4 && parts.every(Number.isFinite)) {
      return { x: parts[0], y: parts[1], width: parts[2], height: parts[3] };
    }
    fail(`${file}: invalid viewBox="${viewBox}"`);
  }
  const width = numberAttr(values, "width");
  const height = numberAttr(values, "height");
  if (width == null || height == null) {
    fail(`${file}: SVG needs viewBox or width/height`);
  }
  return { x: 0, y: 0, width, height };
}

function command(verb, points = []) {
  return { verb, points };
}

function tokenizePath(d) {
  return [...d.matchAll(/[a-zA-Z]|[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?/g)].map((m) => m[0]);
}

function readNumber(tokens, index, file) {
  if (index >= tokens.length || /[a-zA-Z]/.test(tokens[index])) {
    fail(`${file}: expected number in path data`);
  }
  const value = Number.parseFloat(tokens[index]);
  if (!Number.isFinite(value)) {
    fail(`${file}: invalid number "${tokens[index]}" in path data`);
  }
  return value;
}

function parsePathData(d, file) {
  const tokens = tokenizePath(d);
  const result = [];
  let i = 0;
  let op = "";
  let current = { x: 0, y: 0 };
  let start = { x: 0, y: 0 };

  const hasNumber = () => i < tokens.length && !/[a-zA-Z]/.test(tokens[i]);
  const point = (relative) => {
    const x = readNumber(tokens, i++, file);
    const y = readNumber(tokens, i++, file);
    return relative ? { x: current.x + x, y: current.y + y } : { x, y };
  };

  while (i < tokens.length) {
    if (/[a-zA-Z]/.test(tokens[i])) {
      op = tokens[i++];
    } else if (!op) {
      fail(`${file}: path data must start with a command`);
    }

    const relative = op === op.toLowerCase();
    switch (op.toUpperCase()) {
      case "M": {
        current = point(relative);
        start = current;
        result.push(command("MoveTo", [current]));
        while (hasNumber()) {
          current = point(relative);
          result.push(command("LineTo", [current]));
        }
        break;
      }
      case "L":
        while (hasNumber()) {
          current = point(relative);
          result.push(command("LineTo", [current]));
        }
        break;
      case "H":
        while (hasNumber()) {
          const x = readNumber(tokens, i++, file);
          current = { x: relative ? current.x + x : x, y: current.y };
          result.push(command("LineTo", [current]));
        }
        break;
      case "V":
        while (hasNumber()) {
          const y = readNumber(tokens, i++, file);
          current = { x: current.x, y: relative ? current.y + y : y };
          result.push(command("LineTo", [current]));
        }
        break;
      case "C":
        while (hasNumber()) {
          const a = point(relative);
          const b = point(relative);
          current = point(relative);
          result.push(command("CubicTo", [a, b, current]));
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
          result.push(command("CubicTo", [a, b, current]));
        }
        break;
      case "Z":
        result.push(command("Close"));
        current = start;
        break;
      case "A":
      case "S":
      case "T":
        fail(`${file}: path command ${op} is not supported; convert it to cubic curves first`);
      default:
        fail(`${file}: unsupported path command ${op}`);
    }
  }
  return result;
}

function parsePoints(text, file) {
  const values = text.trim().split(/[\s,]+/).filter(Boolean).map(Number);
  if (values.length % 2 !== 0 || values.some((value) => !Number.isFinite(value))) {
    fail(`${file}: invalid point list "${text}"`);
  }
  const points = [];
  for (let i = 0; i < values.length; i += 2) {
    points.push({ x: values[i], y: values[i + 1] });
  }
  return points;
}

function cubicEllipse(cx, cy, rx, ry) {
  const k = 0.552284749831;
  return [
    command("MoveTo", [{ x: cx + rx, y: cy }]),
    command("CubicTo", [{ x: cx + rx, y: cy + k * ry }, { x: cx + k * rx, y: cy + ry }, { x: cx, y: cy + ry }]),
    command("CubicTo", [{ x: cx - k * rx, y: cy + ry }, { x: cx - rx, y: cy + k * ry }, { x: cx - rx, y: cy }]),
    command("CubicTo", [{ x: cx - rx, y: cy - k * ry }, { x: cx - k * rx, y: cy - ry }, { x: cx, y: cy - ry }]),
    command("CubicTo", [{ x: cx + k * rx, y: cy - ry }, { x: cx + rx, y: cy - k * ry }, { x: cx + rx, y: cy }]),
    command("Close"),
  ];
}

function parseElement(tag, values, file) {
  if (tag === "path") {
    const d = values.get("d");
    if (!d) {
      fail(`${file}: <path> missing d attribute`);
    }
    return parsePathData(d, file);
  }
  if (tag === "line") {
    return [
      command("MoveTo", [{ x: numberAttr(values, "x1", 0), y: numberAttr(values, "y1", 0) }]),
      command("LineTo", [{ x: numberAttr(values, "x2", 0), y: numberAttr(values, "y2", 0) }]),
    ];
  }
  if (tag === "polyline" || tag === "polygon") {
    const points = parsePoints(values.get("points") ?? "", file);
    const commands = points.map((p, index) => command(index === 0 ? "MoveTo" : "LineTo", [p]));
    if (tag === "polygon") {
      commands.push(command("Close"));
    }
    return commands;
  }
  if (tag === "rect") {
    if (numberAttr(values, "rx", 0) !== 0 || numberAttr(values, "ry", 0) !== 0) {
      fail(`${file}: rounded rect is not supported; convert it to path data first`);
    }
    const x = numberAttr(values, "x", 0);
    const y = numberAttr(values, "y", 0);
    const width = requiredNumberAttr(values, "width", file);
    const height = requiredNumberAttr(values, "height", file);
    return [
      command("MoveTo", [{ x, y }]),
      command("LineTo", [{ x: x + width, y }]),
      command("LineTo", [{ x: x + width, y: y + height }]),
      command("LineTo", [{ x, y: y + height }]),
      command("Close"),
    ];
  }
  if (tag === "circle") {
    const r = requiredNumberAttr(values, "r", file);
    return cubicEllipse(numberAttr(values, "cx", 0), numberAttr(values, "cy", 0), r, r);
  }
  if (tag === "ellipse") {
    return cubicEllipse(
      numberAttr(values, "cx", 0),
      numberAttr(values, "cy", 0),
      requiredNumberAttr(values, "rx", file),
      requiredNumberAttr(values, "ry", file),
    );
  }
  return [];
}

function parseSvg(file) {
  const svg = fs.readFileSync(file, "utf8");
  const commands = [];
  const viewBox = parseViewBox(svg, file);
  for (const match of svg.matchAll(/<(path|line|polyline|polygon|rect|circle|ellipse)\b([^>]*)>/gi)) {
    commands.push(...parseElement(match[1].toLowerCase(), attrs(match[2]), file));
  }
  if (commands.length === 0) {
    fail(`${file}: no supported SVG geometry found`);
  }
  return { name: iconName(file), viewBox, commands };
}

function iconName(file) {
  return path.basename(file, path.extname(file))
    .split(/[^a-zA-Z0-9]+/)
    .filter(Boolean)
    .map((part) => part[0].toUpperCase() + part.slice(1))
    .join("");
}

function f(value) {
  const rounded = Math.abs(value) < 0.00005 ? 0 : value;
  let text = rounded.toFixed(4).replace(/\.?0+$/, "");
  if (!text.includes(".")) {
    text += ".0";
  }
  return `${text}f`;
}

function pointInitializer(point) {
  return `{${f(point.x)}, ${f(point.y)}}`;
}

function commandInitializer(item) {
  if (item.verb === "Close") {
    return "    {PathVerb::Close, {{}, {}, {}}},";
  }
  const points = [...item.points];
  while (points.length < 3) {
    points.push(null);
  }
  const initialized = points.map((point) => (point ? pointInitializer(point) : "{}")).join(", ");
  return `    {PathVerb::${item.verb}, {${initialized}}},`;
}

function renderIcon(icon) {
  const pathName = `k${icon.name}IconPath`;
  const iconConstName = `k${icon.name}Icon`;
  return `constexpr PathCommand ${pathName}[] = {
${icon.commands.map(commandInitializer).join("\n")}
};

constexpr PathIcon ${iconConstName} = MakePathIcon(${pathName}, {${f(icon.viewBox.x)}, ${f(icon.viewBox.y)}, ${f(icon.viewBox.x + icon.viewBox.width)}, ${f(icon.viewBox.y + icon.viewBox.height)}});
`;
}

function render(icons) {
  return `// Generated by tools/svg-to-icons-inc.mjs. Do not edit by hand.

namespace icons {

enum class PathVerb {
    MoveTo,
    LineTo,
    CubicTo,
    Close,
};

struct PathCommand {
    PathVerb verb;
    D2D1_POINT_2F points[3];
};

struct PathIcon {
    const PathCommand* commands;
    size_t command_count;
    D2D1_RECT_F view_box;
};

template <size_t Count>
constexpr PathIcon MakePathIcon(const PathCommand (&commands)[Count], D2D1_RECT_F view_box)
{
    return PathIcon{commands, Count, view_box};
}

${icons.map(renderIcon).join("\n")}
} // namespace icons
`;
}

const { out, inputs } = parseArgs(process.argv.slice(2));
const icons = inputs.map(parseSvg);
const generated = render(icons);
fs.mkdirSync(path.dirname(out), { recursive: true });
fs.writeFileSync(out, generated, "utf8");
