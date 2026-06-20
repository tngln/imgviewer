#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <quickjs.h>

namespace script {

class QuickJsValue final {
public:
    QuickJsValue(JSContext* context, JSValue value);
    QuickJsValue(const QuickJsValue&) = delete;
    QuickJsValue& operator=(const QuickJsValue&) = delete;
    QuickJsValue(QuickJsValue&& other) noexcept;
    QuickJsValue& operator=(QuickJsValue&& other) noexcept;
    ~QuickJsValue();

    JSValue Get() const { return value_; }
    JSValueConst GetConst() const { return value_; }
    JSValue Release();
    void Reset(JSContext* context, JSValue value);

private:
    JSContext* context_ = nullptr;
    JSValue value_ = JS_UNDEFINED;
};

class ObjectBuilder final {
public:
    explicit ObjectBuilder(JSContext* context);
    ObjectBuilder(const ObjectBuilder&) = delete;
    ObjectBuilder& operator=(const ObjectBuilder&) = delete;
    ObjectBuilder(ObjectBuilder&& other) noexcept = default;
    ObjectBuilder& operator=(ObjectBuilder&& other) noexcept = default;

    JSValue Get() const { return value_.Get(); }
    JSValue Release() { return value_.Release(); }

    ObjectBuilder& Set(const char* name, std::wstring_view value);
    ObjectBuilder& Set(const char* name, std::string_view value);
    ObjectBuilder& Set(const char* name, const char* value);
    ObjectBuilder& Set(const char* name, bool value);
    ObjectBuilder& Set(const char* name, int32_t value);
    ObjectBuilder& Set(const char* name, uint32_t value);
    ObjectBuilder& Set(const char* name, float value);
    ObjectBuilder& Set(const char* name, double value);
    ObjectBuilder& SetFunction(const char* name, JSCFunction* function, int length);
    ObjectBuilder& SetValue(const char* name, JSValue value);

    template <typename Fill>
    ObjectBuilder& SetObject(const char* name, Fill fill)
    {
        ObjectBuilder object(context_);
        fill(object);
        return SetValue(name, object.Release());
    }

private:
    JSContext* context_ = nullptr;
    QuickJsValue value_;
};

std::string ToStringUtf8(JSContext* context, JSValueConst value);

void SetString(JSContext* context, JSValue object, const char* name, std::wstring_view value);
void SetString(JSContext* context, JSValue object, const char* name, std::string_view value);
void SetString(JSContext* context, JSValue object, const char* name, const char* value);
void SetBool(JSContext* context, JSValue object, const char* name, bool value);
void SetInt(JSContext* context, JSValue object, const char* name, int32_t value);
void SetUint(JSContext* context, JSValue object, const char* name, uint32_t value);
void SetFloat(JSContext* context, JSValue object, const char* name, float value);
void SetFunction(JSContext* context, JSValue object, const char* name, JSCFunction* function, int length);

bool BoolProperty(JSContext* context, JSValueConst object, const char* name, bool fallback);
std::optional<bool> OptionalBoolProperty(JSContext* context, JSValueConst object, const char* name);
int32_t Int32Property(JSContext* context, JSValueConst object, const char* name, int32_t fallback);
float FloatProperty(JSContext* context, JSValueConst object, const char* name, float fallback);

bool StringProperty(JSContext* context, JSValueConst object, const char* name, std::string* result);
bool StrictBoolProperty(JSContext* context, JSValueConst object, const char* name, bool* result);
bool StrictInt32Property(JSContext* context, JSValueConst object, const char* name, int32_t* result);

bool ArrayLength(JSContext* context, JSValueConst value, uint32_t* length);
bool ArrayNumber(JSContext* context, JSValueConst value, uint32_t index, float* number);
JSValue StringArray(JSContext* context, const std::vector<std::string>& values);
JSValue WideStringArray(JSContext* context, const std::vector<std::wstring>& values);

} // namespace script
