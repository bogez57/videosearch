// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

#ifndef WIN32_BGZ_BASE_H
#define WIN32_BGZ_BASE_H

////////////////////////////////
//~ bgz: Child Processes

typedef struct W32_Process W32_Process;
struct W32_Process
{
  W32_Process *next;
  HANDLE parent_read;
  PROCESS_INFORMATION info;
  ChildProcessStatus status;
};

// One owner per pipe and arena. Only stop is shared while the worker runs;
// chunks and error_code are read by the caller after joining the thread.
typedef struct W32_ProcessPipe W32_ProcessPipe;
struct W32_ProcessPipe
{
  HANDLE handle;
  HANDLE thread;
  Arena *arena;
  String8 input;
  String8List chunks;
  B32 writing;
  volatile LONG stop;
  DWORD error_code;
};

#endif // WIN32_BGZ_BASE_H
