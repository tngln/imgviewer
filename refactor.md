# ImgViewer 重构进度

## 已完成的清理

### 1. 统一 `IsKeyDown` 函数
- **之前**: `imgviewer.host.cc` 和 `ui.window.cc` 各自在匿名命名空间定义了相同的 `IsKeyDown(int virtual_key)` 函数
- **之后**: 移动到 `win32.util.hpp/cc` 中作为 `util::IsKeyDown()`
- **文件变更**: 
  - `win32.util.hpp` - 添加声明
  - `win32.util.cc` - 添加实现
  - `imgviewer.host.cc` - 删除本地定义，改用 `util::IsKeyDown()`
  - `ui.window.cc` - 删除本地定义，通过 `UiModifiers::Current()` 间接使用

### 2. 统一 `CurrentUiModifiers` / `CurrentModifiers`
- **之前**: `imgviewer.host.cc` 有自由函数 `CurrentUiModifiers()`，`UiWindowHost` 有成员函数 `CurrentModifiers()`
- **之后**: 在 `UiModifiers` 结构体中添加静态方法 `UiModifiers::Current()`
- **文件变更**:
  - `ui.events.hpp` - 在 `UiModifiers` 中添加 `static Current()` 声明
  - `ui.events.cc` - 新文件，实现 `UiModifiers::Current()`
  - `CMakeLists.txt` - 添加 `ui.events.cc`
  - `imgviewer.host.cc` - 删除本地定义，改用 `UiModifiers::Current()`
  - `ui.window.cc` - `CurrentModifiers()` 内部调用 `UiModifiers::Current()`

### 3. 移除 `TextBox` 中不必要的 `const_cast`
- **之前**: `TextBox::Render()` 使用 `const_cast` 修改 `caret_point_`、`dwrite_factory_`、`text_format_`
- **之后**: 将这些成员标记为 `mutable`，移除 `const_cast`
- **文件变更**:
  - `ui.textbox.hpp` - 添加 `mutable` 关键字到 `caret_visible_`、`caret_point_`、`dwrite_factory_`、`text_format_`
  - `ui.textbox.hpp` - `SetTextServices()` 改为 `const` 方法
  - `ui.textbox.cc` - 移除两处 `const_cast`

### 4. 提取格式化函数到 `util.format.hpp/cc`
- **之前**: `FormatFileSize`、`FormatFileTime`、`FormatImageDimensions`、`FormatImageType` 在 `imgviewer.cc` 匿名命名空间
- **之后**: 移动到 `util::` 命名空间
- **文件变更**:
  - `util.format.hpp` - 新文件，声明格式化函数
  - `util.format.cc` - 新文件，实现格式化函数
  - `CMakeLists.txt` - 添加 `util.format.cc`
  - `imgviewer.cc` - 删除本地定义，改用 `util::Format*()` 函数

### 5. 提取按键显示函数到 `imgviewer.keybindings`
- **之前**: `KeyName`、`GestureText` 在 `imgviewer.settings.cc` 匿名命名空间
- **之后**: 移动到 `imgviewer.keybindings.hpp/cc`，与 `KeyGesture` 类型放在一起
- **文件变更**:
  - `imgviewer.keybindings.hpp` - 添加 `KeyName()`、`GestureText()` 声明
  - `imgviewer.keybindings.cc` - 添加实现
  - `imgviewer.settings.cc` - 删除本地定义

### 6. 抽取输入事件构造工厂函数
- **之前**: `imgviewer.host.cc`、`ui.window.cc`、`ui.popup.cc` 中反复手写 `UiInputEvent{.type=..., .pointer=..., .point=..., .hwnd=..., .popup_host=...}` 聚合初始化
- **之后**: 在 `UiInputEvent` 中添加静态工厂方法 `Pointer()` 和 `Key()`，自动镜像 point/popup_host 字段
- **文件变更**:
  - `ui.events.hpp` - 添加 `UiInputEvent::Pointer()` 和 `UiInputEvent::Key()` 静态方法
  - `imgviewer.host.cc` - 替换 7 处手写构造为工厂调用
  - `ui.window.cc` - 替换 4 处手写构造为工厂调用
  - `ui.popup.cc` - 替换 2 处手写构造为工厂调用
- **未替换**: `WM_MOUSELEAVE`（主窗口）因 null context 处理特殊；`WM_CHAR`/`WM_IME_*`/`WM_CONTEXTMENU`/`WM_TIMER`/`WM_ACTIVATE` 等非 Pointer/Key 事件

## 待清理项目

### 低风险
- [ ] 统一常量命名风格（有些用 `k` 前缀，有些用大写）

### 中等风险
- [ ] `ClientPixelSize` 函数可能可以移到 `win32.util`（但需要处理 D2D1 依赖）

## 架构级问题（暂不处理）

这些问题需要更大范围的重构，不适合作为小步骤清理：

1. **双重 UI 宿主系统** - 主窗口用 `WindowProc`，子窗口用 `UiWindowHost`
2. **渲染系统分裂** - DirectComposition vs HwndRenderTarget
3. **布局系统缺失** - 纯命令式手动计算
4. **`ImgViewerContext` 过大** - God Object 问题
