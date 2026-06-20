#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>

struct JSContext;
struct JSRuntime;

namespace script {

inline constexpr size_t kDefaultQuickJsMemoryLimit = 32u * 1024u * 1024u;
inline constexpr size_t kDefaultQuickJsStackSize = 1024u * 1024u;

struct QuickJsEvalResult final {
    bool ok = false;
    std::string value_utf8;
};

class QuickJsRuntime;

class QuickJsContext final {
public:
    QuickJsContext(QuickJsRuntime& runtime, JSContext* context);
    QuickJsContext(const QuickJsContext&) = delete;
    QuickJsContext& operator=(const QuickJsContext&) = delete;
    ~QuickJsContext();

    JSContext* Context() const { return context_; }
    QuickJsRuntime& Runtime() const { return runtime_; }
    QuickJsEvalResult EvalScript(std::string_view source_utf8, std::string_view filename_utf8);
    void CaptureException();

private:
    QuickJsRuntime& runtime_;
    JSContext* context_ = nullptr;
};

class QuickJsRuntime final {
public:
    QuickJsRuntime() = default;
    QuickJsRuntime(const QuickJsRuntime&) = delete;
    QuickJsRuntime& operator=(const QuickJsRuntime&) = delete;
    QuickJsRuntime(QuickJsRuntime&&) = delete;
    QuickJsRuntime& operator=(QuickJsRuntime&&) = delete;
    ~QuickJsRuntime();

    bool Initialize(
        size_t memory_limit = kDefaultQuickJsMemoryLimit,
        size_t stack_size = kDefaultQuickJsStackSize);
    std::unique_ptr<QuickJsContext> CreateContext();
    QuickJsEvalResult EvalScript(std::string_view source_utf8, std::string_view filename_utf8);
    int PumpJobs();
    std::string TakeExceptionTextUtf8();
    void CaptureException(JSContext* context = nullptr);

    bool IsInitialized() const { return runtime_ != nullptr; }
    JSRuntime* Runtime() const { return runtime_; }
    JSContext* Context();

private:
    static int InterruptHandler(JSRuntime* runtime, void* opaque);

    JSRuntime* runtime_ = nullptr;
    std::unique_ptr<QuickJsContext> default_context_;
    std::string last_exception_utf8_;
};

} // namespace script
