# ImgViewer 重构评估与执行计划（refactor.md）

> 基于 2026-06 全量代码审读（src/ 约 27.8k 行，HEAD = `a88a912 everything dcomp.`）。
> 本文档分为四部分：现状诊断、债务清单（带文件定位）、重构方向收益排序、可供 Coding Agent 执行的分步操作。

---

## 0. 一句话结论

代码库实际上是**"retained 元素树 + immediate 驱动方式"的混合体**：`UiElement` 树、ID、metadata、a11y 都已存在（retained 的骨架是好的），但主窗口绕过了这棵树——布局靠手工 Arrange 链、事件靠手写优先级 if 链、状态靠每帧整体重推、渲染靠每个事件同步全量重画。**最大的杠杆不是新写框架，而是"信任已有的树"并删除绕过它的平行结构**。预计完成本文 P1–P3 后可净删 4000–6000 行，且不改变外部行为。

---

## 1. 现状诊断

### 1.1 分层地图

| 层 | 模块 | 角色 | 健康度 |
|---|---|---|---|
| 框架 | `ui.element` `ui.events` `ui.root` `ui.cc`(UiController) | 元素树、事件、焦点/捕获/悬停 | ★★★☆ 骨架好，分发模型半成品 |
| 框架 | `ui.panel` `ui.layout` `ui.button` `ui.slider` `ui.table` `ui.textbox`… | 控件库 | ★★★☆ 可用，但事件处理逻辑重复散落 |
| 框架 | `ui.window`(UiWindowHost) `ui.popup` `ui.renderer`+`ui.surface` `ui.graphics_device` | 窗口宿主 / 弹窗 / DComp | ★★☆☆ **三套 DComp surface 代码并存** |
| 宿主 | `imgviewer.host.*`（6 个文件） | 主窗口消息泵 + effects 合并 | ★★☆☆ 与 UiWindowHost 大面积职责重叠 |
| 应用 UI | `imgviewer.ui.*`（toolbar + 7 个 toolstrip + titlebar + info_panel） | 主窗口 UI | ★★☆☆ 7 份平行样板，AI 生成痕迹最重 |
| 应用 | `imgviewer.cc` `imgviewer.viewer` `imgviewer.edit` `imgviewer.interaction` | 业务控制器 | ★★★☆ edit 是 god object，imgviewer.cc 是杂物间 |
| 基础 | `image.*` `win32.*` `util.*` `math` | 解码/系统封装 | ★★★★ 边界清晰，基本不用动 |

### 1.2 关键架构事实

1. **两套窗口宿主并存**。主窗口走 `imgviewer.host.cc` 手写 WindowProc（5 个顺序 switch 处理链）；Settings/About/Developer 走 `UiWindowHost`。两边各自实现了：IME 组合串读取（`imgviewer.host.cc:114` vs `ui.window.cc:604`）、caret timer、popup 转发、capture、a11y provider 创建、DComp surface 管理、渲染调度。`UiWindowHost` 是后写的、更干净的那套；主窗口是历史包袱。

2. **两种渲染调度模型**。主窗口在事件处理内**同步**调 `RenderImgViewer()`（`ApplyHostEffects`, `imgviewer.host.cc:335`），一次鼠标按下可能触发 2–3 次全量渲染（`HandleImgViewerPointerMessage` 的 WM_LBUTTONDOWN 路径调了 3 次 `ApplyHostEffects`，每次都可能渲染）。`UiWindowHost` 则是正确的 `Invalidate → WM_PAINT → Render` 合并模型。

3. **每帧全量 Measure/Arrange/Render**（`UiController::Render`, `ui.cc:252`），没有布局缓存与脏区域；对当前 UI 规模可接受，但说明"retained"只 retain 了对象，没 retain 布局/绘制结果。

4. **事件分发是双轨制**：`UiController::DispatchPointerEvent`（`ui.cc:164`）先给 `root_->OnPointerEvent()`（即 `ImgViewerUi` 手写的 9 层 if 优先级链, `imgviewer.ui.cc:118-153`），未处理再 `FindById(target)->OnInputEvent()`（O(n) 查找、包一层 `UiInputEvent`）。元素同时有 `OnInputEvent/OnPointerEvent/OnKeyEvent` 三个虚函数入口，是一次未完成的"统一事件"迁移。

5. **状态同步是推送式全刷**：`RenderImgViewer`（`imgviewer.cc:83`）每次渲染前把 7 个 toolstrip 的 state struct 全部重建重推。这其实是 immediate 思维，但好处是单向数据流——保留此方向，删掉的是七份接收端样板。

6. **Action 系统是整数隧道**：`UiAction` 只是 `int` 包装（`ui.action.hpp`），值域直接复用 `ImgViewerAction`，框架层用负数 sentinel 表示文本编辑动作。**没有参数化 payload**，导致枚举爆炸：每种笔色/线宽/字号/背景色都是独立 action（`EditPenColorRed`…`EditTextSize36` 共 40+ 个），连锁产生：
   - `ExecuteImgViewerAction` 300 行 switch（`imgviewer.cc:708-1018`）
   - `IsImgViewerActionEnabled` 50 行 `||` 链（`imgviewer.cc:219-265`）
   - `ImgViewerUi::HandleUiAction` 里 190 行手搓菜单树并镜像所有 state（`imgviewer.ui.cc:161-289`）
   - 每个 toolstrip 的 ButtonSpec 表
   同一信息（颜色表、宽度表、字号表）在 **4 个地方**重复维护。

---

## 2. 债务与 pitfall 清单

### 2.1 正确性 / 生命周期

| # | 问题 | 位置 | 说明 |
|---|---|---|---|
| L1 | **单槽 modal 被多窗口覆写** | `imgviewer.interaction.cc:112` | `modal_` 只有一个槽。先开 Settings 再开 About，关 About 后 `ClearModal(About)` 把 modal 清成 None，而 Settings 仍开着 → 边缘点击翻页等被 modal 门控的行为在 Settings 开着时恢复了。应改为计数/集合或栈。 |
| L2 | **子窗口 context 用 `void*` + new/delete + PostMessage 握手自杀** | `imgviewer.hpp:55-57`、`imgviewer.settings.cc:717-941`、about/developer 同构 | `SettingsWindowContext` 裸 new，OnDestroy 时 PostMessage 给 owner，owner 收到后 delete。主窗口先于子窗口销毁（WM_DESTROY → PostQuitMessage）时消息丢失 → 泄漏（退出时无害但是设计债）。三个窗口三份相同握手代码。 |
| L3 | **`ApplyHostEffects` 自递归** | `imgviewer.host.cc:301-354` | effects → DispatchUiAction → 新 effects → 递归 ApplyHostEffects；ExecuteImgViewerAction 内部又会直接调 RenderImgViewer/ApplyMerged。无递归深度保护，且"判断 action_effects 是否为空"的 8 项条件是手写的（漏字段风险）。 |
| L4 | **无设备丢失处理** | 全部 DComp/D3D 路径 | 没有任何 `DXGI_ERROR_DEVICE_REMOVED` / `IDCompositionDevice` 失效的恢复逻辑。GPU 驱动重置后应用会进入静默黑屏或持续失败。Retained 框架落地前应至少集中化 HRESULT 出口以便日后补。 |
| L5 | **截屏路径在 UI 线程 Sleep** | `imgviewer.cc:1101,1128` | `Sleep(120)`/`Sleep(80)` 阻塞消息泵。 |
| L6 | WM_LBUTTONDOWN 不检查 `CanUiReceivePointer`，而 Move/Up 检查 | `imgviewer.host.pointer.cc:81` vs `:32,:168` | 不对称的门控；color picker / 捕获状态下按下事件仍进 UI 树。即使现状无可见 bug，也是踩坑点。 |
| L7 | `pressed_key_actions[256]` 仅按 VK 索引 | `imgviewer.hpp:49`、`host.keyboard.cc:98` | 按下时带修饰键解析 action，抬起时只按 VK 查表——修饰键中途变化时 OnActionUp 收到的 action 与实际不符（目前 viewer 只用方向类 action，影响小）。 |
| L8 | 全局静态 `UiElementIdGenerator` | `ui.element.cc:14` | ID 永续增长 + 每次 `ResetImgViewerUi`（语言切换）整树重建，旧 ID 仍可能存在于 tooltip/a11y 侧。`FindById` 为线性递归查找，每个事件至少一次。 |
| L9 | a11y 枚举 O(n²) | `ui.cc:282-292` | `ElementMetadataAt(i)` 每次调用都重建整个 `AccessibleElements` 向量；UIA 遍历时是平方复杂度。 |
| L10 | `kImgViewerUiActionMessage` 同一消息号被主窗口与全部 UiWindowHost 共用 | `imgviewer.messages.hpp:6` | 行为正确（各自 hwnd），但 effect_target 的 `UiElementId` 通过 LPARAM 跨窗口传整数（`ui.window.cc:155`），依赖全局 ID 生成器不冲突——隐式耦合。 |

### 2.2 框架层语义问题

| # | 问题 | 位置 |
|---|---|---|
| F1 | `UiRoot` 接口被 a11y 撑爆：`ElementRangeValue/Minimum/Maximum/SmallChange/LargeChange/SetElementRangeValue/ElementValue/IsElementReadOnly` 共 8 个虚函数，全是 slider/textbox 的事，且 `UiController` 1:1 纯转发了 15 个方法（`ui.cc:316-354`）。这些应该是**元素自身的能力**（虚函数或接口查询），由 controller 直接走树。 | `ui.root.hpp:31-38` |
| F2 | `UiInputEvent` 是"万物结构体"：同时含 pointer、key、character、text、timer_id、screen_point、hwnd、popup_host，且与 `UiPointerEvent`/`UiKeyEvent` 字段互相镜像（`focused`、`popup_host` 双份）。每次分发拷贝 `std::wstring`。 | `ui.events.hpp:68-104` |
| F3 | `UiEventResult` 10 个字段 + 应用层 `ImgViewerEventResult` + `ImgViewerHostEffects` 三层结果对象互相 Merge，Merge 规则手写且不一致（`Merge(UiEventResult)` 丢弃 focus/close_popup/wants_ime_position）。 | `ui.events.hpp:118`、`imgviewer.host.internal.hpp:20` |
| F4 | `ApplyElementEffect(UiElementId)` 是框架与应用之间唯一的"控件被激活"通知，本质是 stringly-typed 回调。Retained UI 应让控件持有回调（见 §3 方向 D）。 | `ui.root.hpp:24`、各 SettingsUi 等实现 |
| F5 | 弹窗动作经 `PostMessage(action_message)` 绕一圈窗口消息再回到 DispatchUiAction，而不是直接回调。 | `ui.popup.cc`(ForwardAction)、`ui.window.cc:154` |
| F6 | `ToolButtonPointerEvent` / `ToolButtonKeyEvent` / `SameColor` 在 pen/shape/text toolstrip 中**逐字复制 3–4 份**；其逻辑（按下捕获、抬起命中才发 action）就是 `Button`/`IconButton` 已有的逻辑，控件库里却没有可复用的"自定义绘制按钮"基类。 | `imgviewer.ui.pen_toolstrip.cc:20-74` 等 |
| F7 | `Button::PreferredWidth` 有两个重载（一个吃 DrawContext 一个吃裸 factory），暴露了测量上下文未统一。 | `ui.button.hpp:14-15` |

### 2.3 应用层语义 / 放错位置的东西

| # | 问题 | 位置 | 应去处 |
|---|---|---|---|
| A1 | `EditingTextCaretPoint` + `DocumentPointToViewportPoint`（120 行纯几何/DWrite 测量）住在**消息宿主**里 | `imgviewer.host.cc:134-246` | `imgviewer.edit` 或 `imgviewer.renderer`（viewport 变换矩阵与 renderer 内部的画布变换是同一份知识，现在两处维护） |
| A2 | `ApplyWindowOpacity` / `ImgViewerWindowStyle` / `ApplyImgViewerWindowFrame` / `SyncWindowState`（窗口 chrome）住在 `imgviewer.cc` | `imgviewer.cc:124-189,497-518` | `imgviewer.host.chrome.cc` |
| A3 | `ShowWindowSizeToast` 在 host.cc，但读写 context 的 toast 字段 | `imgviewer.host.cc:536` | 与 A2 一起归 chrome/host |
| A4 | `ImgViewerContext` 是 god struct：color picker 的三个字段、edge-click 的两个字段、toast 去重的两个字段、IME 两个字段、三个 `void*` 子窗口指针全部平铺 | `imgviewer.hpp:31-70` | 各自归入子状态对象（color picker 状态应该整体属于一个 ColorPickerController；见 P4） |
| A5 | `imgviewer.cc` 同时承担：action 分发、文件装载、剪贴板、截图、另存为、info panel 状态构建、动画 timer——1428 行 | `imgviewer.cc` | 拆分（见 P4） |
| A6 | `ImgViewerEditController`：选择/像素选择/画笔/形状/文本（含 IME 状态机）/裁剪/历史/导出 全在一类中，295 行头、2179 行实现，40+ 私有成员裸放 | `imgviewer.edit.hpp` | 按工具拆 session 对象（见 P6，低优先级） |
| A7 | `TryViewerAction` 与 `TryEditAction` 是两个完全相同的函数 | `imgviewer.cc:556-568` | 合一或删除 |
| A8 | `SyncPopupModal` 在 internal.hpp 已声明，host.cc:248 又重复前置声明 | `imgviewer.host.cc:248` | 删除 |
| A9 | `SettingsUi::HitTest` override 带着 `/// XXX: This should be removed later.`——footer 按钮挂在树上却不在布局流内，手工 Arrange + 手工 HitTest 补丁 | `imgviewer.settings.cc:327-340` | 用 DockPanel/footer StackPanel 进布局流 |
| A10 | `ImgViewerUi::Arrange` 中 animation_toolbar 锚点选择是 6 层嵌套三目 | `imgviewer.ui.cc:87-97` | "当前可见 toolstrip" 应是一个查询函数 |
| A11 | 测试缺位：`tests/` 只有图片素材，无任何自动化测试；重构安全网完全依赖手工 + developer window | `tests/` | 见 §4 第 0 步 |

### 2.4 AI 生成样板的典型形态（重构迟滞所在）

7 个 toolstrip（pen/shape/text/selection/color_picker/animation/edit_toolbar）每个都有一套**结构完全平行**的：

```
enum class ButtonKey { ..., Count };          // ~20 行
struct ButtonSpec + kButtonSpecs 表            // ~40 行
struct ButtonInstance { id; element; }         // ~5 行
构造函数循环建按钮                              // ~25 行
SetScalePercent / SetState / Rect / Measure /
Arrange / Render / OnPointerEvent(空!)         // ~60 行
UpdateVisualState 循环                          // ~20 行
+ 本地复制的 SameColor/ToolButton*Event        // ~55 行
```

`ImgViewerFloatingToolbar` 已经被抽出来（重构开始了），但**停在了"共享容器"这一步**，没有继续抽"按钮条 = spec 表 + 活跃判定函数"这层。每个 toolstrip 的 `OnPointerEvent` 都是 `return {};`（死代码），`Measure` 都是同一句 `toolbar_->Measure(N)`。这是典型的"每次让 agent 加一个 toolstrip 就复制上一个"的产物。**七份合计约 1800 行，可压缩到 ~400 行**（一个通用 `ToolStrip` + 七张数据表）。

同样的迟滞出现在：
- 主窗口未迁移到 `UiWindowHost`（host.* 共 ~1500 行，其中 ≥60% 与 ui.window.cc 同义）；
- `UiWindowHost`/`PopupHost` 各自手撸单 surface DComp 代码，而 `UiSurfaceManager` 已存在且更好；
- `OnInputEvent/OnPointerEvent/OnKeyEvent` 三入口并存（统一事件迁移做了一半）。

---

## 3. 重构方向与收益评估（诚实版）

按 **收益/风险比** 排序。"收益"含删码量、复杂度下降、为 retained UI 铺路三项。

### P1（最高收益，先做）：Toolstrip 统一化 —— 纯删除，低风险
- 新建 `imgviewer.ui.toolstrip.{hpp,cc}`：通用 `ImgViewerUiToolStrip`，构造接受 `std::span<const ToolStripItemSpec>`；spec 含 action、name/tooltip string id、automation id、渲染器（枚举：色块/线宽/图标/自定义函数指针）与"活跃判定"。
- 把 `ToolButtonPointerEvent/ToolButtonKeyEvent/SameColor` 上移：前两者直接做成框架层 `ui.button` 的受保护行为或独立 `ui.button_behavior.hpp`；`SameColor` 进 `math.hpp` 或 `ui.theme`。
- 七个 toolstrip 文件退化为各自一张 spec 表 + State 定义。
- **预计净删 1200–1500 行。无行为变化。可被 agent 机械执行。**

### P2（高收益，中风险）：参数化 Action —— 砍掉枚举爆炸
- 给 `UiAction` 增加 payload：`struct UiAction { int verb; int32_t arg; }`（颜色可用 ARGB 编码进 arg，字号/线宽用定点），或应用层定义 `ImgViewerCommand { ImgViewerAction verb; variant<monostate,D2D1_COLOR_F,float,…> arg; }`。
- `EditPenColor*` 8 个 → `EditPenColor(arg)` 1 个；同理线宽、字号、文本色、背景色、形状种类。枚举从 ~120 减到 ~70。
- `ExecuteImgViewerAction` 的 40+ case 合并为 6 个；`IsImgViewerActionEnabled` 的 `||` 链合并；菜单构建、toolstrip spec、keybinding 表共用同一份颜色/尺寸常量表（新建 `imgviewer.palette.hpp`）。
- 注意：keybindings 序列化里 action 以名字/整数存盘的话需要兼容迁移（检查 `imgviewer.keybindings.cc` 的持久化格式后再动）。
- **预计净删 600–900 行；消除 4 处重复维护的颜色表。**

### P3（战略收益最大，中高风险）：主窗口迁入 `UiWindowHost` 形态，统一渲染调度
这是"朝向健全 retained UI"的主干工程，建议拆三步：

1. **渲染调度统一**（先做、独立可交付）：`ApplyHostEffects` 不再同步调 `RenderImgViewer`，改为 `InvalidateRect`；主窗口 WM_PAINT 里渲染（DComp 下仍可在 WM_PAINT 中 BeginPaint/EndPaint + Commit）。删除一事件多次渲染问题，`needs_render` 布尔传染链显著缩短。
2. **宿主能力下沉**：把主窗口独有的能力做成 `UiWindowDelegate` 的扩展点——canvas 层（viewer/edit 的 pointer 分流）、IME 关联开关、动画 timer、拖放。`UiWindowHost` 增加"未被 UI 处理的 pointer 事件转交 delegate"通道。host.pointer/keyboard 的内容迁为 `ImgViewerWindowDelegate` 的实现。
3. **删除**：`imgviewer.host.cc` 的 IME 读取/popup 转发/effects 合并、`imgviewer.host.internal.hpp` 的大部分、`ImgViewerHostEffects`（被 `UiEventResult` + delegate 通道取代）。
- 风险点：主窗口的 borderless chrome（WM_NCCALCSIZE/NCHITTEST）、edge-click、color picker、edit capture 的分流次序必须逐条对照保留。建议迁移时为 `ImgViewerInteractionState` 写出当前分流的真值表（pointer_router 已是纯函数，直接补单测）。
- **预计净删 800–1200 行，消灭双宿主；此后所有窗口行为一致，IME/caret/popup 只有一份。**

### P4（中收益，低风险）：归位与拆分
- A1 的几何函数移入 edit/renderer；A2/A3 chrome 函数移入 host.chrome；A7/A8 直接删。
- `imgviewer.cc` 拆为：`imgviewer.actions.cc`（分发）、`imgviewer.document.cc`（Load/Paste/Screenshot/SaveAs，统一"换图后重置"序列——目前 `LoadImgViewerImageFile`/`HandleImgViewerPasteClipboard`/`LoadImgViewerScreenshotBitmap` 三处复制同一段 8 行重置逻辑）、info panel 状态构建移到 `imgviewer.ui.info_panel` 侧。
- color picker 三字段 + edge click 两字段收进小型 struct。
- **预计净删 200–300 行，主要收益是语义清晰。**

### P5（框架级，为 retained UI 收口）：统一事件与回调
- 元素只留一个 `virtual UiEventResult OnEvent(const UiEvent&)`；`UiEvent` 用 `std::variant<PointerData, KeyData, TextData, TimerData,…>` 或 tagged struct，删除字段镜像。
- `UiController` 改为：单次 hit-test → 沿父链 tunnel/bubble（`UiElement` 需要 parent 指针——加上它，这是 retained 树目前最大的结构缺口）→ 移除 `UiRoot::OnPointerEvent` 优先级链 hook（`ImgViewerUi::OnPointerEvent` 的 9 层 if 直接删，toolstrip 面板作为 root 的子元素按 z 序命中）。
- 控件获得 `std::function<void()>`/`std::function<void(Value)>` 回调；`ApplyElementEffect`、`effect_target`、popup 的 PostMessage 回路全部退役。`UiAction` 仅保留给 keybinding/menu 这类"命令"语义。
- F1：把 range/value 能力改为 `UiElement` 的虚函数（或 `ValueProvider*` 查询接口），删 `UiRoot` 上 8 个虚函数与 `UiController` 的 15 个转发。
- **这是真正"健全 retained UI"的分水岭；建议在 P1–P3 完成、有测试护栏后做。**

### P6（可选/低优先）
- edit controller 按工具拆 session 对象 + `HistoryEntry` 改 variant（现在每条历史背 9 份对象拷贝）。
- 布局缓存（Measure 失效标记）——当前 UI 规模下收益小，retained 化(P5)后顺手做。
- L4 设备丢失恢复：在 `GraphicsDevice` 集中检测 `D2DERR_RECREATE_TARGET`/`DXGI_ERROR_DEVICE_REMOVED`，向上抛"重建一切"信号。
- a11y 枚举缓存（L9）。

### 明确不建议做的
- 不要引第三方 UI 框架重写：现有树 + DComp 分层（`UiSurfaceManager` 的 z-order 层设计）已经对口这个应用的需求（图像层/编辑层/UI overlay 三层）。
- 不要先做布局系统泛化（grid/flex）：当前需求 StackPanel + 手工锚点足够，等 P5 后按需加。

---

## 4. Coding Agent 执行清单

> 约定：每步独立编译、独立提交；行为不变的步骤用 developer window（`--developer`）+ 手工冒烟（打开图片、切换编辑工具、改设置、切语言、borderless 切换、IME 输入文本）验证。**先做第 0 步建立最小护栏。**

### Step 0 — 测试护栏（半天）
1. 在 CMake 中加一个 `imgviewer_tests` 可执行目标（console，可用最小断言宏，不必引框架）。
2. 先覆盖纯函数模块：`imgviewer.host.pointer_router.cc`（全部 owner×edit_active 组合的真值表）、`ui.layout.cc`（PlaceStack 系列）、`imgviewer.keybindings`（ActionForKey 往返）、`imgviewer_edit_geometry`。
3. 可选：`UiController` 不依赖 HWND，可对 SettingsUi 做"合成 pointer 序列 → 断言 draft_ 变化"的无头测试。

### Step 1 — 纯清理（半天）
1. 删 `imgviewer.host.cc:248` 重复声明；合并 `TryViewerAction/TryEditAction`。
2. 提取 `LoadImgViewerImageFile`/Paste/Screenshot 共用的"换图重置"私有函数（`imgviewer.cc`）。
3. `SameColor` 移入 `math.hpp`（命名 `math::NearlyEqual(D2D1_COLOR_F,…)`），删 3 处本地拷贝。
4. `ToolButtonPointerEvent/ToolButtonKeyEvent` 移入新文件 `ui.button_behavior.{hpp,cc}`，删 3 处拷贝。

### Step 2 — P1 Toolstrip 统一（1–2 天）
1. 新建 `imgviewer.ui.toolstrip.{hpp,cc}`：
   - `struct ToolStripItemSpec { ImgViewerAction action; ImgViewerStringId name, tooltip; const wchar_t* automation_id; ItemVisual visual; /* variant: 色块 D2D1_COLOR_F | 线宽 float | 字形 const wchar_t* | PathIcon | 自定义 render fn */ }`
   - `class ImgViewerUiToolStrip`：持有 `ImgViewerFloatingToolbar`，构造建按钮；`SetActivePredicate(std::function<bool(size_t index)>)` 或直接 `SetActiveStates(std::span<const bool>)`；统一 `Measure/Arrange/Render/SetScalePercent`。
   - 通用按钮元素一个：`ToolStripButton : UiElement`，渲染按 visual variant 分支，事件用 Step 1.4 的 behavior。
2. 逐个迁移 pen → shape → text → selection → animation → color_picker → edit_toolbar（后两个有额外控件，允许保留薄子类）。每迁一个删一对旧文件，编译+冒烟。
3. `ImgViewerUi` 中 `SetXxxToolstripState` 七个函数保留签名（对上游零侵入），内部换为对通用对象的调用。
4. 顺手修 A10：在 `ImgViewerUi` 加 `D2D1_RECT_F ActiveToolstripAnchorRect() const`，替换嵌套三目。

### Step 3 — P2 参数化 Action（1–2 天）
1. 先读 `imgviewer.keybindings.cc` 与 `imgviewer.config.cc` 确认 action 持久化格式；若按枚举整数存盘，保留旧枚举值的解析兼容表。
2. 新建 `imgviewer.palette.hpp`：`kEditColors[8]`（名字 string id + D2D1_COLOR_F）、`kPenWidths[4]`、`kTextSizes[5]`、`kTextBackgrounds[6]` 单一事实源。
3. `ImgViewerAction` 收缩：保留 `EditPenColor/EditPenWidth/EditTextSize/EditTextColor/EditTextBackground/EditShapeKind` 六个带参 verb；payload 通过 `UiAction.value` 高位编码或新 `arg` 字段（推荐后者：`struct UiAction { int value; int32_t arg; }`，比较运算同时比较两者）。
4. 重写 `ExecuteImgViewerAction` 对应 case；菜单构建与 toolstrip spec 改为循环生成自 palette。
5. 验证：快捷键绑定 UI、菜单选中态、toolstrip 活跃态三处与旧版逐项对照。

### Step 4 — P3.1 渲染调度统一（1 天）
1. `ApplyHostEffects` 中 `RenderImgViewer(context)` 替换为 `InvalidateRect(hwnd, nullptr, FALSE)`；新增主窗口 WM_PAINT 处理（`imgviewer.host.lifecycle.cc`）调 `RenderImgViewer` + `PositionMainWindowIme`。
2. 排查所有直接调 `RenderImgViewer` 的非初始化点（grep 约 30 处），改 Invalidate；WM_SIZE/WM_DPICHANGED 保留同步渲染（避免 resize 闪烁）。
3. 验证拖拽平移/画笔的流畅性（DComp Commit 频率不应下降；若 WM_PAINT 合并导致拖画延迟，可在捕获期间用 `RedrawWindow(RDW_UPDATENOW)`）。

### Step 5 — P3.2/3.3 主窗口迁入 UiWindowHost（3–5 天，最大单项）
1. 给 `UiWindowDelegate` 增加：`OnPointerUnhandled(UiWindowHost&, const UiPointerEvent&) -> bool`、`OnKeyUnhandled(...)`、`OnChromeMessage(...)`（NCCALCSIZE/NCHITTEST 转交）。
2. 新 `ImgViewerMainDelegate`：吸收 `imgviewer.host.pointer.cc`/`keyboard.cc` 的画布分流逻辑（edge-click、color picker、edit、viewer），保持 `ImgViewerInteractionState` 不变。
3. 主窗口 IME 特例（编辑文本时才启用、按文档坐标定位）通过 delegate 钩子 `CaretScreenPoint()` 提供给 host 的 PositionIme。
4. 迁移完成后删除：`imgviewer.host.cc` 中 ImeCompositionString/DispatchToPopup/ApplyMerged 族/`ImgViewerHostEffects`、`imgviewer.host.internal.hpp` 大半、`HandleImgViewerKeyboardMessage` 与 `HandleImgViewerPointerMessage` 文件整体（内容已成为 delegate 方法）。
5. 此步每完成一个消息类别迁移就提交一次；优先序：keyboard → pointer → lifecycle → chrome。

### Step 6 — P4 归位（1 天）
按 §2.3 A1–A5 执行移动；同时修 L1（modal 改 `std::vector<ImgViewerModalOwner>` 栈语义）与 L2（三个子窗口 context 统一为模板/基类 `OwnedUiWindow<T>`，用 `std::unique_ptr` + 显式 `OnOwnerDestroyed` 通知，删三份握手）。

### Step 7 — P5 统一事件与回调（框架收口，2–4 天，建议独立分支）
1. `UiElement` 加 `parent_` 指针（AddChild 时设置）。
2. 引入 `OnEvent(const UiEvent&)` 默认转发旧三入口 → 逐控件迁移 → 删旧入口。
3. `UiController` 改单轨分发：capture 优先 → hit-test → bubble；删 `UiRoot::OnPointerEvent/OnKeyEvent/OnInputEvent` 钩子与 `ImgViewerUi::OnPointerEvent` if 链、`SettingsUi::HitTest`（A9 同时解决）。
4. 控件回调：`Button::SetOnClick(std::function<void()>)` 等；`ApplyElementEffect`/`effect_target` 退役；`UiRoot` 的 range/value 8 虚函数下沉为元素能力，删 `UiController` 转发层。
5. PopupHost 动作改直接回调（构造时传 `std::function<void(UiAction, UiElementId)>`），删 PostMessage 回路。

### Step 8 —（可选）DComp 收敛
`UiWindowHost::EnsureDCompSurface` 与 `PopupHost::EnsureDCompSurface/EnsureDCompResources` 改用 `UiSurfaceManager`（单层注册即可），删两份手撸 surface 代码（约 250 行）。

---

## 5. 验收口径

- 行数：P1–P4 完成后 `src/` 预期 ≤ 23k 行（当前 27.8k）；P5/P8 后 ≤ 21k。
- 不变量：所有现有交互（含 IME 中文输入、borderless、DPI 切换、a11y/UIA 树、edge-click、动画播放、settings 实时透明度预览）行为不变。
- 结构断言：完成 P3+P5 后，`imgviewer.host.*` 仅剩 chrome + delegate；`grep -c "context == nullptr"` 应从当前 ~80 处降到个位数（context 生命周期由 host 保证）；`UiRoot` 接口 ≤ 8 个虚函数。
