#pragma once

#include <windows.h>

#include <optional>
#include <string>
#include <vector>

struct ImageSequencePosition final {
    size_t index = 0;
    size_t total = 0;
};

class ImageSequence final {
public:
    void Clear();
    HRESULT SetCurrentPath(const wchar_t* path);
    std::optional<std::wstring> Previous() const;
    std::optional<std::wstring> Next() const;
    ImageSequencePosition Position() const;

private:
    std::vector<std::wstring> files_;
    size_t current_index_ = 0;
};
