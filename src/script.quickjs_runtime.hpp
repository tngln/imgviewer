#pragma once

#include <cstddef>
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
    QuickJsEvalResult EvalScript(std::string_view source_utf8, std::string_view filename_utf8);
    int PumpJobs();
    std::string TakeExceptionTextUtf8();

    bool IsInitialized() const { return runtime_ != nullptr && context_ != nullptr; }

private:
    static int InterruptHandler(JSRuntime* runtime, void* opaque);
    void CaptureException(JSContext* context);

    JSRuntime* runtime_ = nullptr;
    JSContext* context_ = nullptr;
    std::string last_exception_utf8_;
};

} // namespace script
