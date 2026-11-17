// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

#include "metagen/metagen_os/core/metagen_os_core.c"
#if OS_WINDOWS
# include "metagen/metagen_os/core/win32/metagen_os_core_win32.c"
#else
# error OS layer not implemented.
#endif
