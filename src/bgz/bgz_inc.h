////////////////////////////////
//~ bgz: what a program includes
//
// The program's side of bgz.dll, and the first thing its main unit includes:
// the headers of raddebugger's layers and of bgz's. build.c is the dll's side.
//
// raddebugger's layers are compiled into bgz.dll and are `internal` there,
// so a program cannot call their functions. It calls bgz's instead: per area,
// a bgz file (bgz/base/base_threads.h, bgz/window_manager/window_manager.h,
// ...) whose `exported` PascalCase functions call raddebugger's of the same
// name. In the program, which only declares them, `exported` imports them
// (bgz/base/base_core.h). raddebugger's types, enums and the macros that call
// nothing (MemoryCopy, DLLPushBack, KB, ...) are used as they are.
//
// A raddebugger function a program starts using needs a PascalCase function
// in the bgz file for its area.
//
// Include paths: the folder this file is in, and its rad and bgz folders -
// rad first (see build.bat).

#ifndef BGZ_INC_H
#define BGZ_INC_H

// raddebugger's code is held to raddebugger's warnings, not the program's (as
// in build.c for the dll).
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Weverything"
#endif
#include "rad/base/base_inc.h"
#include "rad/window_manager/window_manager_inc.h"
#include "rad/shell/shell_inc.h"
#include "rad/font_provider/font_provider.h" // its types, for bgz's font provider and font cache headers
#if defined(__clang__)
# pragma clang diagnostic pop
#endif

#include "bgz/base/base_inc.h"
#include "bgz/window_manager/window_manager.h"
#include "bgz/shell/shell.h"
#include "bgz/http/http.h"
#include "bgz/render/render_core.h"
#include "bgz/font_provider/font_provider.h"
#include "bgz/font_cache/font_cache.h"
#include "bgz/draw/draw.h"

// raddebugger's base_markup.h compiles its debugger markup (thread names and
// colors, for raddbg) into every unit that includes base, and that prints with
// raddbg_vsnprintf - stb_sprintf, which base_core.h makes static to each
// unit. bgz.dll has its copy (rad/base/base_core.c); this is the program's,
// with the one base function raddebugger's stb_sprintf calls.
internal void
memory_write32(void *ptr, U32 v)
{
  MemoryCopy(ptr, &v, sizeof(v));
}
#define STB_SPRINTF_IMPLEMENTATION
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Weverything"
#endif
#include "third_party/stb/stb_sprintf.h"
#if defined(__clang__)
# pragma clang diagnostic pop
#endif

#endif // BGZ_INC_H
