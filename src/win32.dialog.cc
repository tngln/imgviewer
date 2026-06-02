#include "win32.dialog.hpp"

#include <shobjidl.h>

#include <vector>

#include <wil/com.h>
#include <wil/resource.h>
#include <wil/result_macros.h>

namespace win32 {

HRESULT OpenNativeFileDialog(HWND hwnd, const NativeOpenFileDialogOptions& options, std::wstring* path)
{
    RETURN_HR_IF_NULL(E_POINTER, path);

    wil::com_ptr<IFileOpenDialog> dialog;
    RETURN_IF_FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(dialog.put())));

    if (!options.filters.empty()) {
        std::vector<COMDLG_FILTERSPEC> filters;
        filters.reserve(options.filters.size());
        for (const NativeFileDialogFilter& filter : options.filters) {
            filters.push_back({filter.name, filter.pattern});
        }
        RETURN_IF_FAILED(dialog->SetFileTypes(static_cast<UINT>(filters.size()), filters.data()));
        RETURN_IF_FAILED(dialog->SetFileTypeIndex(options.default_filter_index));
    }

    const HRESULT show_result = dialog->Show(hwnd);
    if (show_result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
        return show_result;
    }
    RETURN_IF_FAILED(show_result);

    wil::com_ptr<IShellItem> item;
    RETURN_IF_FAILED(dialog->GetResult(item.put()));

    wil::unique_cotaskmem_string file_path;
    RETURN_IF_FAILED(item->GetDisplayName(SIGDN_FILESYSPATH, file_path.put()));
    *path = file_path.get();
    return S_OK;
}

} // namespace win32
