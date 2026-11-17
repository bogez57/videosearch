// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

#ifndef BGZ_BASE_INC_H
#define BGZ_BASE_INC_H

////////////////////////////////
//~ bgz: bgz's Base Additions
//
// What bgz adds on top of raddebugger's base (rad/base/, an exact copy - see
// rad/RAD_VERSION.md). Include after rad/base/base_inc.h.

#include "base_core.h"
#include "base_arena.h"
#include "base_memory.h"
#include "base_math.h"
#include "base_strings.h"
#include "base_thread_context.h"
#include "base_threads.h"
#include "base_command_line.h"
#include "base_log.h"
#include "base_files.h"
#include "base_processes.h"
#include "base_pipes.h"
#include "base_secrets.h"
#include "base_async.h"
#include "base_entry_point.h"

#if OS_WINDOWS
# include "../win32/base/win32_base.h" // relative: through -I it would find rad's
#else
# error bgz_base not implemented for this OS.
#endif

#endif // BGZ_BASE_INC_H
