////////////////////////////////
//~ bgz: Log

exported Log*
LogAlloc(void) {
   Log* result = log_alloc();
   return result;
}

exported void
LogSelect(Log* log) {
   log_select(log);
}

exported void
LogMsg(LogMsgKind kind, String8 string) {
   log_msg(kind, string);
}

exported void
LogScopeBegin(void) {
   log_scope_begin();
}

exported LogScopeResult
LogScopeEnd(Arena* arena) {
   LogScopeResult result = log_scope_end(arena);
   return result;
}

exported void
LogInfo(String8 string) {
   log_info(string);
}

exported void
LogInfoF(char* fmt, ...) {
   Temp    scratch = scratch_begin(0, 0);
   va_list args;
   va_start(args, fmt);
   String8 string = str8fv(scratch.arena, fmt, args);
   va_end(args);
   log_info(string);
   scratch_end(scratch);
}

exported void
LogUserError(String8 string) {
   log_user_error(string);
}

exported void
LogUserErrorF(char* fmt, ...) {
   Temp    scratch = scratch_begin(0, 0);
   va_list args;
   va_start(args, fmt);
   String8 string = str8fv(scratch.arena, fmt, args);
   va_end(args);
   log_user_error(string);
   scratch_end(scratch);
}

exported void
LogUserWarning(String8 string) {
   log_user_warning(string);
}

exported void
LogUserWarningF(char* fmt, ...) {
   Temp    scratch = scratch_begin(0, 0);
   va_list args;
   va_start(args, fmt);
   String8 string = str8fv(scratch.arena, fmt, args);
   va_end(args);
   log_user_warning(string);
   scratch_end(scratch);
}
