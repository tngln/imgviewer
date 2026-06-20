#pragma once

#include <windows.h>

#include <cstdint>
#include <unordered_map>

#include <quickjs.h>

#include "script.quickjs_runtime.hpp"

namespace script {

class ScriptTimerManager final {
public:
    explicit ScriptTimerManager(QuickJsRuntime& engine);
    ScriptTimerManager(const ScriptTimerManager&) = delete;
    ScriptTimerManager& operator=(const ScriptTimerManager&) = delete;
    ~ScriptTimerManager();

    void SetHwnd(HWND hwnd);
    uint32_t SetTimer(JSContext* context, JSValueConst callback, uint32_t delay_ms, bool repeat);
    void ClearTimer(uint32_t id);
    bool HasTimer(uint32_t id) const;
    bool OnTimer(uint32_t id, bool* value_changed);
    void ClearAll();

private:
    struct Entry final {
        JSContext* context = nullptr;
        JSValue callback = JS_UNDEFINED;
        uint32_t delay_ms = 0;
        bool repeat = false;
        bool armed = false;
    };

    UINT_PTR NativeTimerId(uint32_t id) const;
    bool ArmTimer(uint32_t id, Entry* entry);

    QuickJsRuntime& engine_;
    HWND hwnd_ = nullptr;
    uint32_t next_id_ = 1;
    std::unordered_map<uint32_t, Entry> timers_;
};

constexpr UINT_PTR kScriptTimerNativeBase = 0x4000;

} // namespace script
