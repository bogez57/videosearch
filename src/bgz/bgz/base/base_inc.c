// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

#include "base_core.c"
#include "base_arena.c"
#include "base_memory.c"
#include "base_math.c"
#include "base_strings.c"
#include "base_thread_context.c"
#include "base_threads.c"
#include "base_command_line.c"
#include "base_log.c"
#include "base_files.c"
#include "base_processes.c"
#include "base_async.c"
#include "base_entry_point.c"

#if OS_WINDOWS
# include "../win32/base/win32_base.c"
#endif
