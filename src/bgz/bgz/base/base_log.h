////////////////////////////////
//~ bgz: Log
//
// bgz's functions for raddebugger's log (rad/base/base_log.h). Each only
// calls raddebugger's function of the same name.

#ifndef BGZ_BASE_LOG_H
#define BGZ_BASE_LOG_H

exported Log*           LogAlloc(void);
exported void           LogSelect(Log* log);
exported void           LogMsg(LogMsgKind kind, String8 string);
exported void           LogScopeBegin(void);
exported LogScopeResult LogScopeEnd(Arena* arena);

// Functions here, macros in raddebugger.
exported void           LogInfo(String8 string);
exported void           LogInfoF(char* fmt, ...);
exported void           LogUserError(String8 string);
exported void           LogUserErrorF(char* fmt, ...);

// Something the user should know but nothing failed - a change that needs a
// restart. Shown like a user error (the editor's diagnostics layer), in a
// warning colour.
exported void           LogUserWarning(String8 string);
exported void           LogUserWarningF(char* fmt, ...);

// raddebugger's own names, redefined: raddebugger's versions call log_infof.
#undef LogInfoNamedBlock
#undef LogInfoNamedBlockF
#define LogInfoNamedBlock(s)    DeferLoop(LogInfoF("%S:\n{\n", (s)), LogInfoF("}\n"))
#define LogInfoNamedBlockF(...) DeferLoop((LogInfoF(__VA_ARGS__), LogInfoF(":\n{\n")), LogInfoF("}\n"))

#endif // BGZ_BASE_LOG_H
