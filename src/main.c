////////////////////////////////
//~ bgz: videosearch
//
// The program's one unit: bgz's headers, then each layer's header, then each
// layer's .c, in dependency order. bgz itself is not compiled here - it is
// bgz.dll (src\bgz\build.bat), linked through bin\bgz.lib.
//
//    bin\videosearch.exe                the folders of videos searched last time
//    bin\videosearch.exe <folder> ...   adds those folders to them
//    bin\videosearch.exe --cpu          runs the models on the processor, not the graphics card
//
// Folders are added and taken out in the Folders panel (the button beside
// the search bar; it is up by itself when there are none). A folder dropped
// on the window is added too (a video dropped adds its folder). What is typed is the search: the list under it is what matches,
// best first. Esc empties the search bar. A click on a hit plays its clip
// in the window: Space pauses, Left and Right go 5 seconds, Esc goes back.
//
// The indexer runs a frame a second of every video through CLIP's image
// model and keeps what it makes of them on disk (indexer\indexer.md); a
// search puts what is typed through the text model and finds the frames
// that match it best (search\search.md). The videos' sound goes the same
// way through a second model, CLAP, if it is there. Where it is going is in
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
#include "picker/picker.h"
#include "audio/audio.h"
#include "audio/audio_device.h"
#include "video/video.h"
#include "video/video_decoder.h"
#include "embed/embed.h"
#include "index/index.h"
#include "indexer/indexer.h"
#include "search/search.h"
#include "thumbs/thumbs.h"
#include "app_state.h"
#include "frame.h"

////////////////////////////////
//~ bgz: implementations

#include "ui/ui_inc.c"
#include "text/text.c"
#include "library/library.c"
#include "picker/picker_win32.c" // the folder window: the one file that knows Windows' is there (picker.h)
#include "audio/audio.c"
#include "audio/audio_wasapi.c" // the device: the one file to change for another (audio_device.h)
#include "video/video.c"
#include "video/video_ffmpeg.c" // the decoder: the one file to change for another (video_decoder.h)
#include "embed/embed.c"
#include "embed/embed_tokenizer.c"
#include "embed/embed_sound.c"
#include "embed/embed_sound_tokenizer.c"
#include "embed/embed_onnx.c" // the model: the one file to change to run it with something else (embed.h)
#include "index/index.c"
#include "indexer/indexer.c"
#include "search/search.c"
#include "thumbs/thumbs.c"
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
   LogInfoF("Video Search started\n");

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

   // Sound, before any clip is played: a video takes a voice as it opens.
   AUD_Init();
   app_state.thumbs = THM_Alloc();
   app_state.play_arena = ArenaAlloc();

   // The videos: the folders searched last time, and any on the command line.
   app_state.library = LIB_Alloc();
   {
      Temp    scratch = ScratchBegin(0, 0);
      String8 data = PushStr8F(scratch.arena, "%S\\..\\data", GetProcessInfo()->binary_path);
      String8 model_name = Str8Lit("clip-vit-base-patch32");
      app_state.model = EMB_Open(PushStr8F(scratch.arena, "%S\\models\\%S", data, model_name),
                                 PushStr8F(scratch.arena, "%S\\clip\\bpe_merges.txt", data),
                                 !CmdLineHasFlag(cmdline, Str8Lit("cpu")));
      String8 sound_model_name = Str8Lit("clap-htsat-unfused");
      EMB_OpenSound(app_state.model, PushStr8F(scratch.arena, "%S\\models\\%S", data, sound_model_name),
                    PushStr8F(scratch.arena, "%S\\clap\\vocab.json", data),
                    PushStr8F(scratch.arena, "%S\\clap\\merges.txt", data));
      String8 index_folder = PushStr8F(scratch.arena, "%S\\index", FR_LocalFolder(scratch.arena));
      app_state.indexer = IDX_Alloc(index_folder, model_name, sound_model_name, app_state.model);
      app_state.search = SRCH_Alloc(index_folder, app_state.model);
      ScratchEnd(scratch);
   }
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
   VID_Close(app_state.play_video);
   AUD_Shutdown();
   THM_Stop(app_state.thumbs);
   IDX_Stop(app_state.indexer);
   EMB_Close(app_state.model);
   FileClose(app_state.log_file);

   Core_R_Shutdown();
}

int main(int argc, char** argv) {
   UnusedVariable(argc);
   UnusedVariable(argv);

   CoreMainThreadEntry(_EntryPoint, _Main_RepaintFrame);
   return 0;
}
