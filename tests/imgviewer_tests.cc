// Minimal, dependency-light regression harness for ImgViewer's pure modules.
//
// Built as the `imgviewer_tests` console target (see CMakeLists.txt). This is
// the safety net for the signal-driven/declarative refactor: it pins the
// behaviour of the pure helpers that the dispatch/host migration relies on.

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "experimental/util.signal.hpp"
#include "imgviewer.edit_geometry.hpp"
#include "imgviewer.host.pointer_router.hpp"
#include "imgviewer.interaction.hpp"
#include "imgviewer.keybindings.hpp"
#include "script.quickjs_runtime.hpp"
#include "ui.button_behavior.hpp"
#include "ui.element.hpp"
#include "ui.layout.hpp"

namespace {

int g_checks = 0;
int g_failures = 0;

void CheckImpl(bool condition, const char* expr, const char* file, int line)
{
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::printf("FAIL %s:%d: %s\n", file, line, expr);
    }
}

#define CHECK(cond) CheckImpl((cond), #cond, __FILE__, __LINE__)

bool NearF(float a, float b, float eps = 0.01f)
{
    return std::fabs(a - b) < eps;
}

// ---------------------------------------------------------------------------
// pointer_router
// ---------------------------------------------------------------------------

void TestPointerRouter()
{
    ImgViewerInteractionState state;
    // Default: viewing, canvas owner = Viewer, no capture.
    CHECK(CanUiReceivePointer(state));

    state.BeginPointerCapture(ImgViewerPointerCaptureOwner::Ui);
    CHECK(CanUiReceivePointer(state));
    state.BeginPointerCapture(ImgViewerPointerCaptureOwner::ViewerPan);
    CHECK(!CanUiReceivePointer(state));
    state.ClearPointerCapture();

    CHECK(IsViewerPointerCapture(ImgViewerPointerCaptureOwner::ViewerPan));
    CHECK(IsViewerPointerCapture(ImgViewerPointerCaptureOwner::ViewerRotate));
    CHECK(!IsViewerPointerCapture(ImgViewerPointerCaptureOwner::EditStroke));

    CHECK(IsEditPointerCapture(ImgViewerPointerCaptureOwner::EditStroke));
    CHECK(IsEditPointerCapture(ImgViewerPointerCaptureOwner::EditCrop));
    CHECK(IsEditPointerCapture(ImgViewerPointerCaptureOwner::EditPixelSelection));
    CHECK(!IsEditPointerCapture(ImgViewerPointerCaptureOwner::ViewerPan));

    CHECK(CapturedPointerTarget(ImgViewerPointerCaptureOwner::Ui) == ImgViewerPointerTarget::Ui);
    CHECK(CapturedPointerTarget(ImgViewerPointerCaptureOwner::ColorPicker) == ImgViewerPointerTarget::ColorPicker);
    CHECK(CapturedPointerTarget(ImgViewerPointerCaptureOwner::EdgeClickNavigation) == ImgViewerPointerTarget::EdgeClickNavigation);
    CHECK(CapturedPointerTarget(ImgViewerPointerCaptureOwner::EditCrop) == ImgViewerPointerTarget::EditTool);
    CHECK(CapturedPointerTarget(ImgViewerPointerCaptureOwner::ViewerPan) == ImgViewerPointerTarget::Viewer);
    CHECK(CapturedPointerTarget(ImgViewerPointerCaptureOwner::None) == ImgViewerPointerTarget::None);

    // Canvas target depends on canvas owner and edit_active.
    ImgViewerInteractionState canvas;
    canvas.SetCanvasOwner(ImgViewerCanvasOwner::ColorPicker);
    CHECK(CanvasPointerTarget(canvas, false) == ImgViewerPointerTarget::ColorPicker);
    canvas.SetCanvasOwner(ImgViewerCanvasOwner::EditTool);
    CHECK(CanvasPointerTarget(canvas, true) == ImgViewerPointerTarget::EditTool);
    CHECK(CanvasPointerTarget(canvas, false) == ImgViewerPointerTarget::None);
    canvas.SetCanvasOwner(ImgViewerCanvasOwner::Viewer);
    CHECK(CanvasPointerTarget(canvas, true) == ImgViewerPointerTarget::Viewer);
    canvas.SetCanvasOwner(ImgViewerCanvasOwner::None);
    CHECK(CanvasPointerTarget(canvas, true) == ImgViewerPointerTarget::None);

    // Active target: a non-UI capture overrides canvas; a UI capture defers to canvas.
    ImgViewerInteractionState active;
    active.SetCanvasOwner(ImgViewerCanvasOwner::Viewer);
    active.BeginPointerCapture(ImgViewerPointerCaptureOwner::EditStroke);
    CHECK(ActivePointerTarget(active, true) == ImgViewerPointerTarget::EditTool);
    active.ClearPointerCapture();
    active.BeginPointerCapture(ImgViewerPointerCaptureOwner::Ui);
    CHECK(ActivePointerTarget(active, true) == ImgViewerPointerTarget::Viewer);
}

// Multiple modal owners can coexist (e.g. Settings open + main-window popup);
// clearing one must not drop the others (refactor.md L1).
void TestModalStack()
{
    ImgViewerInteractionState state;
    CHECK(!state.HasModal());

    state.SetModal(ImgViewerModalOwner::Settings);
    CHECK(state.HasModal());
    CHECK(state.IsModal(ImgViewerModalOwner::Settings));

    state.SetModal(ImgViewerModalOwner::Popup);
    CHECK(state.IsModal(ImgViewerModalOwner::Settings));
    CHECK(state.IsModal(ImgViewerModalOwner::Popup));

    state.ClearModal(ImgViewerModalOwner::Popup);
    CHECK(state.HasModal());
    CHECK(state.IsModal(ImgViewerModalOwner::Settings));
    CHECK(!state.IsModal(ImgViewerModalOwner::Popup));

    state.ClearModal(ImgViewerModalOwner::Settings);
    CHECK(!state.HasModal());
}

// ---------------------------------------------------------------------------
// ui.layout
// ---------------------------------------------------------------------------

void TestLayout()
{
    using namespace ui_layout;

    const auto vertical = PlaceVerticalStack(D2D1::RectF(0, 0, 100, 100), {20.0f, 30.0f}, 10.0f);
    CHECK(vertical.size() == 2);
    CHECK(NearF(vertical[0].top, 0.0f) && NearF(vertical[0].bottom, 20.0f));
    CHECK(NearF(vertical[0].left, 0.0f) && NearF(vertical[0].right, 100.0f));
    CHECK(NearF(vertical[1].top, 30.0f) && NearF(vertical[1].bottom, 60.0f));

    const auto horizontal = PlaceHorizontalStack(D2D1::RectF(0, 0, 100, 50), {10.0f, 40.0f}, 5.0f);
    CHECK(horizontal.size() == 2);
    CHECK(NearF(horizontal[0].left, 0.0f) && NearF(horizontal[0].right, 10.0f));
    CHECK(NearF(horizontal[1].left, 15.0f) && NearF(horizontal[1].right, 55.0f));

    const auto row = PlaceBottomRightRow(D2D1::RectF(0, 0, 100, 100), {20.0f, 30.0f}, 10.0f, 5.0f, 5.0f, 5.0f);
    CHECK(row.size() == 2);
    // Rightmost item hugs container.right - right_padding.
    CHECK(NearF(row[1].right, 95.0f));
    CHECK(NearF(row[1].left, 65.0f));
    CHECK(NearF(row[0].right, 60.0f));
    CHECK(NearF(row[0].left, 40.0f));
    CHECK(NearF(row[0].bottom, 95.0f) && NearF(row[0].top, 85.0f));

    const D2D1_RECT_F below = Below(D2D1::RectF(10, 10, 60, 30), 5.0f, 40.0f, 20.0f);
    CHECK(NearF(below.top, 35.0f) && NearF(below.bottom, 55.0f));
    CHECK(NearF(below.left, 10.0f) && NearF(below.right, 50.0f));
}

// ---------------------------------------------------------------------------
// edit_geometry
// ---------------------------------------------------------------------------

void TestEditGeometry()
{
    using namespace imgviewer_edit_geometry;

    CHECK(NormalizeEditRotation(-1) == 3);
    CHECK(NormalizeEditRotation(4) == 0);
    CHECK(NormalizeEditRotation(7) == 3);

    const D2D1_SIZE_U size = D2D1::SizeU(40, 30);
    CHECK(EditPreviewSize(size, 0).width == 40 && EditPreviewSize(size, 0).height == 30);
    CHECK(EditPreviewSize(size, 1).width == 30 && EditPreviewSize(size, 1).height == 40);
    CHECK(EditPreviewSize(size, 2).width == 40 && EditPreviewSize(size, 2).height == 30);

    // Round-trip: source -> preview -> source for every rotation quadrant.
    for (int rotation = 0; rotation < 4; ++rotation) {
        const D2D1_POINT_2F source = D2D1::Point2F(12.0f, 7.0f);
        const D2D1_POINT_2F preview = SourcePointToEditPreviewPoint(source, size, rotation);
        const D2D1_POINT_2F back = EditPreviewPointToSourcePoint(preview, size, rotation);
        CHECK(NearF(back.x, source.x) && NearF(back.y, source.y));
    }
}

// ---------------------------------------------------------------------------
// keybindings
// ---------------------------------------------------------------------------

void TestKeybindings()
{
    const ActionBindings defaults = DefaultActionBindings();
    CHECK(!defaults.key_bindings.empty());

    // Every default gesture resolves to a non-None action.
    for (const KeyBinding& binding : defaults.key_bindings) {
        const ImgViewerAction action = ActionForKey(
            defaults,
            binding.gesture.virtual_key,
            binding.gesture.ctrl,
            binding.gesture.shift,
            binding.gesture.alt);
        CHECK(action != ImgViewerAction::None);
    }

    // An obviously-unbound gesture resolves to None.
    const ImgViewerAction unbound = ActionForKey(defaults, VK_F24, true, true, true);
    CHECK(unbound == ImgViewerAction::None);
}

// ---------------------------------------------------------------------------
// util::Signal
// ---------------------------------------------------------------------------

void TestSignal()
{
    util::Signal<int> value(1);
    CHECK(value.Get() == 1);

    int observed = 0;
    int notifications = 0;
    const size_t id = value.Subscribe([&](int v) {
        observed = v;
        ++notifications;
    });

    CHECK(value.Set(2));            // changed
    CHECK(value.Get() == 2);
    CHECK(observed == 2);
    CHECK(notifications == 1);

    CHECK(!value.Set(2));           // unchanged -> no notify
    CHECK(notifications == 1);

    value.Unsubscribe(id);
    CHECK(value.Set(3));
    CHECK(notifications == 1);      // listener removed

    // bool and wstring instantiations still work.
    util::Signal<bool> flag(false);
    bool flag_seen = false;
    flag.Subscribe([&](bool v) { flag_seen = v; });
    CHECK(flag.Set(true));
    CHECK(flag_seen);

    util::Signal<std::wstring> text(L"a");
    std::wstring text_seen;
    text.Subscribe([&](const std::wstring& v) { text_seen = v; });
    CHECK(text.Set(L"b"));
    CHECK(text_seen == L"b");
}

// Re-entrancy: subscribing/unsubscribing from inside a notification must not
// corrupt the listener list. Pins the fix for the signal notify-during-iterate
// hazard (refactor.md S2).
[[maybe_unused]] void TestSignalReentrancy()
{
    util::Signal<int> value(0);

    int outer = 0;
    int inner = 0;
    value.Subscribe([&](int v) {
        outer = v;
        // Add a new subscriber during notification.
        value.Subscribe([&](int nv) { inner = nv; });
    });

    CHECK(value.Set(1));   // must not crash; outer sees 1
    CHECK(outer == 1);
    CHECK(value.Set(2));   // the inner subscriber added during the first notify now fires
    CHECK(outer == 2);
    CHECK(inner == 2);
}

// Step 1: a control with an on-click callback fires it and suppresses the
// UiAction; without a callback it still returns the action (coexistence).
void TestButtonClickCallback()
{
    const UiAction action{42};

    // With callback: invokes it, suppresses action.
    UiElement with_callback(UiMetadata(UiElementRole::Button, action, L"with"));
    int clicks = 0;
    with_callback.SetOnClick([&]() { ++clicks; });

    const UiPointerEvent up{
        .type = UiEventType::PointerUp,
        .button = UiPointerButton::Left,
        .target = with_callback.Id(),
        .captured = with_callback.Id(),
    };
    const UiEventResult result = ToolButtonPointerEvent(with_callback, up);
    CHECK(clicks == 1);
    CHECK(result.handled);
    CHECK(result.action == kUiActionNone);
    CHECK(result.capture == UiCaptureRequest::Release);

    // Without callback: still returns the action.
    UiElement without_callback(UiMetadata(UiElementRole::Button, action, L"without"));
    const UiPointerEvent up2{
        .type = UiEventType::PointerUp,
        .button = UiPointerButton::Left,
        .target = without_callback.Id(),
        .captured = without_callback.Id(),
    };
    const UiEventResult result2 = ToolButtonPointerEvent(without_callback, up2);
    CHECK(result2.handled);
    CHECK(result2.action == action);

    // Keyboard activation routes through the callback too.
    int key_clicks = 0;
    with_callback.SetOnClick([&]() { ++key_clicks; });
    const UiKeyEvent key{.type = UiEventType::KeyDown, .virtual_key = VK_RETURN};
    const UiEventResult key_result = ToolButtonKeyEvent(with_callback, key);
    CHECK(key_clicks == 1);
    CHECK(key_result.action == kUiActionNone);
}

// ---------------------------------------------------------------------------
// QuickJS runtime
// ---------------------------------------------------------------------------

void TestQuickJsRuntime()
{
    script::QuickJsRuntime runtime;
    CHECK(runtime.Initialize());
    CHECK(runtime.IsInitialized());

    const script::QuickJsEvalResult value = runtime.EvalScript("40 + 2", "quickjs-value-test.js");
    CHECK(value.ok);
    CHECK(value.value_utf8 == "42");

    const script::QuickJsEvalResult promise = runtime.EvalScript(
        "globalThis.__imgviewerQuickJsTest = 0;"
        "Promise.resolve().then(() => { globalThis.__imgviewerQuickJsTest = 7; });",
        "quickjs-promise-test.js");
    CHECK(promise.ok);
    CHECK(runtime.PumpJobs() == 1);
    const script::QuickJsEvalResult pumped = runtime.EvalScript(
        "globalThis.__imgviewerQuickJsTest",
        "quickjs-promise-check.js");
    CHECK(pumped.ok);
    CHECK(pumped.value_utf8 == "7");

    const script::QuickJsEvalResult thrown = runtime.EvalScript(
        "throw new Error('quickjs smoke failure');",
        "quickjs-error-test.js");
    CHECK(!thrown.ok);
    const std::string exception = runtime.TakeExceptionTextUtf8();
    CHECK(exception.find("quickjs smoke failure") != std::string::npos);
}

} // namespace

int main()
{
    TestPointerRouter();
    TestModalStack();
    TestLayout();
    TestEditGeometry();
    TestKeybindings();
    TestSignal();
    TestSignalReentrancy();
    TestButtonClickCallback();
    TestQuickJsRuntime();

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
