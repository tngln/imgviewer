/// <reference types="../types/imgviewer" />

import { type Rect, drawButton as drawCommonButton, hitTestRecord } from "./common";

const counterName = "developer.counter";
let hover = "";
let active = "";
let lastMessage = "TypeScript UI loaded";
let subscription = 0;

const buttons: Record<string, Rect> = {
  increment: { x: 24, y: 96, width: 132, height: 34 },
  reset: { x: 168, y: 96, width: 92, height: 34 },
  reload: { x: 272, y: 96, width: 96, height: 34 },
  close: { x: 380, y: 96, width: 90, height: 34 },
};

function hitTest(x: number, y: number): string {
  return hitTestRecord(buttons, x, y);
}

function drawButton(canvas: CanvasApi, name: string, label: string): void {
  const rect = buttons[name];
  const isActive = active === name;
  const isHover = hover === name;
  drawCommonButton(canvas, rect, label, { hover: isHover, active: isActive }, {
    fill: "#FF243044",
    hoverFill: "#FF2E4A6F",
    activeFill: "#FF3366AA",
    stroke: "#FF546276",
    hoverStroke: "#FF7DB7FF",
    text: "#FFEAF2FF",
  });
}

function counter(): number {
  return signals.get(counterName) ?? 0;
}

function activate(name: string): void {
  if (name === "increment") {
    signals.set(counterName, counter() + 1);
  } else if (name === "reset") {
    signals.set(counterName, 0);
  } else if (name === "reload") {
    host.reload();
  } else if (name === "close") {
    host.close();
  }
}

if (subscription === 0) {
  subscription = signals.subscribe(counterName, value => {
    lastMessage = `Native signal changed: ${value}`;
    host.invalidate();
  });
}

globalThis.imgviewerDeveloperUi = {
  render(canvas, env) {
    canvas.clear("#FF111827");
    canvas.fillText("ImgViewer Developer UI", 24, 22, env.width - 48, 28, "#FFF8FAFC");
    canvas.fillText("Rendered by TypeScript -> Bun -> QuickJS -> native canvas", 24, 52, env.width - 48, 22, "#FF93A4B8");

    drawButton(canvas, "increment", "Increment");
    drawButton(canvas, "reset", "Reset");
    drawButton(canvas, "reload", "Reload");
    drawButton(canvas, "close", "Close");

    canvas.fillRect(24, 154, Math.max(0, env.width - 48), 1, "#FF334155");
    canvas.fillText(`developer.counter = ${counter()}`, 24, 180, env.width - 48, 24, "#FFE2E8F0");
    canvas.fillText(lastMessage, 24, 210, env.width - 48, 24, "#FFCBD5E1");
    canvas.fillText("F5 reloads the script. Esc closes this window.", 24, env.height - 44, env.width - 48, 24, "#FF64748B");
  },

  pointer(event) {
    if (event.type === "move") {
      const next = hitTest(event.x, event.y);
      const changed = next !== hover;
      hover = next;
      return { handled: next !== "", invalidate: changed };
    }

    if (event.type === "down" && event.button === "left") {
      active = hitTest(event.x, event.y);
      return { handled: active !== "", capture: active !== "", invalidate: active !== "" };
    }

    if (event.type === "up" && event.button === "left") {
      const target = hitTest(event.x, event.y);
      const clicked = active !== "" && active === target;
      const wasActive = active !== "";
      active = "";
      if (clicked) {
        activate(target);
      }
      return { handled: wasActive || target !== "", capture: false, invalidate: wasActive };
    }

    if (event.type === "leave") {
      const changed = hover !== "";
      hover = "";
      active = "";
      return { handled: changed, capture: false, invalidate: changed };
    }

    return { handled: false };
  },

  key(event) {
    if (event.type === "down" && event.virtualKey === 0x52 && event.ctrl) {
      host.reload();
      return { handled: true };
    }
    return { handled: false };
  },
};
