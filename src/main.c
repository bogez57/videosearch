////////////////////////////////
//~ bgz: videosearch
//
// The program's one unit: bgz's headers, then each layer's header, then each
// layer's .c, in dependency order. bgz itself is not compiled here - it is
// bgz.dll (src\bgz\build.bat), linked through bin\bgz.lib.
//
//    bin\videosearch.exe                the folders of videos searched last time
//    bin\videosearch.exe <folder> ...   adds those folders to them
//
// A folder dropped on the window is added too (a video dropped adds its
// folder). What is typed is the search: the list under it is what matches,
// best first. Esc empties the search bar.
//
// For now a match is by file name only (search\search.md). Meanwhile the
// indexer reads a frame a second from every video (indexer\indexer.md). How
// it is to find clips by what is in them is in
// Claude outputs\clip_search_plan.md.

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h> // printf: the log, to the console as well as its file (FR_WriteLogs)

// bgz lib
#include "bgz_inc.h"

// The UI library - the editor project's own, as the slideshow has it, copied
// in as app code and built on bgz's draw and font cache. Its file-private
// functions are marked PRIVATE, which the editor defines in its state_ops.h;
// this program has no state ops, so it is defined here, ahead of the UI.
#define PRIVATE static
#include "ui/ui_inc.h"

////////////////////////////////
//~ bgz: headers / public types

#include "text/text.h"
#include "library/library.h"
#include "video/video_decoder.h"
#include "indexer/indexer.h"
#include "search/search.h"
#include "app_state.h"
#include "frame.h"

////////////////////////////////
//~ bgz: implementations

// The UI times its passes with raddebugger's ProfScope. With no profiler
// built in, that is DeferLoop((0), ProfEnd()), and the (0) is an unused
// value. Only that warning, and only for the UI (as in the slideshow).
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunused-value"
#endif
#include "ui/ui_inc.c"
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
// The text layer as the slideshow has it. Only TXT_OpenFonts is used so far:
// the rest is there for drawing text by hand, as the slideshow does.
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunused-function"
#endif
#include "text/text.c"
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
#include "library/library.c"
// The decoder as the slideshow has it. Sound and seeking are not used yet:
// the indexer only reads each video through, picture only.
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wunused-function"
#endif
#include "video/video_ffmpeg.c" // the decoder: the one file to change for another (video_decoder.h)
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
#include "indexer/indexer.c"
#include "search/search.c"
#include "frame.c"

////////////////////////////////
//~ bgz: one frame
//
// The main loop's frame, also run by Windows while the window is being
// resized: raddebugger's window manager calls update() on every WM_SIZE and
// WM_PAINT, and update() calls frame() - which bgz.dll hands on to
// _Main_RepaintFrame (see CoreMainThreadEntry in
// src\bgz\bgz\base\base_entry_point.h). Without it the window shows a stale,
// stretched frame for as long as its edge is dragged.

// What _Main_RepaintFrame draws: it is called by bgz with no arguments. 0
// until the window is ready to be drawn into.
global App_State* g_repaint_app_state = 0;

// Set while a frame is being drawn, so a repaint asked for from inside one
// does nothing - see _Main_RepaintFrame.
global B32 g_frame_running = 0;

internal void
_Main_RunFrame(App_State* app_state) {
   g_frame_running = 1;

   Core_R_BeginFrame();
   Core_R_WindowBeginFrame(app_state->window, app_state->window_eqp);
   FrameWork(app_state);
   Core_R_WindowEndFrame(app_state->window, app_state->window_eqp);
   Core_R_EndFrame();

   g_frame_running = 0;
}

// A frame with no input, at the window's new size. Called from inside
// Core_WM_GetEvents, part way through gathering this frame's events onto the
// frame arena, so everything it puts on the frame arena is given back before
// it returns (a temp, not ArenaClear), and it sees no events.
//
// Returns whether to quit, which only raddebugger's own main loops read.
// Ours is below, so: never.
internal B32
_Main_RepaintFrame(void) {
   App_State* app_state = g_repaint_app_state;

   if (app_state != 0 && !g_frame_running) {
      // The swap chain takes no frame it was not waited for.
      Core_R_WaitForFrame();

      Temp         temp = TempBegin(app_state->frame_arena);
      WM_EventList events = app_state->events;

      MemoryZeroStruct(&app_state->events);
      _Main_RunFrame(app_state);

      app_state->events = events;
      TempEnd(temp);
   }

   return 0;
}

////////////////////////////////
//~ bgz: entry point

internal void
_EntryPoint(CmdLine* cmdline) {
   SetThreadName(Str8Lit("MainThread"));

   App_State app_state = {0};
   app_state.frame_arena = ArenaAlloc();
   app_state.results_arena = ArenaAlloc();

   // The log, before anything that can have something to say. What is
   // logged before the first frame is written just ahead of it, and after
   // that each frame writes its own (FrameWork).
   app_state.log = LogAlloc();
   LogSelect(app_state.log);
   LogScopeBegin();
   FR_OpenLogFile(&app_state);

   app_state.fonts = TXT_OpenFonts();

   // The UI layer's state, selected for the rest of the run: there is only
   // the one. The icon font is found beside the program, not from the
   // folder it was started in.
   app_state.ui_state = UI_StateAlloc();
   UI_SelectState(app_state.ui_state);
   {
      Temp    scratch = ScratchBegin(0, 0);
      String8 icon_font_path = PushStr8F(scratch.arena, "%S\\..\\data\\fonts\\Icons.ttf", GetProcessInfo()->binary_path);
      app_state.ui_icon_font = FilePathExists(icon_font_path) ? Core_FNT_TagFromPath(icon_font_path) : Core_FNT_TagZero();
      ScratchEnd(scratch);
   }

   // The videos: the folders searched last time, and any on the command line.
   app_state.library = LIB_Alloc();
   app_state.indexer = IDX_Alloc();
   FR_LoadFolders(&app_state);
   for (String8Node* node = cmdline->inputs.first; node != 0; node = node->next) {
      FR_AddFolder(&app_state, node->string);
   }

   // UseDefaultPosition: where Windows puts a new window - only the rect's
   // size is used.
   app_state.window = Core_WM_WindowOpen(R2F32P(0, 0, 1280, 720), WM_WindowFlag_UseDefaultPosition, Str8Lit("Video Search"));
   app_state.window_eqp = Core_R_WindowEquip(app_state.window);

   {
      Temp temp = TempBegin(app_state.frame_arena);
      FR_WriteLogs(&app_state, LogScopeEnd(app_state.frame_arena));
      TempEnd(temp);
   }

   // The first frame, drawn and presented while the window is still hidden,
   // so it shows with its contents rather than blank.
   g_repaint_app_state = &app_state;
   _Main_RunFrame(&app_state);

   Core_WM_WindowFirstPaint(app_state.window);
   Core_WM_WindowFocus(app_state.window);

   while (!app_state.quit) {
      Core_R_WaitForFrame(); // vsync

      app_state.events = Core_WM_GetEvents(app_state.frame_arena, 0);
      _Main_RunFrame(&app_state);

      ArenaClear(app_state.frame_arena);
   }

   g_repaint_app_state = 0;
   IDX_Stop(app_state.indexer);
   FileClose(app_state.log_file);

   Core_R_Shutdown();
}

int main(int argc, char** argv) {
   UnusedVariable(argc);
   UnusedVariable(argv);

   CoreMainThreadEntry(_EntryPoint, _Main_RepaintFrame);
   return 0;
}
