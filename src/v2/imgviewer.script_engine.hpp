#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

#include "script.quickjs_runtime.hpp"

struct JSContext;
struct JSRuntime;

namespace imgviewer::v2 {

struct ScriptEvalResult final {
    bool ok = false;
    std::string value_utf8;
};

class ScriptContext;

class ScriptEngine final {
public:
    ScriptEngine() = default;
    ScriptEngine(const ScriptEngine&) = delete;
    ScriptEngine& operator=(const ScriptEngine&) = delete;
    ~ScriptEngine();

    bool Initialize(
        size_t memory_limit = script::kDefaultQuickJsMemoryLimit,
        size_t stack_size = script::kDefaultQuickJsStackSize);
    std::unique_ptr<ScriptContext> CreateContext();
    int PumpJobs();
    void CaptureException(JSContext* context);
    std::string TakeExceptionTextUtf8();

    bool IsInitialized() const { return runtime_ != nullptr; }
    JSRuntime* Runtime() const { return runtime_; }

private:
    static int InterruptHandler(JSRuntime* runtime, void* opaque);

    JSRuntime* runtime_ = nullptr;
    std::string last_exception_utf8_;
};

class ScriptContext final {
public:
    ScriptContext(ScriptEngine& engine, JSContext* context);
    ScriptContext(const ScriptContext&) = delete;
    ScriptContext& operator=(const ScriptContext&) = delete;
    ~ScriptContext();

    JSContext* Context() const { return context_; }
    ScriptEngine& Engine() const { return engine_; }
    ScriptEvalResult EvalScript(std::string_view source_utf8, std::string_view filename_utf8);
    void CaptureException();

private:
    ScriptEngine& engine_;
    JSContext* context_ = nullptr;
};

} // namespace imgviewer::v2
