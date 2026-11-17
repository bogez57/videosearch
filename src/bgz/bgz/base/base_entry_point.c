////////////////////////////////
//~ bgz: Entry Point
//
// Starts render, the font provider and the font cache, which sit above base:
// this file is compiled after their headers (bgz/build.c).

global CORE_EntryPointFunctionType* core_app_entry_point = 0;
global CORE_FrameFunctionType*      core_app_frame = 0;

// Called by raddebugger's main_thread_base_entry_point once its layers are
// up (rad/base/base_entry_point.c).
internal void
entry_point(CmdLine* cmdline) {
   // The main thread's room for the crash filter - every other thread gets it
   // from ThreadLaunch (base_threads.c).
   CrashDebuggerThreadBegin();

   // Started here, not by raddebugger's entry point (see bgz/build.c).
   if (core_app_frame != 0) {
      Core_R_Init(cmdline);
      Core_FP_Init();
      Core_FNT_Init();
   }
   AsyncLanesBegin(cmdline);
   core_app_entry_point(cmdline);
   AsyncLanesEnd();
}

// Called by raddebugger's update() (rad/base/base_entry_point.c), which its
// window manager calls on WM_SIZE and WM_PAINT.
internal B32
frame(void) {
   B32 result = 0;
   if (core_app_frame != 0) {
      result = core_app_frame();
   }
   return result;
}

exported void
CoreMainThreadEntry(CORE_EntryPointFunctionType* entry, CORE_FrameFunctionType* frame) {
   core_app_entry_point = entry;
   core_app_frame = frame;
#if OS_WINDOWS
   // What raddebugger's wWinMain does (rad/win32/base/win32_base.c), from a dll:
   // the arguments come from Windows rather than from the C runtime's startup,
   // which is the app's.
   int argc = 0;
   WCHAR** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
   CoInitializeEx(0, COINIT_APARTMENTTHREADED);
   w32_entry_point_caller(argc, argv);
   LocalFree(argv);
#else
# error CoreMainThreadEntry not implemented for this OS.
#endif
}
