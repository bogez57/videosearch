// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

#ifndef BGZ_BASE_PROCESSES_H
#define BGZ_BASE_PROCESSES_H

////////////////////////////////
//~ bgz: raddebugger's Processes
//
// bgz's functions for raddebugger's processes (rad/base/base_processes.h).
// Each only calls raddebugger's function of the same name.

exported ProcessInfo* GetProcessInfo(void);
exported String8      GetCurrentPath(Arena* arena);

////////////////////////////////
//~ bgz: Child Processes
//
// A command run through the shell, its output read as it comes in, polled
// from the frame thread. raddebugger's processes (base/base_processes.h) are
// a different thing - launch parameters, pipes you make yourself - so these
// have their own handle.

typedef struct ChildProcess ChildProcess;
struct ChildProcess
{
  U64 u64[1];
};

typedef struct ChildProcessStatus ChildProcessStatus;
struct ChildProcessStatus
{
  B8  launch_failed;
  B8  running;
  B8  read_failed;
  B8  kill_failed;
  B8  was_killed;
  U32 exit_code;
};

exported ChildProcess       ChildProcessLaunch(String8 command, String8 working_directory);
exported void               ChildProcessRelease(ChildProcess process);
exported String8            ChildProcessReadOutput(Arena *arena, ChildProcess process);
exported void               ChildProcessKill(ChildProcess process);
exported U64                PIDFromChildProcess(ChildProcess process);
exported ChildProcessStatus StatusFromChildProcess(ChildProcess process);

////////////////////////////////
//~ bgz: Running A Tool
//
// A finite tool invocation. Strings are UTF-8; arguments exclude argv[0] and
// contain literal values, not shell syntax. Supply an executable path (there
// is no PATH search). Empty working_directory inherits the current directory.
// Input may contain arbitrary bytes. Zero timeout means no deadline.

typedef struct ProcessRunParams ProcessRunParams;
struct ProcessRunParams
{
  String8 executable;
  String8List arguments;
  String8 working_directory;
  String8 input;
  U64 timeout_us;
};

typedef struct ProcessRunResult ProcessRunResult;
struct ProcessRunResult
{
  B32 launch_failed;
  B32 io_failed;
  B32 timed_out;
  B32 exited;       // exit_code is valid, including after forced termination
  U32 exit_code;
  U32 os_error;     // first platform error; a nonzero child exit is not an OS error
  String8 output;
  String8 error;
};

// Blocking: call from a worker, never the frame thread. Params/input must live
// until return; output/error are copied onto arena after all I/O workers join.
// Success requires !launch_failed && !io_failed && !timed_out && exited &&
// exit_code == 0. On failure, captured output may be partial. The timeout also
// covers draining pipes. This owns a finite process tree: remaining
// descendants are terminated on return, so this is not a way to launch
// background services.
exported ProcessRunResult ProcessRun(Arena *arena, ProcessRunParams *params);

// The full path of a program named as on a command line: name as it is when
// it has a folder in it, else looked for on PATH (".exe" added when it has no
// extension). Empty when there is no such file.
exported String8 ProgramPathFromName(Arena *arena, String8 name);

////////////////////////////////
//~ bgz: Other Processes
//
// Whether a program is running, by its exe's file name ("raddbg.exe"), in any
// case. Any process with that name counts, whoever started it.

exported B32 ProcessIsRunning(String8 exe_name);

// Lets another program bring its window to the front - Windows only lets the
// program in front pass that on, so this works while this one is in front (it
// is, right after a key press here). Good until the next input.
exported void AllowOtherProcessToTakeFocus(void);

////////////////////////////////
//~ bgz: Crash Debugger @Added(CrashDebugger)
//
// When this process crashes, bring the RAD Debugger onto it - debugger is its
// path, or raddbg.exe on PATH. One already open is told to attach (--ipc
// attach, so it keeps its window and breakpoints); otherwise, or if it has not
// attached within 10 seconds, a new one starts with --jit_pid/--jit_code/
// --jit_addr. Once one is attached, run the faulting instruction again, so the debugger stops
// on it with the whole call stack. The RAD Debugger first stops where it
// attached - in ntdll, this filter waiting, the crash a few frames down the
// call stack - and stops on the crash itself once continued. Empty turns it
// off. Called again, it only changes the path. With no path, a debugger
// already attached, or one that does not attach within a minute, the crash
// goes on to the filter that was there before (raddebugger's dialog, or
// --quiet's silent exit).
exported void CrashDebuggerSet(String8 debugger);

////////////////////////////////
//~ bgz: Crash Report @Added(CrashReport)
//
// When this process crashes, write report_dir\crash_<date>_<time>.txt first -
// before the crash debugger above, whether one is set or not - for reading
// without a debugger: the exception, where (function, file:line), the thread,
// its stack use, when the exe and core.dll were built and whether their pdbs
// were found, project (any text), the call stack, and the last 50 lines of
// report_dir\editor_log.txt. Then report_dir\last_crash.txt holds the report's
// file name, for the next start to find, and crash_<date>_<time>.dmp beside
// it is a minidump of a few MB (every thread's stack, memory near the pointers
// on the stacks) for opening in a debugger. Written by
// a thread of its own, so a stack overflow is reported too; the crash waits up
// to 30 seconds for it.
// Called again, it only changes the folder and the project.
// Not while a debugger is attached from the start: that takes the crash first.
exported void CrashReportSet(String8 report_dir, String8 project);

// What the program adds to the end of a crash report, from its own state: up
// to size bytes of text into buffer, the count written returned. Called on the
// report thread after the report and the dump are written - so it must not
// allocate from anything the crash may have broken, and if it faults, only
// its own part is lost. 0 takes it away.
typedef U64 CrashReportExtraFunction(char* buffer, U64 size);
exported void CrashReportSetExtra(CrashReportExtraFunction* function);

#endif // BGZ_BASE_PROCESSES_H
