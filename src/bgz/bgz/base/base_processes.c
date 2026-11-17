////////////////////////////////
//~ bgz: raddebugger's Processes
//
// bgz's own process functions are per OS, in win32_base.c.

exported ProcessInfo*
GetProcessInfo(void) {
   ProcessInfo* result = get_process_info();
   return result;
}

exported String8
GetCurrentPath(Arena* arena) {
   String8 result = get_current_path(arena);
   return result;
}
