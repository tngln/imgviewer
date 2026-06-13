#include "script.quickjs_runtime.hpp"

#include <utility>

#include "quickjs.h"

namespace script {
namespace {

std::string ToStringUtf8(JSContext* context, JSValueConst value)
{
    const char* text = JS_ToCString(context, value);
    if (text == nullptr) {
        return {};
    }

    std::string result(text);
    JS_FreeCString(context, text);
    return result;
}

} // namespace

QuickJsRuntime::~QuickJsRuntime()
{
    if (context_ != nullptr) {
        JS_FreeContext(context_);
        context_ = nullptr;
    }
    if (runtime_ != nullptr) {
        JS_FreeRuntime(runtime_);
        runtime_ = nullptr;
    }
}

bool QuickJsRuntime::Initialize(size_t memory_limit, size_t stack_size)
{
    if (IsInitialized()) {
        return true;
    }

    runtime_ = JS_NewRuntime();
    if (runtime_ == nullptr) {
        last_exception_utf8_ = "JS_NewRuntime failed";
        return false;
    }

    JS_SetMemoryLimit(runtime_, memory_limit);
    JS_SetMaxStackSize(runtime_, stack_size);
    JS_SetInterruptHandler(runtime_, &QuickJsRuntime::InterruptHandler, this);

    context_ = JS_NewContext(runtime_);
    if (context_ == nullptr) {
        last_exception_utf8_ = "JS_NewContext failed";
        JS_FreeRuntime(runtime_);
        runtime_ = nullptr;
        return false;
    }

    return true;
}

QuickJsEvalResult QuickJsRuntime::EvalScript(std::string_view source_utf8, std::string_view filename_utf8)
{
    if (!IsInitialized()) {
        last_exception_utf8_ = "QuickJsRuntime is not initialized";
        return {};
    }

    JSValue value = JS_Eval(
        context_,
        source_utf8.data(),
        source_utf8.size(),
        filename_utf8.empty() ? "<script>" : std::string(filename_utf8).c_str(),
        JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(value)) {
        JS_FreeValue(context_, value);
        CaptureException(context_);
        return {};
    }

    QuickJsEvalResult result{
        .ok = true,
        .value_utf8 = ToStringUtf8(context_, value),
    };
    JS_FreeValue(context_, value);
    return result;
}

int QuickJsRuntime::PumpJobs()
{
    if (runtime_ == nullptr) {
        last_exception_utf8_ = "QuickJsRuntime is not initialized";
        return -1;
    }

    int count = 0;
    while (true) {
        JSContext* job_context = nullptr;
        const int result = JS_ExecutePendingJob(runtime_, &job_context);
        if (result == 0) {
            return count;
        }
        if (result < 0) {
            CaptureException(job_context != nullptr ? job_context : context_);
            return -1;
        }
        ++count;
    }
}

std::string QuickJsRuntime::TakeExceptionTextUtf8()
{
    return std::exchange(last_exception_utf8_, {});
}

int QuickJsRuntime::InterruptHandler(JSRuntime*, void*)
{
    return 0;
}

void QuickJsRuntime::CaptureException(JSContext* context)
{
    if (context == nullptr) {
        last_exception_utf8_ = "JavaScript exception";
        return;
    }

    JSValue exception = JS_GetException(context);
    const std::string exception_text = ToStringUtf8(context, exception);
    JSValue stack = JS_GetPropertyStr(context, exception, "stack");
    const std::string stack_text =
        !JS_IsUndefined(stack) && !JS_IsException(stack) ? ToStringUtf8(context, stack) : std::string();
    if (!JS_IsUndefined(stack) && !JS_IsException(stack)) {
        last_exception_utf8_ = stack_text;
    }
    if (!exception_text.empty() &&
        last_exception_utf8_.find(exception_text) == std::string::npos) {
        last_exception_utf8_ = last_exception_utf8_.empty()
            ? exception_text
            : exception_text + "\n" + last_exception_utf8_;
    }
    if (last_exception_utf8_.empty()) {
        last_exception_utf8_ = "JavaScript exception";
    }
    JS_FreeValue(context, stack);
    JS_FreeValue(context, exception);
}

} // namespace script
