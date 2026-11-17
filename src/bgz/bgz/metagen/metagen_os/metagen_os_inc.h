// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

#ifndef OS_INC_H
#define OS_INC_H

#if !defined(ArenaImpl_Reserve)
# define ArenaImpl_Reserve  OS_Reserve
#endif
#if !defined(ArenaImpl_Release)
# define ArenaImpl_Release  OS_Release
#endif
#if !defined(ArenaImpl_Commit)
# define ArenaImpl_Commit   OS_Commit
#endif
#if !defined(ArenaImpl_Decommit)
# define ArenaImpl_Decommit OS_Decommit
#endif

#if !defined(TCTXImpl_SetThreadName)
# define TCTXImpl_SetThreadName OS_SetThreadName
#endif

#include "metagen/metagen_os/core/metagen_os_core.h"

#if OS_WINDOWS
# include "metagen/metagen_os/core/win32/metagen_os_core_win32.h"
#else
# error OS layer not implemented.
#endif

#endif // OS_INC_H
