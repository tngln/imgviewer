// Minimal, dependency-light regression harness for ImgViewer's pure modules.
//
// Built as the `imgviewer_tests` console target (see CMakeLists.txt). This is
// the safety net for the signal-driven/declarative refactor: it pins the
// behaviour of the pure helpers that the dispatch/host migration relies on.

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <quickjs.h>

#include "experimental/util.signal.hpp"
#include "imgviewer.edit_geometry.hpp"
#include "imgviewer.config.hpp"
#include "imgviewer.host.pointer_router.hpp"
#include "imgviewer.interaction.hpp"
#include "imgviewer.keybindings.hpp"
#include "script.canvas_color.hpp"
#include "script.quickjs_helper.hpp"
#include "script.quickjs_runtime.hpp"
#include "imgviewer.script_ui.hpp"

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

JSValue TestNativeAdd(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
{
    int32_t left = 0;
    int32_t right = 0;
    if (argc > 0) {
        JS_ToInt32(context, &left, argv[0]);
    }
    if (argc > 1) {
        JS_ToInt32(context, &right, argv[1]);
    }
    return JS_NewInt32(context, left + right);
}

struct TestSignalApi final {
    util::Signal<int> value{0};
    JSValue callback = JS_UNDEFINED;
    size_t subscription = 0;
};

TestSignalApi* TestSignalApiFromContext(JSContext* context)
{
    return static_cast<TestSignalApi*>(JS_GetContextOpaque(context));
}

std::string TestSignalName(JSContext* context, JSValueConst value)
{
    return script::ToStringUtf8(context, value);
}

JSValue TestSignalsGet(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
{
    TestSignalApi* api = TestSignalApiFromContext(context);
    if (api == nullptr || argc < 1 || TestSignalName(context, argv[0]) != "test.counter") {
        return JS_UNDEFINED;
    }
    return JS_NewInt32(context, api->value.Get());
}

JSValue TestSignalsSet(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
{
    TestSignalApi* api = TestSignalApiFromContext(context);
    if (api == nullptr || argc < 2 || TestSignalName(context, argv[0]) != "test.counter") {
        return JS_FALSE;
    }
    int32_t next = 0;
    JS_ToInt32(context, &next, argv[1]);
    return JS_NewBool(context, api->value.Set(next));
}

JSValue TestSignalsSubscribe(JSContext* context, JSValueConst, int argc, JSValueConst* argv)
{
    TestSignalApi* api = TestSignalApiFromContext(context);
    if (api == nullptr || argc < 2 || TestSignalName(context, argv[0]) != "test.counter" ||
        !JS_IsFunction(context, argv[1])) {
        return JS_NewInt32(context, 0);
    }
    if (api->subscription != 0) {
        api->value.Unsubscribe(api->subscription);
        JS_FreeValue(context, api->callback);
    }
    api->callback = JS_DupValue(context, argv[1]);
    api->subscription = api->value.Subscribe([context, api](int value) {
        JSValue arg = JS_NewInt32(context, value);
        JSValue result = JS_Call(context, api->callback, JS_UNDEFINED, 1, &arg);
        JS_FreeValue(context, arg);
        JS_FreeValue(context, result);
    });
    return JS_NewInt32(context, 1);
}

JSValue TestSignalsUnsubscribe(JSContext* context, JSValueConst, int argc, JSValueConst*)
{
    TestSignalApi* api = TestSignalApiFromContext(context);
    if (api == nullptr || argc < 1 || api->subscription == 0) {
        return JS_FALSE;
    }
    api->value.Unsubscribe(api->subscription);
    api->subscription = 0;
    JS_FreeValue(context, api->callback);
    api->callback = JS_UNDEFINED;
    return JS_TRUE;
}

void InstallTestSignals(JSContext* context, TestSignalApi* api)
{
    JS_SetContextOpaque(context, api);
    JSValue global = JS_GetGlobalObject(context);
    JSValue signals = JS_NewObject(context);
    JS_SetPropertyStr(context, signals, "get", JS_NewCFunction(context, TestSignalsGet, "get", 1));
    JS_SetPropertyStr(context, signals, "set", JS_NewCFunction(context, TestSignalsSet, "set", 2));
    JS_SetPropertyStr(context, signals, "subscribe", JS_NewCFunction(context, TestSignalsSubscribe, "subscribe", 2));
    JS_SetPropertyStr(context, signals, "unsubscribe", JS_NewCFunction(context, TestSignalsUnsubscribe, "unsubscribe", 1));
    JS_SetPropertyStr(context, global, "signals", signals);
    JS_FreeValue(context, global);
}

std::filesystem::path CurrentExeDirectory()
{
    std::vector<wchar_t> buffer(MAX_PATH);
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0) {
        return {};
    }
    return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
}

std::filesystem::path TestConfigPath()
{
    return CurrentExeDirectory() / L"imgviewer.config.js";
}

void WriteTestConfig(std::string_view source)
{
    std::ofstream output(TestConfigPath(), std::ios::binary | std::ios::trunc);
    output << source;
}

void DeleteTestConfig()
{
    std::error_code ignored;
    std::filesystem::remove(TestConfigPath(), ignored);
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

void TestConfigJsLoad()
{
    DeleteTestConfig();
    WriteTestConfig(
        "globalThis.imgviewerConfig = {"
        "  language: 'zh-CN',"
        "  initialImageView: 'actualSize',"
        "  rememberWindowSize: false,"
        "  pixelatedSampling: true,"
        "  checkerboardBackground: true,"
        "  borderlessWindow: true,"
        "  edgeClickNavigation: true,"
        "  windowOpacity: 55,"
        "  toolbarScale: 160,"
        "  edgeClickNavigationZone: 40,"
        "  window: { width: 222, height: 333 },"
        "  keyBindings: { nextImage: ['Ctrl+N'], previousImage: [] }"
        "};");

    script::QuickJsRuntime runtime;
    CHECK(runtime.Initialize());
    ImgViewerConfig config;
    CHECK(SUCCEEDED(LoadImgViewerConfig(runtime, &config)));
    CHECK(config.language == "zh-CN");
    CHECK(config.initial_image_view_mode == InitialImageViewMode::ActualSize);
    CHECK(!config.remember_window_size);
    CHECK(config.pixelated_sampling);
    CHECK(config.checkerboard_background);
    CHECK(config.borderless_window);
    CHECK(config.edge_click_navigation);
    CHECK(config.window_opacity_percent == 55);
    CHECK(config.toolbar_scale_percent == 160);
    CHECK(config.edge_click_navigation_zone_percent == 40);
    CHECK(config.window_size.width == 222);
    CHECK(config.window_size.height == 333);
    CHECK(ActionForKey(config.action_bindings, 'N', true, false, false) == ImgViewerAction::NextImage);
    CHECK(ActionForKey(config.action_bindings, VK_LEFT, false, false, false) == ImgViewerAction::None);
    DeleteTestConfig();
}

void TestConfigJsFallbacks()
{
    DeleteTestConfig();
    WriteTestConfig(
        "globalThis.imgviewerConfig = {"
        "  language: 'fr-FR',"
        "  rememberWindowSize: 'no',"
        "  windowOpacity: '10',"
        "  toolbarScale: 999,"
        "  edgeClickNavigationZone: 1,"
        "  window: { width: 1, height: 'bad' }"
        "};");

    script::QuickJsRuntime runtime;
    CHECK(runtime.Initialize());
    ImgViewerConfig config;
    CHECK(SUCCEEDED(LoadImgViewerConfig(runtime, &config)));
    CHECK(config.language == "en-US");
    CHECK(config.remember_window_size);
    CHECK(config.window_opacity_percent == 100);
    CHECK(config.toolbar_scale_percent == 160);
    CHECK(config.edge_click_navigation_zone_percent == 5);
    CHECK(config.window_size.width == 160);
    CHECK(config.window_size.height == 640);

    WriteTestConfig("throw new Error('broken config');");
    CHECK(SUCCEEDED(LoadImgViewerConfig(runtime, &config)));
    CHECK(config.language == "en-US");
    CHECK(config.window_size.width == 960);

    WriteTestConfig("globalThis.notImgViewerConfig = {};");
    CHECK(SUCCEEDED(LoadImgViewerConfig(runtime, &config)));
    CHECK(config.language == "en-US");
    CHECK(ActionForKey(config.action_bindings, VK_RIGHT, false, false, false) == ImgViewerAction::NextImage);
    DeleteTestConfig();
}

void TestConfigJsSaveRoundTrip()
{
    DeleteTestConfig();
    ImgViewerConfig saved;
    saved.language = "zh-CN";
    saved.initial_image_view_mode = InitialImageViewMode::ActualSize;
    saved.remember_window_size = false;
    saved.window_size.width = 321;
    saved.window_size.height = 654;
    saved.action_bindings = DefaultActionBindings();
    ApplyKeyBindingConfig(ImgViewerAction::NextImage, {"Ctrl+N"}, &saved.action_bindings);
    ApplyKeyBindingConfig(ImgViewerAction::PreviousImage, {}, &saved.action_bindings);

    CHECK(SUCCEEDED(SaveImgViewerConfig(saved)));
    std::ifstream input(TestConfigPath(), std::ios::binary);
    const std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    CHECK(source.find("globalThis.imgviewerConfig =") != std::string::npos);

    script::QuickJsRuntime runtime;
    CHECK(runtime.Initialize());
    ImgViewerConfig loaded;
    CHECK(SUCCEEDED(LoadImgViewerConfig(runtime, &loaded)));
    CHECK(loaded.language == "zh-CN");
    CHECK(loaded.initial_image_view_mode == InitialImageViewMode::ActualSize);
    CHECK(!loaded.remember_window_size);
    CHECK(loaded.window_size.width == 321);
    CHECK(loaded.window_size.height == 654);
    CHECK(ActionForKey(loaded.action_bindings, 'N', true, false, false) == ImgViewerAction::NextImage);
    CHECK(ActionForKey(loaded.action_bindings, VK_LEFT, false, false, false) == ImgViewerAction::None);
    DeleteTestConfig();
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

void TestQuickJsObjectBuilder()
{
    script::QuickJsRuntime runtime;
    CHECK(runtime.Initialize());
    JSContext* context = runtime.Context();

    script::ObjectBuilder built(context);
    built.Set("text", "hello")
        .Set("wide", std::wstring(L"wide"))
        .Set("flag", true)
        .Set("i", int32_t{-7})
        .Set("u", uint32_t{42})
        .Set("f", 2.5f)
        .Set("d", 3.25)
        .SetObject("nested", [](script::ObjectBuilder& nested) {
            nested.Set("value", "child");
        })
        .SetValue("items", script::StringArray(context, std::vector<std::string>{"a", "b"}))
        .SetFunction("nativeAdd", TestNativeAdd, 2);

    JSValue global = JS_GetGlobalObject(context);
    JS_SetPropertyStr(context, global, "built", built.Release());
    JS_FreeValue(context, global);

    const script::QuickJsEvalResult result = runtime.EvalScript(
        "built.text === 'hello' && "
        "built.wide === 'wide' && "
        "built.flag === true && "
        "built.i === -7 && "
        "built.u === 42 && "
        "built.f === 2.5 && "
        "built.d === 3.25 && "
        "built.nested.value === 'child' && "
        "built.items.join(',') === 'a,b' && "
        "built.nativeAdd(4, 5) === 9 ? 'ok' : 'bad'",
        "quickjs-object-builder-test.js");
    CHECK(result.ok);
    CHECK(result.value_utf8 == "ok");
}

void TestQuickJsNativeFunctionRegistration()
{
    script::QuickJsRuntime runtime;
    CHECK(runtime.Initialize());
    JSContext* context = runtime.Context();
    JSValue global = JS_GetGlobalObject(context);
    JS_SetPropertyStr(context, global, "nativeAdd", JS_NewCFunction(context, TestNativeAdd, "nativeAdd", 2));
    JS_FreeValue(context, global);

    const script::QuickJsEvalResult result = runtime.EvalScript("nativeAdd(19, 23)", "quickjs-native-test.js");
    CHECK(result.ok);
    CHECK(result.value_utf8 == "42");
}

void TestQuickJsSignalApiSmoke()
{
    script::QuickJsRuntime runtime;
    CHECK(runtime.Initialize());
    TestSignalApi api;
    InstallTestSignals(runtime.Context(), &api);

    const script::QuickJsEvalResult result = runtime.EvalScript(
        "globalThis.seen = 0;"
        "const sub = signals.subscribe('test.counter', value => { globalThis.seen = value; });"
        "signals.set('test.counter', 5);"
        "signals.unsubscribe(sub);"
        "signals.set('test.counter', 8);"
        "signals.get('test.counter') + ':' + globalThis.seen;",
        "quickjs-signal-test.js");
    CHECK(result.ok);
    CHECK(result.value_utf8 == "8:5");

    if (api.subscription != 0) {
        api.value.Unsubscribe(api.subscription);
    }
    JS_FreeValue(runtime.Context(), api.callback);
}

void TestCanvasColorParser()
{
    const std::optional<D2D1_COLOR_F> rgb = script::ParseCanvasColor("#336699");
    CHECK(rgb.has_value());
    CHECK(NearF(rgb->r, 0x33 / 255.0f));
    CHECK(NearF(rgb->g, 0x66 / 255.0f));
    CHECK(NearF(rgb->b, 0x99 / 255.0f));
    CHECK(NearF(rgb->a, 1.0f));

    const std::optional<D2D1_COLOR_F> argb = script::ParseCanvasColor("#80336699");
    CHECK(argb.has_value());
    CHECK(NearF(argb->a, 0x80 / 255.0f));
    CHECK(NearF(argb->r, 0x33 / 255.0f));

    CHECK(!script::ParseCanvasColor("336699").has_value());
    CHECK(!script::ParseCanvasColor("#GG6699").has_value());
    CHECK(!script::ParseCanvasColor("#12345").has_value());
}

void TestScriptVectorIconReader()
{
    script::QuickJsRuntime runtime;
    CHECK(runtime.Initialize());

    const script::QuickJsEvalResult result = runtime.EvalScript(
        "globalThis.goodIcon = {"
        "  id: 'test-icon',"
        "  viewBox: [0, 0, 24, 24],"
        "  commands: [['M', 1, 2], ['L', 3, 4], ['C', 5, 6, 7, 8, 9, 10], ['Z']]"
        "};"
        "globalThis.badIcon = { viewBox: [0, 0, 24, 24], commands: [['A', 1, 2, 3, 4, 5, 6, 7]] };"
        "globalThis.noViewBox = { commands: [['M', 1, 2]] };"
        "'ready';",
        "vector-icon-test.js");
    CHECK(result.ok);

    JSContext* context = runtime.Context();
    JSValue global = JS_GetGlobalObject(context);
    JSValue good_value = JS_GetPropertyStr(context, global, "goodIcon");
    imgviewer::ScriptVectorIcon good_icon;
    CHECK(imgviewer::ReadVectorIcon(context, good_value, &good_icon));
    CHECK(good_icon.id == "test-icon");
    CHECK(good_icon.commands.size() == 4);
    CHECK(good_icon.commands[0].verb == icons::PathVerb::MoveTo);
    CHECK(good_icon.commands[1].verb == icons::PathVerb::LineTo);
    CHECK(good_icon.commands[2].verb == icons::PathVerb::CubicTo);
    CHECK(good_icon.commands[3].verb == icons::PathVerb::Close);
    CHECK(NearF(good_icon.view_box.right, 24.0f));
    CHECK(NearF(good_icon.view_box.bottom, 24.0f));
    JS_FreeValue(context, good_value);

    JSValue bad_value = JS_GetPropertyStr(context, global, "badIcon");
    imgviewer::ScriptVectorIcon bad_icon;
    CHECK(!imgviewer::ReadVectorIcon(context, bad_value, &bad_icon));
    JS_FreeValue(context, bad_value);

    JSValue no_view_box_value = JS_GetPropertyStr(context, global, "noViewBox");
    imgviewer::ScriptVectorIcon no_view_box_icon;
    CHECK(!imgviewer::ReadVectorIcon(context, no_view_box_value, &no_view_box_icon));
    JS_FreeValue(context, no_view_box_value);
    JS_FreeValue(context, global);
}

} // namespace

int main()
{
    TestPointerRouter();
    TestModalStack();
    TestEditGeometry();
    TestKeybindings();
    TestConfigJsLoad();
    TestConfigJsFallbacks();
    TestConfigJsSaveRoundTrip();
    TestSignal();
    TestSignalReentrancy();
    TestQuickJsRuntime();
    TestQuickJsObjectBuilder();
    TestQuickJsNativeFunctionRegistration();
    TestQuickJsSignalApiSmoke();
    TestCanvasColorParser();
    TestScriptVectorIconReader();

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
