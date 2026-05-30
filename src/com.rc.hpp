#pragma once

#include <windows.h>

template <typename Owner>
class ComRc final {
public:
    ULONG AddRef() noexcept
    {
        return static_cast<ULONG>(InterlockedIncrement(&ref_count_));
    }

    ULONG Release(Owner* owner) noexcept
    {
        const ULONG ref_count = static_cast<ULONG>(InterlockedDecrement(&ref_count_));
        if (ref_count == 0) {
            delete owner;
        }

        return ref_count;
    }

private:
    volatile LONG ref_count_ = 1;
};
