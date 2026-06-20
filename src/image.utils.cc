#include "image.utils.hpp"

#include <filesystem>
#include <fstream>

#include <wil/result_macros.h>

namespace image_utils {

HRESULT ReadFileBytes(const wchar_t* path, std::vector<BYTE>* bytes, size_t max_size)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, path);
    RETURN_HR_IF_NULL(E_POINTER, bytes);

    std::ifstream stream(std::filesystem::path(path), std::ios::binary | std::ios::ate);
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), !stream);

    const std::streamoff file_size = stream.tellg();
    RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INVALID_DATA), file_size < 0);

    const size_t size = static_cast<size_t>((std::min)(
        static_cast<uint64_t>(file_size),
        static_cast<uint64_t>(max_size)));
    bytes->resize(size);
    stream.seekg(0, std::ios::beg);
    if (!bytes->empty()) {
        stream.read(reinterpret_cast<char*>(bytes->data()), static_cast<std::streamsize>(bytes->size()));
        RETURN_HR_IF(E_FAIL, stream.gcount() != static_cast<std::streamsize>(bytes->size()));
    }

    return S_OK;
}

} // namespace image_utils
