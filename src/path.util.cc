#include "path.util.hpp"

namespace util {

std::wstring FileNameFromPath(const wchar_t* path, const wchar_t* fallback)
{
    if (path == nullptr) {
        return fallback;
    }

    std::wstring value(path);
    const size_t separator = value.find_last_of(L"\\/");
    if (separator != std::wstring::npos) {
        value.erase(0, separator + 1);
    }

    return value.empty() ? std::wstring(fallback) : value;
}

} // namespace util
