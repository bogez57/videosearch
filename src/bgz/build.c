////////////////////////////////
//~ bgz: bgz's unit
//
// raddebugger's layers (exact copies in rad/) and bgz's on top (bgz/). raddebugger's
// functions are `internal`, so core.c compiles all of this as one unit.
//
// raddebugger's .c files come before the render, font provider and font cache
// headers so its entry point doesn't start them - entry_point does
// (bgz/base/base_entry_point.c). raddebugger's socket and http are in rad/ but
// not built; bgz has its own http (bgz/http/http.h).

#define _CRT_SECURE_NO_WARNINGS

// What raddebugger's base reads to know what it is being built into. Set here,
// not in a file of the program's: bgz.dll is the same whichever program
// links it. Anything left out takes raddebugger's default - see the end of
// rad/base/base_context_cracking.h.
//
// A program with a window: raddebugger's update() calls frame() only when
// this is set. A program without one passes no frame function to
// CoreMainThreadEntry (bgz/base/base_entry_point.h).
#define OS_FEATURE_GRAPHICAL 1

// raddebugger's own async thread group (rad/base/base_entry_point.c) stays off:
// bgz runs its own - see bgz/base/base_async.h.
#define NEED_ASYNC 0

// raddebugger's code is held to raddebugger's warnings (its build.bat), not
// to bgz's -Werror -Wconversion -Wshadow (bgz's build.bat).
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Weverything"
#endif

//- raddebugger: headers
#include "rad/base/base_inc.h"
#include "rad/window_manager/window_manager_inc.h"
#include "rad/shell/shell_inc.h"

//- raddebugger: implementations
#include "rad/base/base_inc.c"
#include "rad/window_manager/window_manager_inc.c"
#include "rad/shell/shell_inc.c"

#if defined(__clang__)
# pragma clang diagnostic pop
#endif

//- bgz: headers
#include "bgz/base/base_inc.h"
#include "bgz/window_manager/window_manager.h"
#include "bgz/shell/shell.h"
#include "bgz/http/http.h"

#include "bgz/render/render_core.h"

// raddebugger's, after bgz's render header and held to its warnings like the rest of rad.
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Weverything"
#endif
#include "rad/font_provider/font_provider_inc.h"
#if defined(__clang__)
# pragma clang diagnostic pop
#endif

#include "bgz/font_provider/font_provider.h"
#include "bgz/font_cache/font_cache.h"
#include "bgz/draw/draw.h"
#include "bgz/c_lex/c_lex.h"

//- bgz: implementations
#include "bgz/base/base_inc.c"
#include "bgz/window_manager/window_manager.c"
#include "bgz/shell/shell.c"
#include "bgz/http/http.c"
#include "bgz/render/render_core.c"
#include "bgz/render/sokol/render_sokol.c"

#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Weverything"
#endif
#include "rad/font_provider/font_provider_inc.c"
#if defined(__clang__)
# pragma clang diagnostic pop
#endif

#include "bgz/font_provider/font_provider.c"
#include "bgz/font_cache/font_cache.c"
#include "bgz/draw/draw.c"
#include "bgz/c_lex/c_lex.c"
