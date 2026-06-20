#include "script.timer.hpp"

#include <algorithm>
#include <utility>

#include "script.quickjs_helper.hpp"

namespace script {

ScriptTimerManager::ScriptTimerManager(QuickJsRuntime& engine) :
    engine_(engine)
{
}

ScriptTimerManager::~ScriptTimerManager()
{
    ClearAll();
}

void ScriptTimerManager::SetHwnd(HWND hwnd)
{
    if (hwnd_ == hwnd) {
        return;
    }
    if (hwnd_ != nullptr) {
        for (auto& [id, entry] : timers_) {
            if (entry.armed) {
                KillTimer(hwnd_, NativeTimerId(id));
                entry.armed = false;
            }
        }
    }
    hwnd_ = hwnd;
    if (hwnd_ != nullptr) {
        for (auto& [id, entry] : timers_) {
            ArmTimer(id, &entry);
        }
    }
}

uint32_t ScriptTimerManager::SetTimer(JSContext* context, JSValueConst callback, uint32_t delay_ms, bool repeat)
{
    if (context == nullptr || !JS_IsFunction(context, callback)) {
        return 0;
    }

    uint32_t id = next_id_++;
    if (id == 0) {
        id = next_id_++;
    }
    const UINT native_delay = (std::max)(1u, delay_ms);
    auto [it, inserted] = timers_.emplace(id, Entry{
        .context = context,
        .callback = JS_DupValue(context, callback),
        .delay_ms = native_delay,
        .repeat = repeat,
    });
    if (!inserted) {
        return 0;
    }
    if (hwnd_ != nullptr && !ArmTimer(id, &it->second)) {
        JS_FreeValue(context, it->second.callback);
        timers_.erase(it);
        return 0;
    }
    return id;
}

void ScriptTimerManager::ClearTimer(uint32_t id)
{
    const auto found = timers_.find(id);
    if (found == timers_.end()) {
        return;
    }
    if (hwnd_ != nullptr && found->second.armed) {
        KillTimer(hwnd_, NativeTimerId(id));
    }
    JS_FreeValue(found->second.context, found->second.callback);
    timers_.erase(found);
}

bool ScriptTimerManager::HasTimer(uint32_t id) const
{
    return timers_.find(id) != timers_.end();
}

bool ScriptTimerManager::OnTimer(uint32_t id, bool* value_changed)
{
    const auto found = timers_.find(id);
    if (found == timers_.end()) {
        return false;
    }

    Entry entry = found->second;
    if (!entry.repeat) {
        timers_.erase(found);
        if (hwnd_ != nullptr && entry.armed) {
            KillTimer(hwnd_, NativeTimerId(id));
        }
    }

    JSValue result = JS_Call(entry.context, entry.callback, JS_UNDEFINED, 0, nullptr);
    const bool exception = JS_IsException(result);
    JS_FreeValue(entry.context, result);
    const bool failed = exception || engine_.PumpJobs() < 0;
    if (!entry.repeat) {
        JS_FreeValue(entry.context, entry.callback);
    }
    if (value_changed != nullptr) {
        *value_changed = false;
    }
    return !failed;
}

void ScriptTimerManager::ClearAll()
{
    for (auto& [id, entry] : timers_) {
        if (hwnd_ != nullptr && entry.armed) {
            KillTimer(hwnd_, NativeTimerId(id));
        }
        JS_FreeValue(entry.context, entry.callback);
    }
    timers_.clear();
}

UINT_PTR ScriptTimerManager::NativeTimerId(uint32_t id) const
{
    return kScriptTimerNativeBase + id;
}

bool ScriptTimerManager::ArmTimer(uint32_t id, Entry* entry)
{
    if (hwnd_ == nullptr || entry == nullptr) {
        return true;
    }
    if (::SetTimer(hwnd_, NativeTimerId(id), entry->delay_ms, nullptr) == 0) {
        return false;
    }
    entry->armed = true;
    return true;
}

} // namespace script
