////////////////////////////////
//~ bgz: Entry Point
//
// raddebugger's base owns the entry point (rad/base/base_entry_point.c): its
// win32 half sets up the process (w32_entry_point_caller), then
// main_thread_base_entry_point starts every layer it was built with and calls
// entry_point(). Its window manager calls update() - and so frame() - while a
// window is being resized. All of that is in core.dll, so the app's main()
// hands its own two functions to CoreMainThreadEntry and core.dll calls them
// from there.
//
// entry is called once, with the command line, on the main thread; when it
// returns the program is done. frame draws a frame; it is called whenever
// raddebugger's update() runs, from inside wm_get_events too, so it must not
// run a frame inside a frame. 0 for a program with no window (the app
// metaprogram): then bgz's render, font provider and font cache are not
// started.

#ifndef BGZ_BASE_ENTRY_POINT_H
#define BGZ_BASE_ENTRY_POINT_H

typedef void CORE_EntryPointFunctionType(CmdLine* cmdline);
typedef B32  CORE_FrameFunctionType(void);

exported void CoreMainThreadEntry(CORE_EntryPointFunctionType* entry, CORE_FrameFunctionType* frame);

#endif // BGZ_BASE_ENTRY_POINT_H
