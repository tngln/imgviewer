#include "script.quickjs_helper.hpp"

#include <utility>

#include <windows.h>

namespace script {
namespace {

std::string Utf8FromWide(std::wstring_view text)
{
    if (text.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return {};
    }
    std::string value(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), value.data(), length, nullptr, nullptr);
    return value;
}

} // namespace

QuickJsValue::QuickJsValue(JSContext* context, JSValue value) :
    context_(context),
    value_(value)
{
}

QuickJsValue::QuickJsValue(QuickJsValue&& other) noexcept :
    context_(std::exchange(other.context_, nullptr)),
    value_(std::exchange(other.value_, JS_UNDEFINED))
{
}

QuickJsValue& QuickJsValue::operator=(QuickJsValue&& other) noexcept
{
    if (this != &other) {
        Reset(nullptr, JS_UNDEFINED);
        context_ = std::exchange(other.context_, nullptr);
        value_ = std::exchange(other.value_, JS_UNDEFINED);
    }
    return *this;
}

QuickJsValue::~QuickJsValue()
{
    Reset(nullptr, JS_UNDEFINED);
}

JSValue QuickJsValue::Release()
{
    context_ = nullptr;
    return std::exchange(value_, JS_UNDEFINED);
}

void QuickJsValue::Reset(JSContext* context, JSValue value)
{
    if (context_ != nullptr) {
        JS_FreeValue(context_, value_);
    }
    context_ = context;
    value_ = value;
}

ObjectBuilder::ObjectBuilder(JSContext* context) :
    context_(context),
    value_(context, JS_NewObject(context))
{
}

ObjectBuilder& ObjectBuilder::Set(const char* name, std::wstring_view value)
{
    SetString(context_, Get(), name, value);
    return *this;
}

ObjectBuilder& ObjectBuilder::Set(const char* name, std::string_view value)
{
    SetString(context_, Get(), name, value);
    return *this;
}

ObjectBuilder& ObjectBuilder::Set(const char* name, const char* value)
{
    SetString(context_, Get(), name, value);
    return *this;
}

ObjectBuilder& ObjectBuilder::Set(const char* name, bool value)
{
    SetBool(context_, Get(), name, value);
    return *this;
}

ObjectBuilder& ObjectBuilder::Set(const char* name, int32_t value)
{
    SetInt(context_, Get(), name, value);
    return *this;
}

ObjectBuilder& ObjectBuilder::Set(const char* name, uint32_t value)
{
    SetUint(context_, Get(), name, value);
    return *this;
}

ObjectBuilder& ObjectBuilder::Set(const char* name, float value)
{
    SetFloat(context_, Get(), name, value);
    return *this;
}

ObjectBuilder& ObjectBuilder::Set(const char* name, double value)
{
    JS_SetPropertyStr(context_, Get(), name, JS_NewFloat64(context_, value));
    return *this;
}

ObjectBuilder& ObjectBuilder::SetFunction(const char* name, JSCFunction* function, int length)
{
    script::SetFunction(context_, Get(), name, function, length);
    return *this;
}

ObjectBuilder& ObjectBuilder::SetValue(const char* name, JSValue value)
{
    JS_SetPropertyStr(context_, Get(), name, value);
    return *this;
}

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

void SetString(JSContext* context, JSValue object, const char* name, std::wstring_view value)
{
    JS_SetPropertyStr(context, object, name, JS_NewString(context, Utf8FromWide(value).c_str()));
}

void SetString(JSContext* context, JSValue object, const char* name, std::string_view value)
{
    const std::string text(value);
    JS_SetPropertyStr(context, object, name, JS_NewString(context, text.c_str()));
}

void SetString(JSContext* context, JSValue object, const char* name, const char* value)
{
    SetString(context, object, name, std::string_view(value != nullptr ? value : ""));
}

void SetBool(JSContext* context, JSValue object, const char* name, bool value)
{
    JS_SetPropertyStr(context, object, name, JS_NewBool(context, value));
}

void SetInt(JSContext* context, JSValue object, const char* name, int32_t value)
{
    JS_SetPropertyStr(context, object, name, JS_NewInt32(context, value));
}

void SetUint(JSContext* context, JSValue object, const char* name, uint32_t value)
{
    JS_SetPropertyStr(context, object, name, JS_NewUint32(context, value));
}

void SetFloat(JSContext* context, JSValue object, const char* name, float value)
{
    JS_SetPropertyStr(context, object, name, JS_NewFloat64(context, value));
}

void SetFunction(JSContext* context, JSValue object, const char* name, JSCFunction* function, int length)
{
    JS_SetPropertyStr(context, object, name, JS_NewCFunction(context, function, name, length));
}

bool BoolProperty(JSContext* context, JSValueConst object, const char* name, bool fallback)
{
    if (!JS_IsObject(object)) {
        return fallback;
    }
    QuickJsValue value(context, JS_GetPropertyStr(context, object, name));
    return JS_IsUndefined(value.Get()) ? fallback : JS_ToBool(context, value.Get()) != 0;
}

std::optional<bool> OptionalBoolProperty(JSContext* context, JSValueConst object, const char* name)
{
    if (!JS_IsObject(object)) {
        return std::nullopt;
    }
    QuickJsValue value(context, JS_GetPropertyStr(context, object, name));
    if (JS_IsUndefined(value.Get())) {
        return std::nullopt;
    }
    return JS_ToBool(context, value.Get()) != 0;
}

int32_t Int32Property(JSContext* context, JSValueConst object, const char* name, int32_t fallback)
{
    if (!JS_IsObject(object)) {
        return fallback;
    }
    QuickJsValue value(context, JS_GetPropertyStr(context, object, name));
    if (JS_IsUndefined(value.Get())) {
        return fallback;
    }
    int32_t result = fallback;
    JS_ToInt32(context, &result, value.Get());
    return result;
}

float FloatProperty(JSContext* context, JSValueConst object, const char* name, float fallback)
{
    if (!JS_IsObject(object)) {
        return fallback;
    }
    QuickJsValue value(context, JS_GetPropertyStr(context, object, name));
    if (JS_IsUndefined(value.Get())) {
        return fallback;
    }
    double result = fallback;
    JS_ToFloat64(context, &result, value.Get());
    return static_cast<float>(result);
}

bool StringProperty(JSContext* context, JSValueConst object, const char* name, std::string* result)
{
    QuickJsValue value(context, JS_GetPropertyStr(context, object, name));
    if (!JS_IsString(value.Get())) {
        return false;
    }

    *result = ToStringUtf8(context, value.Get());
    return true;
}

bool StrictBoolProperty(JSContext* context, JSValueConst object, const char* name, bool* result)
{
    QuickJsValue value(context, JS_GetPropertyStr(context, object, name));
    if (!JS_IsBool(value.Get())) {
        return false;
    }

    *result = JS_ToBool(context, value.Get()) != 0;
    return true;
}

bool StrictInt32Property(JSContext* context, JSValueConst object, const char* name, int32_t* result)
{
    QuickJsValue value(context, JS_GetPropertyStr(context, object, name));
    if (!JS_IsNumber(value.Get())) {
        return false;
    }

    return JS_ToInt32(context, result, value.Get()) == 0;
}

bool ArrayLength(JSContext* context, JSValueConst value, uint32_t* length)
{
    QuickJsValue length_value(context, JS_GetPropertyStr(context, value, "length"));
    return JS_ToUint32(context, length, length_value.Get()) == 0;
}

bool ArrayNumber(JSContext* context, JSValueConst value, uint32_t index, float* number)
{
    QuickJsValue item(context, JS_GetPropertyUint32(context, value, index));
    double parsed = 0.0;
    const bool ok = JS_ToFloat64(context, &parsed, item.Get()) == 0;
    if (ok) {
        *number = static_cast<float>(parsed);
    }
    return ok;
}

JSValue StringArray(JSContext* context, const std::vector<std::string>& values)
{
    JSValue array = JS_NewArray(context);
    uint32_t index = 0;
    for (const std::string& value : values) {
        JS_SetPropertyUint32(context, array, index++, JS_NewString(context, value.c_str()));
    }
    return array;
}

JSValue WideStringArray(JSContext* context, const std::vector<std::wstring>& values)
{
    JSValue array = JS_NewArray(context);
    uint32_t index = 0;
    for (const std::wstring& value : values) {
        JS_SetPropertyUint32(context, array, index++, JS_NewString(context, Utf8FromWide(value).c_str()));
    }
    return array;
}

} // namespace script
