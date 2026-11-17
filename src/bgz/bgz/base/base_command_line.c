////////////////////////////////
//~ bgz: Command Line

exported String8
CmdLineString(CmdLine* cmd_line, String8 name) {
   String8 result = cmd_line_string(cmd_line, name);
   return result;
}

exported B32
CmdLineHasFlag(CmdLine* cmd_line, String8 name) {
   B32 result = cmd_line_has_flag(cmd_line, name);
   return result;
}
