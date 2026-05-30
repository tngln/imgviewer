# WinApp UI Tooling

这个目录放的是给本地 agent 使用的 Windows UI 自动化工具。

## 入口

- 主入口: `python tools/winapp_ui.py`
- Python CLI 负责窗口选择、鼠标键盘事件和命令编排。
- `tools/winapp_ui_helper.cs` 会在首次使用时被自动编译到 `tools/.cache/winapp_ui_helper.exe`，负责 UI Automation、截图和图像检查。

## 常用命令

- 查找窗口:
  - `python tools/winapp_ui.py find-window --class-name ImgViewerWindow`
- 读取 UIA 树:
  - `python tools/winapp_ui.py tree --class-name ImgViewerWindow`
- 截图:
  - `python tools/winapp_ui.py screenshot --class-name ImgViewerWindow --mode screen --out .\\tmp\\imgviewer.png`
  - `python tools/winapp_ui.py screenshot --hwnd 123456 --mode printwindow --out .\\tmp\\imgviewer-frame.png`
- 鼠标移动/点击:
  - `python tools/winapp_ui.py move 40 20 --window-relative`
  - `python tools/winapp_ui.py click 120 140 --window-relative`
- 键盘输入:
  - `python tools/winapp_ui.py key ctrl o`
  - `python tools/winapp_ui.py type "hello"`
- 图像检查:
  - `python tools/winapp_ui.py image-info .\\tmp\\imgviewer.png`
  - `python tools/winapp_ui.py image-diff .\\tmp\\before.png .\\tmp\\after.png`

## 约定

- 默认窗口类名是 `ImgViewerWindow`，所以对 imgviewer 做验证时通常不需要额外传类名。
- `--window-relative` 表示坐标相对目标窗口左上角；不带时使用屏幕绝对坐标。
- `screen` 模式截图会尝试把目标窗口置前后再抓屏，更接近用户实际看到的结果。
- `printwindow` 模式更稳定，但对 DirectComposition/分层窗口可能只能抓到宿主窗口内容。

## 注意事项

- 这个工具默认依赖系统自带的 .NET Framework C# 编译器和 UIAutomation 程序集。
- 如果要做视觉验证，优先使用 `screen` 模式；如果发现抓到的是别的窗口或不完整内容，先确认目标窗口在最前面且没有被遮挡。
- 这个工具输出尽量使用 JSON，方便后续脚本和 agent 直接消费。
