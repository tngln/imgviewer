/// <reference types="../types/imgviewer" />

import { uiBrand, uiText } from "./typography";

const colors = {
  background: "#FFF8FAFC",
  text: "#FF111827",
  muted: "#FF64748B",
  panel: "#FFFFFFFF",
  line: "#FFE2E8F0",
  accent: "#FF2563EB",
};

const notices = [
  ["stb_image.h v2.30", "Sean Barrett - MIT License or Public Domain", "third_parties/stb/LICENSE"],
  ["nlohmann_json", "Niels Lohmann - MIT License", "third_parties/nlohmann_json/LICENSE.MIT"],
  ["Windows Implementation Libraries", "Microsoft Corporation - MIT License", "third_parties/wil/LICENSE"],
];

function drawText(canvas: CanvasApi, text: string, x: number, y: number, width: number, color = colors.text, typeFace: TypeFace = uiText): void {
  canvas.fillText(text, typeFace, x, y, width, 24, color);
}

function section(canvas: CanvasApi, title: string, x: number, y: number, width: number, height: number): number {
  drawText(canvas, title, x, y, width, colors.text);
  y += 30;
  canvas.fillRect(x, y, width, height, colors.panel);
  canvas.strokeRect(x, y, width, height, colors.line, 1);
  return y + 12;
}

globalThis.imgviewerAboutUi = {
  render(canvas, env) {
    canvas.clear(colors.background);
    const left = 24;
    const width = Math.max(180, env.width - left * 2);
    let y = 24;

    drawText(canvas, "ImgViewer", left, y, width, colors.text, uiBrand);
    y += 30;
    drawText(canvas, "Lightweight native image viewer.", left, y, width, colors.muted);
    y += 24;
    drawText(canvas, "Development build", left, y, width, colors.muted);
    y += 42;

    const panelTop = section(canvas, "Third-party notices", left, y, width, 190);
    y = panelTop;
    for (const [name, detail, path] of notices) {
      drawText(canvas, name, left + 14, y, width - 28);
      y += 22;
      drawText(canvas, detail, left + 14, y, width - 28, colors.muted);
      y += 20;
      drawText(canvas, path, left + 14, y, width - 28, colors.muted);
      y += 30;
    }

    drawText(canvas, "Press Esc to close. Press F5 to reload this UI.", left, env.height - 34, width, colors.muted);
  },

  key(event) {
    if (event.type === "down" && event.virtualKey === 0x1b) {
      return { handled: true, action: "closeAbout" };
    }
    return { handled: false };
  },
};
