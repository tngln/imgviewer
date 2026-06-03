# WinApp UI Tooling

这个目录记录本地 agent 使用 Windows 官方 `winapp ui` 命令行工具的约定。

## 入口

- 主入口: `winapp ui`
- 截图统一使用 `winapp ui screenshot`。

## 常用命令

- 列出窗口:
  - `winapp ui list-windows -a imgviewer`
- 读取 UIA 树:
  - `winapp ui inspect -w 123456`
- 搜索元素:
  - `winapp ui search menu -w 123456`
- 点击 UIA 元素:
  - `winapp ui click menu -w 123456`
- 调用 UIA 元素:
  - `winapp ui invoke close -w 123456`
- 截图:
  - `winapp ui screenshot -w 123456 --output .\\tmp\\imgviewer.png --json`
  - `winapp ui screenshot -w 123456 --capture-screen --output .\\tmp\\imgviewer-with-popup.png --json`
- 等待元素:
  - `winapp ui wait-for menu -w 123456`
- 查看焦点:
  - `winapp ui get-focused -w 123456`

## 约定

- 对 imgviewer 先用 `winapp ui list-windows -a imgviewer` 找到目标 HWND。
- 后续命令优先使用 `-w <HWND>`，避免按标题匹配时受到空标题、动态标题或多窗口影响。
- 对弹出菜单、浮层、对话框等需要包含屏幕覆盖层的场景，截图必须加 `--capture-screen`。
- 如果 UIA 树中没有目标项，不要臆造 selector；先截图确认视觉状态，再记录该控件未暴露给 UIA。

## 注意事项

- `winapp ui click` 输出的坐标是工具内部选择的点击点，不应拿来和其他自定义鼠标工具混用。
- 自绘菜单项可能不会出现在 `inspect` 或 `search` 结果里；这属于需要记录的可访问性问题。
- 设置窗口等空标题窗口应通过 `winapp ui list-windows -a imgviewer` 返回的 HWND 定位。
