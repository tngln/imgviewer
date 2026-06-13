# ImgViewer 重构评估与执行计划（refactor.md）

> 基于 2026-06-13 全量代码审读（`src/` 现 24,746 行，HEAD = `c08f73d sam altman sima`）。
> 本文是对 `refactor.old.md` 的接续与方向修正。`refactor.old.md` 假设“保留 retained 树、删除绕过它的平行结构”；**本文改变方向**：以已经落地的 `util::Signal` + `ui_decl` + `ui_bind`（Settings 已采用）为种子，推进到一个**信号驱动、声明式、全量重绘**的框架，主动拆掉 retained 派发机制本身。

---

## 实施进度（2026-06-13 执行记录）

已执行并提交（每步独立编译 + `imgviewer_tests` 89 checks 全绿）：

- **Step 0a 测试护栏**：新增 `imgviewer_tests` console 目标 + `tests/imgviewer_tests.cc`，覆盖 `pointer_router` / `ui.layout` / `edit_geometry` / `keybindings` / `Signal`（含重入）/ 按钮点击回调。
- **Step 0b/0c 信号地基**：`util::Signal<T>` 由 bool/int/wstring 三特化合并为**单一泛型模板**（header-only，`util.signal.cc` 清空），并实现**通知重入安全**（快照计数 + tombstone + Compact）。Signal 改为可移动、不可拷贝。
- **Step 4/R1 绘制核心 brush 缓存**：`UiDrawContext` 增加可选共享 brush；`UiDraw::ResolveBrush` 复用一支 brush（`SetColor`）替代每图元 `CreateSolidColorBrush`；`UiRenderer` UI overlay 每帧仅建一支 scratch brush。
- **Step 1 控件回调出口**：`UiElement` 新增 `SetOnClick/HasOnClick/InvokeClick`；四个共享按钮行为函数在激活时优先调回调并抑制 action（与 action-return 共存）。
- **Step 2a/2b 全量重绘 + 删 needs_render**：宿主边界一律 `Invalidate`（§3.4 doctrine）；**`UiEventResult::needs_render` 字段及 116 处使用全部删除**（grep=0），副作用调用就地上提。
- **Step 3（部分）**：删除死代码 `PopupHost::ForwardAction`。（结论：popup 走同步 `SendMessageW` 非异步隧道；a11y/UIA invoke 合理保留 `kImgViewerUiActionMessage`，full 回调化收益低，未做。）
- **Step 5（部分）**：pen/shape/selection 三个**纯转发** toolstrip wrapper 类删除（6 文件），由 `ImgViewerUi` 直接持有 `ImgViewerUiToolStrip` + spec builder（`imgviewer.ui.toolstrip.{hpp,cc}`），active 计算内联。净删约 225 行。
- **缺陷修复（非计划内，重要）**：`experimental/ui.bind.cc` 的 `BindSliderRow` 此前将**按值参数 `format_value` 以引用捕获**进 `apply_value`，再把该 lambda 拷入比函数更长寿的信号订阅 → 悬垂 `std::function` 调用 → 任意 settings slider 改变即 `abort()`。本次会话的内存布局变化把潜伏 UB 暴露为硬崩溃。已修：延迟订阅按值持有 `normalize`/`format_value`。

**Step 2c（三入口事件归一）评估后暂缓**：触及约 12 个控件，每控件事件逻辑不变，仅“三虚函数→一虚函数 + 删 `UiInputEvent`”，churn 大而净删小、回归面广，列为低优先 polish。

**Step 5 剩余部分的诚实评估**：余下四个 wrapper（edit_toolbar 含 dirty-dot 自定义渲染 + tool/undo 状态、text 含字体枚举、color_picker 含取值显示元素、animation 含帧标签）**并非纯样板**，折叠只会把真实逻辑搬进 `ImgViewerUi`，不降复杂度且增回归风险，故止步。删除 `ImgViewerUi` 的 Measure/Arrange/Render 手工 fan-out 与 toolstrip 的**自定义锚定布局**强耦合（纯子树无法表达“居于 edit toolbar 上方居中”），属高风险结构性改造，未做。

**性能备注（用户观察，已推迟到计划后专项）**：复杂交互下 CPU/GPU 偏高，根因是全量重绘 + 每帧重画全部三层 DComp（image/edit/ui_overlay）。按 §3.4，正解是**按层失效**——UI overlay 变化时不应重画昂贵的 image 层。属计划后性能热点专项。

---

## 0. 一句话结论

`refactor.old.md` 的 P1（toolstrip 统一）和 P2（参数化 action）**已经做完**，并且额外诞生了一套干净的信号/声明式框架（`experimental/`），Settings 窗口已是它的样板。**现在最大的杠杆不再是“合并平行结构”，而是“删掉 retained 派发机制”**：三入口事件、`UiInputEvent` 万物结构体、十字段 `UiEventResult` 合并链、`ApplyElementEffect` 字符串回调、action 经窗口消息隧道回流——这些都是为“判断是否重绘 + 把控件激活回传给应用”而存在的。一旦接受“任何输入后无脑全量重绘”，再让控件直接持有回调（信号已经证明可行），**这套机制可以整体删除，预计净删 3500–5000 行，且让框架真正变成信号驱动**。

绘制核心的单点高收益项：`UiDraw` 目前**每个图元都新建一个 D2D brush**（`ui.draw.cc`），在全量重绘模型下这是最热的分配路径，应改为缓存 brush。

---

## 1. 自 refactor.old.md 以来的进展（不要重复做）

| 旧计划项 | 状态 | 证据 |
|---|---|---|
| P1 Toolstrip 统一 | ✅ 完成 | `imgviewer.ui.toolstrip.{hpp,cc}` + `ToolStripItemSpec`；pen/shape/text/selection 等退化为 spec 表 |
| P2 参数化 Action | ✅ 完成 | `ui.action.hpp` 现为 `struct UiAction { int value; int32_t arg; }`；`imgviewer.palette.hpp` + `PackColor/PackFloat`；`EditSetPenColor/EditSetPenWidth` 已是单一带参 verb |
| `SameColor` 上移 | ✅ 完成 | 现为 `math::NearlyEqual` |
| `ToolButton*Event` 上移 | ✅ 完成 | `ui.button_behavior.{hpp,cc}` |
| 信号/声明式框架（旧计划 P5 的精神） | ✅ 已起步 | `experimental/util.signal`、`experimental/ui.bind`、`experimental/ui.decl`，Settings 全量迁移（git: settings refactor 1–5） |
| 渲染调度统一（旧 P3.1） | 🟡 一半 | 主窗口已有 `WM_PAINT` 处理（`imgviewer.host.lifecycle.cc:133`）且部分路径用 `InvalidateRect`；但仍有 27 处直接 `RenderImgViewer`（`imgviewer.cc`），WM_SIZE/DPI/CREATE 仍同步渲染 |

**仍未动的旧债**：双宿主（`imgviewer.host.*` vs `UiWindowHost`）、三入口事件、`UiInputEvent`/`UiEventResult` god struct、单槽 modal（L1）、`void*` 子窗口握手（L2）、设备丢失（L4）、DComp surface 三份实现。

---

## 2. 现状分层（更新版）

```
信号层（新，干净）     experimental/util.signal      —— bool/int/wstring 三个显式特化
声明式构建（新，干净） experimental/ui.decl          —— VStack/HStack/Section/Bind/Owned<T> fluent
双向绑定（新，干净）   experimental/ui.bind          —— Signal <-> 控件，SubscriptionBag 管生命周期
─────────────────────────────────────────────────────────────────────
retained 树           ui.element / ui.panel / 控件库 —— Measure/Arrange/Render/三入口事件
派发机制（待删）       ui.cc(UiController) / ui.events / ui.root
                      —— UiInputEvent + UiEventResult 合并 + ApplyElementEffect + action 隧道
宿主 A（待删/合并）    imgviewer.host.*（8 文件，主窗口手写消息泵 + effects 合并）
宿主 B（保留为主）     ui.window(UiWindowHost) + UiWindowDelegate（Settings/About/Developer）
绘制核心              ui.renderer + ui.surface + ui.graphics_device + ui.draw
                      imgviewer.renderer（image/edit/ui_overlay 三 DComp 层）
基础                  image.* / win32.* / util.* / math   ★ 边界清晰，不动
```

**核心事实**：信号/声明式那三层是“未来”，已经在 Settings 验证。但它们目前只覆盖了 Settings 的**构建**与**有状态控件的双向绑定**；按钮/菜单仍走旧的 action-return 派发，主窗口 UI（`ImgViewerUi`）完全没有迁移。框架的“迟滞”就在这里：**新框架是个岛，旧派发机制是大陆，两者并存导致每个控件同时背着回调和 action-return 两套出口。**

---

## 3. 目标模型（必须先定义清楚，否则会跑偏）

### 3.1 我们要的是什么

**信号驱动 + 声明式构建 + 薄 retained 树 + 全量重绘**。具体定义：

1. **构建是声明式的**：UI 用 `ui_decl` 的 `VStack/HStack/Section/...` 一次性描述出来，不再手写“每个成员 Measure/Arrange/Render 列三遍”。
2. **状态是信号**：所有“会影响显示的应用状态”是 `util::Signal<T>`。控件订阅信号自动更新；应用改信号即可，不再有 `SetXxxState(...)` 推送函数。
3. **事件出口是回调**：控件持有 `std::function`（`OnClick/OnToggled/OnValueChanged`），直接调用。**删除 action 经 `UiEventResult` 返回、经窗口消息隧道回流、经 `ApplyElementEffect` 字符串回调的整条链路**。`UiAction` 仅保留给 keybinding/menu 这类“命令名”语义。
4. **重绘是无脑的**：任何输入分发后一律 `Invalidate()` → 下一个 `WM_PAINT` 全量重画当前 surface。**删除 `UiEventResult::needs_render`、`handled` 的合并簿记**（它们 90% 的存在意义就是决定要不要重绘）。

### 3.2 我们**不要**什么（诚实的边界）

- **不要纯 IMGUI（每帧重建元素树）**。本应用有 DWrite 文本布局、IME 组合态、焦点、表格滚动位置、tooltip 等**必须跨帧保留的状态**；每帧重建树会把这些状态管理推回应用层，得不偿失。`Owned<T>`/`StackPanel` 这套**保留对象、声明式装配**才是正解——retained 的是“对象与布局结果”，immediate 的是“状态推送与重绘”。这正是 Settings 现在的形态，方向是对的。
- **不要引第三方 UI 框架**。三层 DComp 合成（图像/编辑/UI overlay）正好对口本应用，重写无收益。
- **不要先做布局泛化（grid/flex）或脏区域**。全量重绘前提下脏区域是负资产；当前 StackPanel + 锚点够用。

### 3.3 这意味着 retained 树的“瘦身”而非“废弃”

保留：`UiElement`（Measure/Arrange/Render/HitTest/children）、控件库、`ui.panel`、`ui.layout`、DComp 合成。
删除：`OnInputEvent`/`OnPointerEvent`/`OnKeyEvent` 三入口归一、`UiInputEvent`、`UiEventResult` 的大部分字段、`UiRoot::HandleUiAction/ApplyElementEffect/OnPointerEvent` 钩子、`UiController` 的 action 隧道、`ImgViewerHostEffects` 合并、主窗口手写宿主。

### 3.4 重绘成本模型：层 = 重绘单元，DComp 负责合成（核心原则）

这是本次重构的成本模型，必须贯彻到每一个决策：

1. **重绘的单位是 DComp 层（surface），不是元素。** 任何改动 → 该层 `Invalidate` → `WM_PAINT` 全量重画整层。层内**永远不做脏区域/局部失效**——脏区域追踪是负资产，是 `needs_render` 簿记之所以存在的根因，删之。
2. **UI 层（toolbar/toolstrip/titlebar/info panel/popup）总量很小，全量重绘成本可忽略。** 这是删掉 `needs_render` 全链路、让派发末尾无脑 `Invalidate()` 的正当性来源。当前 UI overlay 已经是“每次 `WM_PAINT` clear + 全画”，方向已对。
3. **当某一层全量重绘成本变大时，扩展手段是“拆出新层交给 DComp”，而不是在层内引入脏区域。** 例如：
   - 图像层与编辑标注层已分离（`image_surface_` / `edit_surface_`），各自独立重绘、DComp 合成——这就是正确范式。
   - 若将来某个高频更新的小部件（如取色放大镜、拖拽中的选区、动画帧）拖慢了所在层，就把它**提升为独立 DComp visual/surface**，让它单独高频重绘，其余层保持静止由 DComp 复合。
   - `UiSurfaceManager` 已支持按 z-order 注册任意多层（`RegisterSurface`/`z_order`），这正是该机制的落点——拆层几乎零成本。
4. **推论：`UiEventResult::needs_render` 与一切“是否要重绘”的合并簿记可以整列删除。** 代价模型从“精确计算最小重绘集”变为“全画当前层；太贵就拆层”。后者代码量小一个量级，且与 DComp 的设计天然契合。

> 一句话：**不要优化重绘范围，要优化层的划分。** 范围优化交给 GPU 合成器（DComp），我们只负责把内容按更新频率分到合适的层。

---

## 4. 债务与 pitfall 清单（更新，按主题）

### 4.1 派发机制（最大的删除机会）

| # | 问题 | 位置 | 说明 |
|---|---|---|---|
| D1 | **三入口事件并存** | `ui.element.hpp` `OnInputEvent/OnPointerEvent/OnKeyEvent` | 一次未完成的“统一事件”迁移做了一半。`UiController::DispatchPointerEvent`（`ui.cc:164+`）把 pointer 包成 `UiInputEvent` 再调 `OnInputEvent`，控件却又各自 override `OnPointerEvent`。每个控件被迫实现/转发多个入口。 |
| D2 | **`UiInputEvent` 万物结构体** | `ui.events.hpp:68` | 同时含 pointer/key/character/text/timer/screen_point/hwnd/popup_host，与 `UiPointerEvent`/`UiKeyEvent` 字段互相镜像（`focused`、`popup_host` 双份），每次分发拷贝 `std::wstring text`。 |
| D3 | **`UiEventResult` 十字段合并链** | `ui.events.hpp:118` | `handled/needs_render/capture/focus/focus_target/action/wants_ime_position/value_changed/close_popup/effect_target`。应用层还有 `ImgViewerHostEffects` 三层 Merge（`imgviewer.host.internal.hpp`），规则手写不一致。全量重绘后 `needs_render` 整列可删。 |
| D4 | **`ApplyElementEffect(UiElementId)` 字符串式回调** | `ui.root.hpp:24`、`ui.cc` 多处 | 框架→应用“控件被激活”的唯一通道，本质 stringly-typed。控件持有 `std::function` 后整体退役。 |
| D5 | **action 经 `PostMessage` 隧道回流** | `ui.popup.cc`(ForwardAction)、`ui.window.cc:154`、`imgviewer.messages.hpp` | 弹窗动作绕窗口消息回到 `DispatchUiAction`，依赖全局 `UiElementId` 不冲突的隐式耦合。 |
| D6 | **按钮仍走 action-return，绑定控件走回调（双轨）** | `ui.button.*` 无 `SetOnClick`；`ui.bind.cc` 用 `SetOnToggled` 等 | 同一框架两套事件出口。统一到回调后 D3/D4/D5 才能整条删。 |
| D7 | `FindById` 每事件 O(n) 递归 + 全局静态 ID 生成器 | `ui.cc`、`ui.element.cc:14` | 派发改“hit-test 直接拿元素指针 + 回调”后不再需要 by-id 反查。 |

> 量级参考：`needs_render|OnInputEvent|ApplyElementEffect|UiInputEvent|effect_target|value_changed` 在 25 个文件命中 235 次。这是 AI 生成 boilerplate 与重构迟滞最集中的地方。

### 4.2 信号框架自身的局限（动它之前先补）

| # | 问题 | 位置 |
|---|---|---|
| S1 | `Signal<T>` 只有 `bool/int/wstring` **三个显式特化**，三份几乎逐字重复的实现 | `util.signal.hpp/.cc` —— 应改为**单一泛型模板**（`Signal<T>`，`T` 支持 `==`），三份合一，且能承载 `enum`/`float`/`D2D1_COLOR_F` 等主窗口状态类型 |
| S2 | `NotifyListeners` 遍历 `listeners_` 时若回调内 `Subscribe`/`Unsubscribe` 会使 vector 失效（迭代器/引用悬垂） | `util.signal.cc` —— 通知前快照或用 index + 标记删除；这是迁移主窗口前必须修的并发/重入隐患 |
| S3 | 无“派生/计算信号”（computed） | 主窗口很多状态是派生的（如 `pen_toolstrip.visible = edit.Active() && tool==Pen`）。需要一个轻量 `Computed`/`Map` 或约定“在一个集中函数里 Set 所有派生信号” |

### 4.3 宿主与生命周期

| # | 问题 | 位置 | 说明 |
|---|---|---|---|
| H1 | **双宿主**：主窗口手写消息泵 vs `UiWindowHost` | `imgviewer.host.*`（8 文件）vs `ui.window.cc` | IME 读取、caret timer、popup 转发、capture、a11y、DComp、渲染调度两边各一份。最大单项 LOC。 |
| H2 | `UiWindowDelegate` **缺扩展点** | `ui.window.hpp:21` | 只有 `OnCreate/OnDestroy/OnUiAction/OnUiValueChanged/OnUnhandledMessage`。主窗口需要：画布层 pointer 透传（viewer/edit 分流、edge-click、color picker）、borderless chrome（NCCALCSIZE/NCHITTEST）、按文档坐标定位 IME caret、动画 timer。迁移前必须先补这些 hook。 |
| L1 | **单槽 modal 被多窗口覆写** | `imgviewer.interaction.cc` | 先 Settings 再 About，关 About 把 modal 清空，Settings 仍开着。改集合/栈语义。 |
| L2 | **`void*` 子窗口 context + new/delete + PostMessage 握手** | `imgviewer.hpp`、`imgviewer.settings.cc:527+`、about/developer 同构 | 三份相同握手。改 `OwnedUiWindow<T>` + `unique_ptr` + 显式通知。 |
| L4 | **无设备丢失处理** | 全部 DComp/D3D 路径 | 无 `DXGI_ERROR_DEVICE_REMOVED`/`D2DERR_RECREATE_TARGET` 恢复。全量重绘模型下集中化 `GraphicsDevice` 出口即可补。 |

### 4.4 绘制核心（用户重点关注）

| # | 问题 | 位置 | 说明 |
|---|---|---|---|
| R1 | **每个图元新建 D2D brush** | `ui.draw.cc` `CreateBrush` | `FillRect/DrawRect/DrawText/...` 每次 `CreateSolidColorBrush`。全量重绘下这是最热分配路径。改为：`UiDrawContext` 持有一个可复用的 `ID2D1SolidColorBrush*`，画前 `SetColor`。**单点高收益、低风险。** |
| R2 | **DComp surface 三份实现** | `UiRenderer`(`ui.renderer/surface`)、`UiWindowHost::EnsureDCompSurface`、`PopupHost::EnsureDCompSurface` | `UiSurfaceManager` 已是最好的一份，另两处手撸。统一后约删 250 行。 |
| R3 | `RenderImgViewer` 每次重建 7 个 toolstrip state 并整推 | `imgviewer.cc:97-135` | 这是“immediate 推送”，方向其实对；但推送目标（`SetXxxToolstripState`→存→`SetState`→`SetActiveStates`）是 boilerplate。改信号后这一段连同接收端一起消失。 |
| R4 | 27 处直接 `RenderImgViewer` | `imgviewer.cc` | 应收敛为“改状态→`Invalidate`→`WM_PAINT` 渲染”单路径（resize/dpi 可保留同步以防闪烁）。 |

### 4.5 放错位置 / 杂物（沿用 refactor.old.md，仍有效）

- `imgviewer.cc` 仍是杂物间（action 分发 + 文件装载 + 剪贴板 + 截图 + 另存为 + info panel 构建 + 动画 timer）。
- 几何/DWrite 测量函数住在消息宿主里（`imgviewer.host.cc` 的 caret/viewport 变换）应归 `imgviewer.edit`/`renderer`。
- chrome 函数（`ApplyImgViewerWindowFrame`/`SyncWindowState`）散在 `imgviewer.cc`，应归 `imgviewer.host.chrome`。
- 截屏路径 UI 线程 `Sleep`（`imgviewer.cc`）。

---

## 5. 方向与收益排序（诚实版，按当前状态重排）

> 与 `refactor.old.md` 不同：P1/P2 已完成；信号框架已存在。下面按**“信号驱动落地” × 删码量 × 风险**重排。

### N0（前置，必做，半天–1 天）：测试护栏 + 信号框架补强
- **仍然没有任何自动化测试**（`tests/` 只有图片）。在动派发机制前，给纯函数补最小护栏：`imgviewer.host.pointer_router`（owner×edit 真值表）、`ui.layout`（PlaceStack）、`imgviewer.keybindings`（往返）、`imgviewer.edit_geometry`。CMake 加 `imgviewer_tests` console 目标。
- 修 S1（`Signal<T>` 泛型化，三特化合一）、S2（通知重入安全）、加 S3（最小 `Computed` 或约定）。这是后续所有信号迁移的地基。
- **收益**：解锁后续步骤的安全网；S1 本身净删约 150 行重复特化。

### N1（最高收益）：事件出口统一到回调 + 删 `needs_render` 簿记
这是把“信号框架从岛变成大陆”的关键，且大部分是删除。
1. 给 `Button`/`IconButton`/`ToolStripButton`/`MenuItem` 加 `SetOnClick(std::function<void()>)`（菜单项已有 action，可平滑改回调）。
2. `UiController` 派发改为：capture 优先 → 单次 hit-test 拿**元素指针** → 调元素回调。删 `root_->HandleUiAction`、`ApplyElementEffect`、`DispatchPointerEvent` 里的 action 隧道与 by-id 反查（D4/D5/D7）。
3. **接受全量重绘**：派发函数末尾统一 `Invalidate()`；删 `UiEventResult::needs_render` 及其在 235 处的传染（D3）。`handled` 仅保留给“是否继续冒泡/是否让宿主处理”这一个布尔。
4. `ImgViewerUi::OnPointerEvent` 的 9 层 if 优先级链（`imgviewer.ui.cc:118-153`）删除——toolstrip 面板作为 root 子元素按 z 序命中即可（需要 N2 的树结构）。
- **预计净删 1500–2200 行；消灭双轨事件出口；`UiEventResult` 从 10 字段降到 2–3。**
- 风险：capture/focus 语义必须逐项对照（textbox 拖选、slider 拖动、popup 关闭）。先在 Settings/Developer 上验证回调路径，再推主窗口。

### N2（高收益）：`ImgViewerUi` 迁移到声明式 + 信号
1. `ImgViewerUi` 持有一组 `util::Signal`（edit_active、tool、pen_color、pen_width、shape_kind、text_style、color_picker_*、animation_*、top_most、maximized、info_panel_*）。
2. toolstrip/toolbar/titlebar/info_panel 用 `ui_decl` 声明式构建一棵子树，绑定到这些信号（`ui_bind` 扩展到 toolstrip 的“active 判定”）。
3. **删除 `SetEditToolbarState/SetPenToolstripState/...` 七个推送函数及其接收端 `SetState/SetActiveStates`**；`RenderImgViewer` 的 40 行状态重建（`imgviewer.cc:97-135`）改为对信号 `Set`（派生信号在一个 `SyncSignals()` 里集中算，配合 S3）。
4. **删除 `ImgViewerUi::Measure/Arrange/Render` 里逐成员列三遍的 fan-out**（`imgviewer.ui.cc:30-80`）——改为一棵真正的 root 子树，`UiController::Render` 一次走完。锚点逻辑（`ActiveToolstripAnchorRect` 6 层 if）用布局/可见性表达。
- **预计净删 800–1200 行；主窗口 UI 与 Settings 同构，心智模型统一。**
- 依赖 N1（回调）与 N0（信号泛型/computed）。

### N3（战略收益最大，最高风险）：主窗口迁入 `UiWindowHost`，灭双宿主
1. 先补 `UiWindowDelegate` 扩展点（H2）：`OnPointerUnhandled`（画布分流）、`OnChromeMessage`（NCCALCSIZE/NCHITTEST）、`CaretScreenPoint`（IME 文档坐标）、动画 timer 钩子。`imgviewer.host.pointer_router` 已是纯函数，直接复用。
2. 新 `ImgViewerMainDelegate` 吸收 `imgviewer.host.pointer/keyboard` 的画布分流（edge-click、color picker、edit、viewer），保持 `ImgViewerInteractionState` 真值表不变。
3. 主窗口 `renderer` 是三层 DComp（image/edit/ui_overlay），`UiWindowHost` 目前是单 surface——**让 `UiWindowHost` 也走 `UiSurfaceManager`（R2）**，主窗口注册三层、子窗口注册一层，宿主代码就统一了。
4. 删除：`imgviewer.host.cc` 的 IME/popup 转发/`ApplyMerged` 族、`imgviewer.host.internal.hpp` 大半、`ImgViewerHostEffects`、`HandleImgViewerKeyboardMessage`/`HandleImgViewerPointerMessage`（成为 delegate 方法）。
- **预计净删 800–1200 行；此后所有窗口 IME/caret/popup/渲染只有一份。**
- 风险点：borderless chrome、edge-click、capture 分流次序逐条对照。每迁一个消息类别提交一次（keyboard→pointer→lifecycle→chrome）。

### N4（中收益，低风险）：绘制核心收口
- **R1（brush 缓存）**：立即可做，独立可交付，主窗口拖拽/画笔流畅度直接受益。
- **R2（DComp surface 三合一）**：随 N3.3 一起或独立做。
- **R4（渲染调度单路径）**：27 处 `RenderImgViewer` 收敛为 `Invalidate`（resize/dpi 保留同步）。
- **L4（设备丢失）**：`GraphicsDevice` 集中检测 `D2DERR_RECREATE_TARGET`/`DEVICE_REMOVED`，上抛“重建一切”。

### N5（中收益，低风险）：归位与生命周期
- L1（modal 栈）、L2（`OwnedUiWindow<T>` 删三份握手）、`imgviewer.cc` 拆分（document/actions/chrome）、几何函数归位、截屏 `Sleep` 移出 UI 线程。

### N6（可选）
- `imgviewer.edit` god object 按工具拆 session；`HistoryEntry` 改 variant。
- a11y 枚举缓存（旧 L9，`ui.cc` `ElementMetadataAt` O(n²)）。

### 明确不建议
- 纯 IMGUI 每帧重建树（见 §3.2）。
- 布局系统泛化、脏区域。
- 引第三方 UI 框架。

---

## 6. Coding Agent 执行清单（每步可在独立上下文执行）

> 约定：每步独立编译、独立提交、行为不变的步骤用 developer window + 手工冒烟（开图、切编辑工具、改设置、切语言、borderless 切换、IME 中文输入、动画播放、a11y/UIA 树）验证。**先做 Step 0。**
> 每步开头都给“自包含上下文”，便于 fresh agent 直接执行。

### Step 0 — 护栏 + 信号地基（N0）
- 上下文：项目无自动化测试；信号在 `experimental/util.signal.{hpp,cc}`。
- 动作：
  1. CMake 加 `imgviewer_tests`（console，最小断言宏）。覆盖 `imgviewer.host.pointer_router`、`ui.layout`、`imgviewer.keybindings`、`imgviewer.edit_geometry`。
  2. `Signal<T>` 改单一泛型模板（要求 `T` 可 `==`、可拷贝/移动），删除 bool/int/wstring 三特化；保留 `Get/Set/Subscribe/Unsubscribe` 接口与“值相等则不通知”语义。
  3. `Set`/通知重入安全：通知前对 listener 列表做快照，或遍历期间禁止结构性修改（pending add/remove 队列）。
  4. 加最小 `Computed<T>`（或在 §Step 5 用集中 `SyncSignals()` 替代，二选一，文档注明选择）。
- 验收：`imgviewer_tests` 全绿；Settings 行为不变。

### Step 1 — 控件回调出口（N1.1）
- 上下文：`Button`/`IconButton`（`ui.button.*`）目前只返回 `UiEventResult{.action=...}`；`Checkbox/RadioButton/Slider/Dropdown` 已有 `SetOnXxx` 回调（见 `ui.bind.cc`）。
- 动作：给 `Button`/`IconButton`/`ToolStripButton`/菜单项加 `SetOnClick(std::function<void()>)`，在命中抬起时调用；暂时**与 action-return 并存**（不删旧路径），先让 Settings footer 按钮改用回调验证。
- 验收：Settings 保存/取消/重置按钮经回调工作；编译通过。

### Step 2 — 派发归一 + 删 needs_render（N1.2–N1.4）
- 上下文：`UiController`（`ui.cc`）三入口 + `UiInputEvent`（`ui.events.hpp`）+ `UiEventResult` 10 字段 + `ApplyElementEffect`（`ui.root.hpp`）。
- 动作（建议拆多次提交）：
  1. `UiElement` 三入口归一为 `OnEvent(const UiEvent&)`（`UiEvent` 用 tagged union/`variant`，去掉 `UiInputEvent` 字段镜像与 `wstring` 拷贝）；旧入口暂留默认转发，逐控件迁移后删。
  2. `UiController` 改“capture→hit-test 拿指针→回调”；删 `HandleUiAction`/`ApplyElementEffect`/by-id 反查。
  3. 派发末尾统一 `Invalidate()`；删 `UiEventResult::needs_render` 全链路。
  4. 删 `ImgViewerUi::OnPointerEvent` 9 层 if（依赖 Step 5 的子树，可先留桩）。
- 验收：Settings/Developer 全交互回归（拖选、滑块、下拉、popup 关闭、ESC）；UIA 树不变。

### Step 3 — 弹窗回调，删消息隧道（N1，D5）
- 上下文：`ui.popup.cc` ForwardAction + `ui.window.cc:154` PostMessage 回流 + `imgviewer.messages.hpp`。
- 动作：`PopupHost` 构造接 `std::function<void(UiAction)>`（或直接回调），删 `kImgViewerUiActionMessage` 隧道与 effect_target 跨窗口整数传递。
- 验收：主菜单/右键菜单项执行正确，无消息号依赖。

### Step 4 — 绘制核心：brush 缓存（N4/R1，独立可交付）
- 上下文：`ui.draw.cc` 每图元 `CreateSolidColorBrush`。
- 动作：`UiDrawContext` 增一个可复用 `ID2D1SolidColorBrush*`（由 renderer 在 BeginDraw 时创建/持有）；`UiDraw` 各方法改 `brush->SetColor(color)` 复用。
- 验收：视觉不变；拖拽/画笔无明显卡顿（可用 developer window 帧计时）。

### Step 5 — 主窗口 UI 声明式 + 信号（N2）
- 上下文：`ImgViewerUi`（`imgviewer.ui.cc`）10 个成员手工 Measure/Arrange/Render；`RenderImgViewer`（`imgviewer.cc:97-135`）推 7 个 state。
- 动作：
  1. `ImgViewerUi` 持有信号集合；用 `ui_decl` 声明 root 子树并 `ui_bind` 到信号。
  2. 删 `SetEditToolbarState/SetPenToolstripState/...` 七推送函数 + 接收端 `SetState/SetActiveStates`。
  3. `RenderImgViewer` 状态段改 `SyncSignals()`（集中 `Set`，派生值在此算）。
  4. 删 `Measure/Arrange/Render` 的 fan-out；`ActiveToolstripAnchorRect` 6 层 if 用可见性/布局表达。
- 验收：所有 toolstrip 显隐、active 高亮、缩放百分比、动画/info panel 行为不变。

### Step 6 — `UiWindowDelegate` 扩展点（N3.1）
- 上下文：`ui.window.hpp:21` 的 delegate 太薄；`imgviewer.host.pointer_router` 已是纯函数。
- 动作：加 `OnPointerUnhandled`、`OnChromeMessage`、`CaretScreenPoint`、timer 钩子；`UiWindowHost` 在 UI 未处理 pointer/未命中 chrome 时转交 delegate。
- 验收：Settings 等现有 delegate 默认实现行为不变。

### Step 7 — 主窗口迁入 UiWindowHost（N3.2–3.4，最大单项，独立分支）
- 上下文：`imgviewer.host.*` 8 文件手写宿主；`UiWindowHost` 单 surface，`UiSurfaceManager` 支持多层。
- 动作：
  1. `UiWindowHost` 改用 `UiSurfaceManager`（R2）；主窗口注册 image/edit/ui_overlay 三层。
  2. 新 `ImgViewerMainDelegate` 吸收 pointer/keyboard 画布分流；IME 走 `CaretScreenPoint`。
  3. 逐消息类别迁移并提交（keyboard→pointer→lifecycle→chrome），每类删对应 `imgviewer.host.*`。
  4. 删 `ImgViewerHostEffects`、`imgviewer.host.internal.hpp` 大半、`ApplyMerged` 族。
- 验收：borderless、edge-click、color picker、edit capture、IME、DPI、动画逐项对照旧版。

### Step 8 — 归位与生命周期（N5）
- L1 modal 改栈；L2 `OwnedUiWindow<T>` 删三份握手；`imgviewer.cc` 拆 document/actions/chrome；几何函数归 edit/renderer；截屏 `Sleep` 移出 UI 线程。

### Step 9 —（可选）设备丢失 + edit 拆分（N4/L4、N6）
- `GraphicsDevice` 集中 HRESULT 出口，重建一切；`imgviewer.edit` 按工具拆 session。

---

## 7. 验收口径

- **行数**：N0–N3 完成后 `src/` 预期 ≤ 20k 行（当前 24.7k）；含 N5 后 ≤ 18k。
- **结构断言**：
  - `UiEventResult` 字段 ≤ 3；`UiInputEvent` 删除。
  - `grep -r "ApplyElementEffect\|needs_render\|HandleUiAction"` → 0。
  - `imgviewer.host.*` 仅剩 chrome + delegate；双宿主消失，IME/caret/popup/DComp 各一份。
  - `ui.draw.cc` 无 per-call `CreateSolidColorBrush`。
  - 主窗口 UI 与 Settings 用同一套 `ui_decl`/`Signal` 构建。
- **不变量**：所有现有交互（IME 中文输入、borderless、DPI 切换、UIA 树、edge-click、动画、settings 实时透明度预览、像素选择/裁切/标注）行为不变。
- **方向断言**：应用状态以 `Signal` 为单一事实源；控件输出以回调为单一出口；任何输入后全量重绘，无 `needs_render` 簿记。
- **成本模型断言**：重绘单位是 DComp 层而非元素；层内无脏区域/局部失效；扩展性靠“拆层交给 DComp”（`UiSurfaceManager` 注册新 z-order 层），不靠层内重绘范围优化。

---

## 8. 给执行者的关键提醒

1. **顺序有依赖**：N0（信号地基/护栏）→ N1（回调+删 needs_render）→ N2（主 UI 信号化）→ N3（灭双宿主）。N4/R1（brush）和 N5 可随时并行插入，风险低。
2. **不要在没有 N0 护栏时动 N1/N3**：派发与宿主是行为最敏感的区域，pointer_router/keybindings 的单测是回归底线。
3. **回调迁移期允许双轨并存**：先加回调、在 Settings 验证、再删 action-return，避免一次性大爆炸。
4. **全量重绘是本次重构的“许可证”**：每当纠结“要不要标记重绘/合并 needs_render”，答案是“直接 `Invalidate`”。这是删码量的主要来源，务必贯彻。
5. **retained 树不是敌人**：要删的是它**外面**的派发/合并/隧道机制，不是 `UiElement`/布局/控件本身。Settings 已经是目标形态，照着推广即可。
