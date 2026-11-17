////////////////////////////////
//~ bgz: Command Line
//
// bgz's functions for raddebugger's command line (rad/base/
// base_command_line.h). Each only calls raddebugger's function of the same
// name.

#ifndef BGZ_BASE_COMMAND_LINE_H
#define BGZ_BASE_COMMAND_LINE_H

exported String8 CmdLineString(CmdLine* cmd_line, String8 name);
exported B32     CmdLineHasFlag(CmdLine* cmd_line, String8 name);

#endif // BGZ_BASE_COMMAND_LINE_H
