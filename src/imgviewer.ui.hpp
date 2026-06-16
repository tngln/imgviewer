#pragma once

#if defined(VER2)
#include "v2/imgviewer.ui.hpp"
#else
#error "The native C++ overlay was removed. Build with VER2."
#endif // defined(VER2)
