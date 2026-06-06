#include "image.sequence.hpp"

#include <algorithm>
#include <cwctype>
#include <filesystem>

#include <shlwapi.h>
#include <wil/result_macros.h>

namespace {

bool IsImageExtension(std::wstring extension)
{
    std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t value) {
        return static_cast<wchar_t>(std::towlower(value));
    });

    return extension == L".bmp" ||
        extension == L".dib" ||
        extension == L".gif" ||
        extension == L".ico" ||
        extension == L".jpg" ||
        extension == L".jpeg" ||
        extension == L".jpe" ||
        extension == L".png" ||
        extension == L".psd" ||
        extension == L".tif" ||
        extension == L".tiff" ||
        extension == L".tga" ||
        extension == L".webp";
}

bool PathSortsBefore(const std::wstring& left, const std::wstring& right)
{
    const std::filesystem::path left_path(left);
    const std::filesystem::path right_path(right);
    const int name_order = StrCmpLogicalW(left_path.filename().c_str(), right_path.filename().c_str());
    if (name_order != 0) {
        return name_order < 0;
    }

    return StrCmpLogicalW(left.c_str(), right.c_str()) < 0;
}

} // namespace

void ImageSequence::Clear()
{
    files_.clear();
    current_index_ = 0;
}

HRESULT ImageSequence::SetCurrentPath(const wchar_t* path)
{
    Clear();

    RETURN_HR_IF_NULL(E_INVALIDARG, path);
    if (path[0] == L'\0') {
        return E_INVALIDARG;
    }

    const std::filesystem::path current_path(path);
    const std::filesystem::path directory = current_path.parent_path();
    if (directory.empty()) {
        files_.assign(1, current_path.wstring());
        current_index_ = 0;
        return S_OK;
    }

    std::vector<std::wstring> files;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        if (error) {
            return HRESULT_FROM_WIN32(error.value());
        }

        const std::filesystem::path entry_path = entry.path();
        if (entry.is_regular_file(error) && !error && IsImageExtension(entry_path.extension().wstring())) {
            files.push_back(entry_path.wstring());
        }
    }
    if (error) {
        return HRESULT_FROM_WIN32(error.value());
    }

    files.push_back(current_path.wstring());
    std::sort(files.begin(), files.end(), PathSortsBefore);
    files.erase(std::unique(files.begin(), files.end()), files.end());

    const auto current = std::find(files.begin(), files.end(), current_path.wstring());
    if (current == files.end()) {
        return E_FAIL;
    }

    current_index_ = static_cast<size_t>(std::distance(files.begin(), current));
    files_ = std::move(files);
    return S_OK;
}

std::optional<std::wstring> ImageSequence::Previous() const
{
    if (files_.empty() || current_index_ == 0) {
        return std::nullopt;
    }

    return files_[current_index_ - 1];
}

std::optional<std::wstring> ImageSequence::Next() const
{
    if (files_.empty() || current_index_ + 1 >= files_.size()) {
        return std::nullopt;
    }

    return files_[current_index_ + 1];
}

ImageSequencePosition ImageSequence::Position() const
{
    if (files_.empty()) {
        return {};
    }

    return ImageSequencePosition{
        .index = current_index_ + 1,
        .total = files_.size(),
    };
}
