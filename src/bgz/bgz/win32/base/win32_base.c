// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

////////////////////////////////
//~ bgz: Child Process Storage
//
// raddebugger's win32 base keeps its own entities for its own handles; these
// are bgz's, for bgz's child processes, with a free list of their own.

global SRWLOCK      w32_bgz_process_srw_lock = SRWLOCK_INIT;
global Arena       *w32_bgz_process_arena = 0;
global W32_Process *w32_bgz_first_free_process = 0;

internal W32_Process*
W32_ProcessAlloc(void) {
   W32_Process* result = 0;
   AcquireSRWLockExclusive(&w32_bgz_process_srw_lock);
   {
      if (w32_bgz_process_arena == 0) {
         w32_bgz_process_arena = arena_alloc();
      }
      result = w32_bgz_first_free_process;
      if (result != 0) {
         SLLStackPop(w32_bgz_first_free_process);
      }
      else {
         result = push_array_no_zero(w32_bgz_process_arena, W32_Process, 1);
      }
      MemoryZeroStruct(result);
   }
   ReleaseSRWLockExclusive(&w32_bgz_process_srw_lock);
   return result;
}

internal void
W32_ProcessRelease(W32_Process* process) {
   AcquireSRWLockExclusive(&w32_bgz_process_srw_lock);
   {
      SLLStackPush(w32_bgz_first_free_process, process);
   }
   ReleaseSRWLockExclusive(&w32_bgz_process_srw_lock);
}

//~ bgz: directory watching @Added(FileWatch)
//
// ReadDirectoryChangesW, overlapped, and polled rather than waited on: every
// poll asks whether the outstanding request has completed yet without
// blocking, and if it has, reads the results and issues the next one. No
// thread, no callback - changes are picked up once per poll, on the caller's
// thread.
//
// Between requests the OS keeps collecting changes for the handle, so nothing
// is missed while the caller is busy - unless more arrives than fits the
// buffer, which comes back as a completion with zero bytes: "overflowed".

struct FileWatch {
   HANDLE     dir;
   OVERLAPPED overlapped;
   B32        recursive;
   B32        pending; // a request is outstanding
   String8    dir_path;

   // DWORD-aligned, as ReadDirectoryChangesW requires. 64 KB is also the most
   // a request over the network may use.
   DWORD buffer[16 * 1024];
};

static void W32_FileWatchIssue(FileWatch* watch) {
   DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                  FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE;

   ResetEvent(watch->overlapped.hEvent);
   watch->pending = !!ReadDirectoryChangesW(watch->dir, watch->buffer, sizeof(watch->buffer), watch->recursive, filter, 0, &watch->overlapped, 0);
}

exported FileWatch*
FileWatchOpen(Arena* arena, String8 dir_path, B32 recursive) {
   FileWatch* result = 0;
   Temp          scratch = scratch_begin(&arena, 1);
   String16      dir_path_16 = str16_from_8(scratch.arena, dir_path);

   // Every share flag, so watching a directory never stops anyone renaming or
   // deleting things in it - or the directory itself.
   HANDLE dir = CreateFileW((WCHAR*)dir_path_16.str,
                            FILE_LIST_DIRECTORY,
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            0,
                            OPEN_EXISTING,
                            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, // a directory, asynchronously
                            0);

   if (dir != INVALID_HANDLE_VALUE) {
      result = push_array(arena, FileWatch, 1);
      result->dir = dir;
      result->recursive = recursive;
      result->dir_path = push_str8_copy(arena, dir_path);
      result->overlapped.hEvent = CreateEventW(0, TRUE, FALSE, 0);

      W32_FileWatchIssue(result);
   }

   scratch_end(scratch);
   return result;
}

exported FileChanges
FileWatchPoll(Arena* arena, FileWatch* watch) {
   FileChanges result = {0};

   if (watch != 0) {
      if (!watch->pending) {
         // The last request could not be issued, so changes may have gone
         // unseen. Try again, and have the caller rescan to be safe.
         W32_FileWatchIssue(watch);
         result.overflowed = 1;
      }
      else {
         DWORD bytes = 0;

         if (GetOverlappedResult(watch->dir, &watch->overlapped, &bytes, FALSE)) {
            if (bytes == 0) {
               result.overflowed = 1;
            }
            else {
               // Read everything out BEFORE issuing the next request, which
               // reuses the buffer.
               U8* at = (U8*)watch->buffer;

               for (;;) {
                  FILE_NOTIFY_INFORMATION* info = (FILE_NOTIFY_INFORMATION*)at;

                  String16 name_16 = str16((U16*)info->FileName, info->FileNameLength / sizeof(WCHAR));
                  String8  name = str8_from_16(arena, name_16);

                  str8_list_push(arena, &result.paths, push_str8f(arena, "%S/%S", watch->dir_path, name));

                  if (info->NextEntryOffset == 0) {
                     break;
                  }

                  at += info->NextEntryOffset;
               }
            }

            W32_FileWatchIssue(watch);
         }
         else if (GetLastError() != ERROR_IO_INCOMPLETE) {
            // The request failed rather than just not being done yet.
            W32_FileWatchIssue(watch);
            result.overflowed = 1;
         }
      }
   }

   return result;
}

exported void
FileWatchClose(FileWatch* watch) {
   if (watch != 0) {
      if (watch->pending) {
         // The OS still owns the buffer while a request is outstanding. Cancel
         // it and wait for the cancel to land before the memory can go.
         DWORD bytes = 0;
         CancelIoEx(watch->dir, &watch->overlapped);
         GetOverlappedResult(watch->dir, &watch->overlapped, &bytes, TRUE);
         watch->pending = 0;
      }

      CloseHandle(watch->overlapped.hEvent);
      CloseHandle(watch->dir);
   }
}

////////////////////////////////////////////////////////////////
//~ rjf: Child Processes

exported ChildProcess
ChildProcessLaunch(String8 command, String8 working_directory) {
   W32_Process* w32_proc = W32_ProcessAlloc();

   Temp scratch = scratch_begin(0, 0);

   //- rjf: convert inputs
   String8  command_prepped = push_str8f(scratch.arena, "cmd.exe /C %.*s", str8_varg(command));
   String16 command16 = str16_from_8(scratch.arena, command_prepped);
   String16 dir16 = str16_from_8(scratch.arena, working_directory);

   //- rjf: make security attributes
   SECURITY_ATTRIBUTES security = {.nLength = sizeof(security)};
   security.bInheritHandle = TRUE;

   //- rjf: setup intermediates
   HANDLE child_write = INVALID_HANDLE_VALUE;
   HANDLE parent_read = INVALID_HANDLE_VALUE;
   HANDLE child_read = INVALID_HANDLE_VALUE;

   B32 success = 0;

   //- rjf: pipe
   if (CreatePipe(&parent_read, &child_write, &security, 0)) {
      if (SetHandleInformation(parent_read, HANDLE_FLAG_INHERIT, 0)) {

         //- rjf: nul file (child stdin)
         child_read = CreateFileA("nul",
                                  GENERIC_READ,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  &security,
                                  OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL,
                                  0);

         //- rjf: process
         STARTUPINFOW startup = {.cb = sizeof(startup)};
         startup.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
         startup.hStdInput = child_read;
         startup.hStdOutput = child_write;
         startup.hStdError = child_write;
         startup.wShowWindow = SW_HIDE;
         if (CreateProcessW(L"c:\\windows\\system32\\cmd.exe",
                            (WCHAR*)command16.str,
                            0, 0, TRUE, 0,
                            0,
                            (WCHAR*)dir16.str,
                            &startup,
                            &w32_proc->info)) {
            success = 1;
            w32_proc->status.running = 1;
            w32_proc->parent_read = parent_read;

            CloseHandle(child_write);
            child_write = INVALID_HANDLE_VALUE;

            CloseHandle(child_read);
            child_read = INVALID_HANDLE_VALUE;
         }
      }
   }

   if (!success) {
      w32_proc->status.launch_failed = 1;

      if (child_write != INVALID_HANDLE_VALUE) {
         CloseHandle(child_write);
      }
      if (parent_read != INVALID_HANDLE_VALUE) {
         CloseHandle(parent_read);
      }
      if (child_read != INVALID_HANDLE_VALUE) {
         CloseHandle(child_read);
      }
   }

   scratch_end(scratch);

   ChildProcess result = {0};
   result.u64[0] = IntFromPtr(w32_proc);
   return result;
}

// @Added(ProcessRun)
// Quote each literal argument using the Windows C runtime rules. Backslashes
// are doubled only before a quote (including the closing quote), so paths,
// empty arguments and embedded quotes all survive the command-line round trip.
internal String8
W32_ProcessQuoteArgument(Arena* arena, String8 argument) {
   String8 result = {.str = push_array_no_zero(arena, U8, 2 * argument.size + 3)};
   result.str[result.size++] = '"';
   U64 slashes = 0;
   for (U64 i = 0; i <= argument.size; i += 1) {
      if (i < argument.size && argument.str[i] == '\\') {
         slashes += 1;
         continue;
      }
      B32 quote = i == argument.size || argument.str[i] == '"';
      U64 count = quote ? 2 * slashes : slashes;
      for (U64 j = 0; j < count; j += 1) result.str[result.size++] = '\\';
      slashes = 0;
      if (i == argument.size) break;
      if (quote) result.str[result.size++] = '\\';
      result.str[result.size++] = argument.str[i];
   }
   result.str[result.size++] = '"';
   result.str[result.size] = 0;
   return result;
}

internal DWORD WINAPI
W32_ProcessPipeThread(void* data) {
   W32_ProcessPipe* pipe = (W32_ProcessPipe*)data;
   U8 buffer[16384];
   U64 offset = 0;
   while (InterlockedCompareExchange(&pipe->stop, 0, 0) == 0) {
      DWORD amount = 0;
      if (pipe->writing) {
         if (offset == pipe->input.size) break;
         DWORD size = (DWORD)Min(pipe->input.size - offset, sizeof(buffer));
         if (!WriteFile(pipe->handle, pipe->input.str + offset, size, &amount, 0)) {
            pipe->error_code = GetLastError();
            break;
         }
         if (amount == 0) {
            pipe->error_code = ERROR_WRITE_FAULT;
            break;
         }
         offset += amount;
      }
      else {
         if (!ReadFile(pipe->handle, buffer, sizeof(buffer), &amount, 0)) {
            DWORD error_code = GetLastError();
            if (error_code != ERROR_BROKEN_PIPE) pipe->error_code = error_code;
            break;
         }
         if (amount == 0) break;
         String8 chunk = push_str8_copy(pipe->arena, str8(buffer, amount));
         str8_list_push(pipe->arena, &pipe->chunks, chunk);
      }
   }
   // In particular, closing the writer is how the child learns stdin is done.
   CloseHandle(pipe->handle);
   return 0;
}

exported ProcessRunResult
ProcessRun(Arena* arena, ProcessRunParams* params) {
   ProcessRunResult result = {0};
   result.launch_failed = 1;
   Temp scratch = scratch_begin(&arena, 1);
   W32_ProcessPipe pipes[3] = {0}; // stdin, stdout, stderr
   HANDLE child_ends[3] = {0};
   PROCESS_INFORMATION info = {0};
   STARTUPINFOEXW startup = {0};
   HANDLE job = 0;
   B32 attributes_ready = 0;
   B32 completed = 0;
   U64 started_ms = GetTickCount64();
   U64 timeout_ms = params->timeout_us / 1000 + (params->timeout_us % 1000 != 0);

   do {
      // Windows strings cannot contain NUL. Reject them rather than launching
      // a different executable or silently truncating one of its arguments.
      String8List values = {0};
      str8_list_push(scratch.arena, &values, params->executable);
      str8_list_push(scratch.arena, &values, params->working_directory);
      for (String8Node* n = params->arguments.first; n != 0; n = n->next) {
         str8_list_push(scratch.arena, &values, n->string);
      }
      B32 invalid = params->executable.size == 0;
      for (String8Node* n = values.first; n != 0; n = n->next) {
         for (U64 i = 0; i < n->string.size; i += 1) {
            if (n->string.str[i] == 0) invalid = 1;
         }
      }
      if (invalid) {
         result.os_error = ERROR_INVALID_PARAMETER;
         break;
      }

      String8List command_parts = {0};
      str8_list_push(scratch.arena, &command_parts, W32_ProcessQuoteArgument(scratch.arena, params->executable));
      for (String8Node* n = params->arguments.first; n != 0; n = n->next) {
         str8_list_push(scratch.arena, &command_parts, str8_lit(" "));
         str8_list_push(scratch.arena, &command_parts, W32_ProcessQuoteArgument(scratch.arena, n->string));
      }
      String16 command = str16_from_8(scratch.arena, str8_list_join(scratch.arena, &command_parts, 0));
      String16 executable = str16_from_8(scratch.arena, params->executable);
      String16 directory = str16_from_8(scratch.arena, params->working_directory);
      SECURITY_ATTRIBUTES security = {sizeof(security), 0, TRUE};
      B32 pipes_ready = 1;
      for (U32 i = 0; i < 3; i += 1) {
         pipes[i].writing = i == 0;
         pipes[i].input = params->input;
         if (i != 0) pipes[i].arena = arena_alloc();
         HANDLE* read_end = i == 0 ? &child_ends[i] : &pipes[i].handle;
         HANDLE* write_end = i == 0 ? &pipes[i].handle : &child_ends[i];
         if (!CreatePipe(read_end, write_end, &security, 0) ||
             !SetHandleInformation(pipes[i].handle, HANDLE_FLAG_INHERIT, 0)) {
            result.os_error = GetLastError();
            pipes_ready = 0;
            break;
         }
      }
      if (!pipes_ready) break;

      SIZE_T attributes_size = 0;
      InitializeProcThreadAttributeList(0, 1, 0, &attributes_size);
      startup.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)push_array_no_zero(scratch.arena, U8, attributes_size);
      if (!InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &attributes_size)) {
         result.os_error = GetLastError();
         break;
      }
      attributes_ready = 1;
      if (!UpdateProcThreadAttribute(startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                     child_ends, sizeof(child_ends), 0, 0)) {
         result.os_error = GetLastError();
         break;
      }
      startup.StartupInfo.cb = sizeof(startup);
      startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
      startup.StartupInfo.wShowWindow = SW_HIDE;
      startup.StartupInfo.hStdInput = child_ends[0];
      startup.StartupInfo.hStdOutput = child_ends[1];
      startup.StartupInfo.hStdError = child_ends[2];

      // Assign the child while suspended: otherwise it could spawn descendants
      // before entering the job, leaving inherited pipes open after a timeout.
      job = CreateJobObjectW(0, 0);
      JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {0};
      limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
      if (job == 0 || !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
         result.os_error = GetLastError();
         break;
      }
      if (!CreateProcessW((WCHAR*)executable.str, (WCHAR*)command.str, 0, 0, TRUE,
                          EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW | CREATE_SUSPENDED,
                          0, params->working_directory.size ? (WCHAR*)directory.str : 0,
                          &startup.StartupInfo, &info)) {
         result.os_error = GetLastError();
         break;
      }
      for (U32 i = 0; i < 3; i += 1) {
         CloseHandle(child_ends[i]);
         child_ends[i] = 0;
      }
      if (!AssignProcessToJobObject(job, info.hProcess)) {
         result.os_error = GetLastError();
         break;
      }
      B32 threads_ready = 1;
      for (U32 i = 0; i < 3; i += 1) {
         pipes[i].thread = CreateThread(0, 0, W32_ProcessPipeThread, &pipes[i], 0, 0);
         if (pipes[i].thread == 0) {
            result.os_error = GetLastError();
            threads_ready = 0;
            break;
         }
      }
      if (!threads_ready) break;
      if (ResumeThread(info.hThread) == (DWORD)-1) {
         result.os_error = GetLastError();
         break;
      }
      result.launch_failed = 0;

      HANDLE waits[4] = {info.hProcess, pipes[0].thread, pipes[1].thread, pipes[2].thread};
      for (;;) {
         DWORD wait = WaitForMultipleObjects(4, waits, TRUE, 10);
         if (wait == WAIT_OBJECT_0) {
            completed = 1;
            break;
         }
         if (wait == WAIT_FAILED) {
            result.io_failed = 1;
            result.os_error = GetLastError();
            break;
         }
         // Read worker state only once the corresponding thread has finished.
         for (U32 i = 0; i < 3; i += 1) {
            if (WaitForSingleObject(pipes[i].thread, 0) == WAIT_OBJECT_0 && pipes[i].error_code != 0) {
               result.io_failed = 1;
               if (result.os_error == 0) result.os_error = pipes[i].error_code;
            }
         }
         if (result.io_failed) break;
         if (timeout_ms != 0 && GetTickCount64() - started_ms >= timeout_ms) {
            result.timed_out = 1;
            break;
         }
      }
   } while (0);

   if (!completed) {
      for (U32 i = 0; i < 3; i += 1) InterlockedExchange(&pipes[i].stop, 1);
      if (job != 0) TerminateJobObject(job, 1);
      // Also covers a child whose job assignment failed while it was suspended.
      if (info.hProcess != 0) TerminateProcess(info.hProcess, 1);
   }
   for (U32 i = 0; i < 3; i += 1) {
      if (child_ends[i] != 0) CloseHandle(child_ends[i]);
      if (pipes[i].thread != 0) {
         // Cancellation can race the worker entering ReadFile/WriteFile. Retry
         // until joined; the stop flag prevents another iteration afterward.
         while (WaitForSingleObject(pipes[i].thread, 10) == WAIT_TIMEOUT) {
            if (!completed) CancelSynchronousIo(pipes[i].thread);
         }
         CloseHandle(pipes[i].thread);
         if (pipes[i].error_code != 0 && !result.timed_out && !result.launch_failed) {
            result.io_failed = 1;
            if (result.os_error == 0) result.os_error = pipes[i].error_code;
         }
      }
      else if (pipes[i].handle != 0) CloseHandle(pipes[i].handle);
   }
   if (info.hProcess != 0) {
      WaitForSingleObject(info.hProcess, INFINITE);
      DWORD exit_code = 0;
      if (GetExitCodeProcess(info.hProcess, &exit_code)) {
         result.exited = 1;
         result.exit_code = exit_code;
      }
      else {
         result.io_failed = 1;
         if (result.os_error == 0) result.os_error = GetLastError();
      }
      CloseHandle(info.hProcess);
      CloseHandle(info.hThread);
   }
   if (job != 0) CloseHandle(job);
   if (attributes_ready) DeleteProcThreadAttributeList(startup.lpAttributeList);
   result.output = str8_list_join(arena, &pipes[1].chunks, 0);
   result.error = str8_list_join(arena, &pipes[2].chunks, 0);
   for (U32 i = 1; i < 3; i += 1) {
      if (pipes[i].arena != 0) arena_release(pipes[i].arena);
   }
   scratch_end(scratch);
   return result;
}

exported void
ChildProcessRelease(ChildProcess handle) {
   W32_Process* w32_proc = (W32_Process*)PtrFromInt(handle.u64[0]);
   if (!w32_proc->status.launch_failed) {
      CloseHandle(w32_proc->info.hProcess);
      CloseHandle(w32_proc->info.hThread);
      CloseHandle(w32_proc->parent_read);
   }
   W32_ProcessRelease(w32_proc);
}

exported String8
ChildProcessReadOutput(Arena* arena, ChildProcess process) {
   W32_Process* w32_proc = (W32_Process*)PtrFromInt(process.u64[0]);

   //- rjf: if the process has exited, save the exit code.
   B32 proc_exit = 0;
   if (w32_proc->status.running) {
      if (WaitForSingleObject(w32_proc->info.hProcess, 0) == WAIT_OBJECT_0) {
         proc_exit = 1;
         DWORD exit_code = 0;
         if (GetExitCodeProcess(w32_proc->info.hProcess, &exit_code)) {
            w32_proc->status.exit_code = exit_code;
         }
      }
   }

   //- rjf: read anything that is ready on the pipe.
   String8 result = {0};
   B32     success = 0;
   if (w32_proc->status.running &&
       !w32_proc->status.launch_failed &&
       !w32_proc->status.read_failed &&
       !w32_proc->status.kill_failed) {
      DWORD read_size = 0;
      if (PeekNamedPipe(w32_proc->parent_read, 0, 0, 0, &read_size, 0)) {
         if (read_size == 0) {
            success = 1;
         }
         else {
            result.str = push_array_no_zero(arena, U8, read_size);
            result.size = read_size;
            DWORD read_amount = 0;
            if (ReadFile(w32_proc->parent_read, result.str, read_size, &read_amount, 0)) {
               if (read_amount == read_size) {
                  success = 1;
               }
            }
         }
      }
   }

   //- rjf: mark failure.
   if (!success) {
      w32_proc->status.read_failed = 1;
   }

   //- rjf: mark exits.
   if (proc_exit) {
      w32_proc->status.running = 0;
   }

   return result;
}

exported void
ChildProcessKill(ChildProcess process) {
   W32_Process* w32_proc = (W32_Process*)PtrFromInt(process.u64[0]);
   B32             success = 0;
   if (w32_proc->status.running) {
      if (TerminateProcess(w32_proc->info.hProcess, 0)) {
         success = 1;
      }
   }
   if (!success) {
      w32_proc->status.kill_failed = 1;
   }
   else {
      w32_proc->status.running = 0;
      w32_proc->status.was_killed = 1;
   }
}

exported U64
PIDFromChildProcess(ChildProcess process) {
   W32_Process* w32_proc = (W32_Process*)PtrFromInt(process.u64[0]);
   U64             result = w32_proc->info.dwProcessId;
   return result;
}

exported ChildProcessStatus
StatusFromChildProcess(ChildProcess process) {
   W32_Process*  w32_proc = (W32_Process*)PtrFromInt(process.u64[0]);
   ChildProcessStatus result = w32_proc->status;
   return result;
}

////////////////////////////////////////////////////////////////
//~ bgz: Other Processes

exported B32
ProcessIsRunning(String8 exe_name) {
   B32    result = 0;
   HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

   if (snapshot != INVALID_HANDLE_VALUE) {
      Temp           scratch = scratch_begin(0, 0);
      PROCESSENTRY32W entry = {.dwSize = sizeof(entry)};

      for (BOOL more = Process32FirstW(snapshot, &entry); more && !result; more = Process32NextW(snapshot, &entry)) {
         String8 name = str8_from_16(scratch.arena, str16_cstring((U16*)entry.szExeFile));
         result = str8_match(name, exe_name, StringMatchFlag_CaseInsensitive);
      }

      scratch_end(scratch);
      CloseHandle(snapshot);
   }

   return result;
}

exported String8
ProgramPathFromName(Arena* arena, String8 name) {
   String8 result = {0};
   Temp    scratch = scratch_begin(&arena, 1);
   String16 name16 = str16_from_8(scratch.arena, name);
   WCHAR    found[MAX_PATH * 4];
   DWORD    size = name.size != 0 ? SearchPathW(0, (WCHAR*)name16.str, L".exe", ArrayCount(found), found, 0) : 0;

   if (size != 0 && size < ArrayCount(found)) {
      result = str8_from_16(arena, str16_cstring((U16*)found));
   }

   scratch_end(scratch);
   return result;
}

exported void
AllowOtherProcessToTakeFocus(void) {
   AllowSetForegroundWindow(ASFW_ANY);
}

////////////////////////////////////////////////////////////////
//~ bgz: Named Pipes
//
// Overlapped, so every wait can also end on a stop event or a timeout: a
// blocking ConnectNamedPipe or ReadFile could only be ended by closing the
// handle from another thread, which is not safe.

#define W32_PIPE_CHUNK_SIZE        KB(64)
#define W32_PIPE_SERVER_TIMEOUT_MS 5000 // for a client that connects and goes quiet

struct PipeServer {
   HANDLE pipe;
   HANDLE io_event;   // manual reset, for the one call in flight
   HANDLE stop_event; // manual reset; set by PipeServerStop
   B32    connected;
};

// One read or write on pipe, waited for until it completes, stop_event is set
// (0: none) or timeout_ms passes (0: no limit). A cut short call is cancelled and
// waited out, so buffer is free again on return. ERROR_MORE_DATA - a message
// longer than buffer - comes back as failure with *out_error set, and the
// bytes read.
internal B32
W32_PipeIO(HANDLE pipe, B32 write, void* buffer, DWORD size, HANDLE io_event, HANDLE stop_event, U64 timeout_ms, DWORD* out_bytes, DWORD* out_error) {
   OVERLAPPED overlapped = {0};
   overlapped.hEvent = io_event;
   ResetEvent(io_event);

   BOOL  done = write ? WriteFile(pipe, buffer, size, 0, &overlapped) : ReadFile(pipe, buffer, size, 0, &overlapped);
   DWORD error = done ? 0 : GetLastError();
   DWORD bytes = 0;

   if (done || error == ERROR_IO_PENDING || error == ERROR_MORE_DATA) {
      if (error == ERROR_IO_PENDING) {
         HANDLE events[2] = {io_event, stop_event};
         DWORD  wait = WaitForMultipleObjects(stop_event != 0 ? 2 : 1, events, FALSE, timeout_ms != 0 ? (DWORD)timeout_ms : INFINITE);

         if (wait != WAIT_OBJECT_0) {
            CancelIoEx(pipe, &overlapped);
         }
      }

      done = GetOverlappedResult(pipe, &overlapped, &bytes, TRUE);
      error = done ? 0 : GetLastError();
   }

   *out_bytes = bytes;
   *out_error = error;
   return done;
}

// One whole message, onto arena.
internal B32
W32_PipeReadMessage(Arena* arena, HANDLE pipe, HANDLE io_event, HANDLE stop_event, U64 timeout_ms, String8* out) {
   Temp        scratch = scratch_begin(&arena, 1);
   String8List chunks = {0};
   B32         result = 0;

   for (;;) {
      U8*   chunk = push_array_no_zero(scratch.arena, U8, W32_PIPE_CHUNK_SIZE);
      DWORD bytes = 0;
      DWORD error = 0;
      B32   done = W32_PipeIO(pipe, 0, chunk, (DWORD)W32_PIPE_CHUNK_SIZE, io_event, stop_event, timeout_ms, &bytes, &error);

      str8_list_push(scratch.arena, &chunks, str8(chunk, bytes));

      if (done) {
         result = 1;
         break;
      }

      if (error != ERROR_MORE_DATA || chunks.total_size > PIPE_MESSAGE_MAX) {
         break;
      }
   }

   *out = result ? str8_list_join(arena, &chunks, 0) : str8_zero();
   scratch_end(scratch);
   return result;
}

internal B32
W32_PipeWriteMessage(HANDLE pipe, String8 message, HANDLE io_event, HANDLE stop_event, U64 timeout_ms) {
   DWORD bytes = 0;
   DWORD error = 0;
   B32   result = 0;

   if (message.size <= PIPE_MESSAGE_MAX) {
      result = W32_PipeIO(pipe, 1, message.str, (DWORD)message.size, io_event, stop_event, timeout_ms, &bytes, &error) && bytes == message.size;
   }

   return result;
}

internal B32
W32_PipeServerStopped(PipeServer* server) {
   return WaitForSingleObject(server->stop_event, 0) == WAIT_OBJECT_0;
}

exported PipeServer*
PipeServerOpen(Arena* arena, String8 name) {
   PipeServer* result = 0;
   Temp        scratch = scratch_begin(&arena, 1);
   String16    path16 = str16_from_8(scratch.arena, push_str8f(scratch.arena, "\\\\.\\pipe\\%S", name));

   // FIRST_PIPE_INSTANCE: fails if another process has the name. The default
   // security lets other users connect only to read, so only this user can
   // send a request.
   HANDLE pipe = CreateNamedPipeW((WCHAR*)path16.str,
                                  PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                  PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
                                  1, (DWORD)W32_PIPE_CHUNK_SIZE, (DWORD)W32_PIPE_CHUNK_SIZE, 0, 0);

   if (pipe != INVALID_HANDLE_VALUE) {
      result = push_array(arena, PipeServer, 1);
      result->pipe = pipe;
      result->io_event = CreateEventW(0, TRUE, FALSE, 0);
      result->stop_event = CreateEventW(0, TRUE, FALSE, 0);
   }

   scratch_end(scratch);
   return result;
}

exported B32
PipeServerAccept(PipeServer* server) {
   B32 result = 0;

   while (server != 0 && !result && !W32_PipeServerStopped(server)) {
      OVERLAPPED overlapped = {0};
      overlapped.hEvent = server->io_event;
      ResetEvent(server->io_event);

      BOOL  connected = ConnectNamedPipe(server->pipe, &overlapped);
      DWORD error = connected ? 0 : GetLastError();

      if (connected || error == ERROR_PIPE_CONNECTED) {
         result = 1; // it connected between CreateNamedPipe/Disconnect and here
      }
      else if (error == ERROR_NO_DATA) {
         DisconnectNamedPipe(server->pipe); // it connected and already left: wait for the next
      }
      else if (error == ERROR_IO_PENDING) {
         HANDLE events[2] = {server->io_event, server->stop_event};
         DWORD  wait = WaitForMultipleObjects(2, events, FALSE, INFINITE);
         DWORD  bytes = 0;

         if (wait != WAIT_OBJECT_0) {
            CancelIoEx(server->pipe, &overlapped);
         }

         result = GetOverlappedResult(server->pipe, &overlapped, &bytes, TRUE) && wait == WAIT_OBJECT_0;

         if (!result) {
            break;
         }
      }
      else {
         break;
      }
   }

   if (server != 0) {
      server->connected = result;
   }

   return result;
}

exported B32
PipeServerRead(Arena* arena, PipeServer* server, String8* out) {
   B32 result = 0;
   *out = str8_zero();

   if (server != 0 && server->connected) {
      result = W32_PipeReadMessage(arena, server->pipe, server->io_event, server->stop_event, W32_PIPE_SERVER_TIMEOUT_MS, out);
   }

   return result;
}

exported B32
PipeServerWrite(PipeServer* server, String8 message) {
   B32 result = 0;

   if (server != 0 && server->connected) {
      result = W32_PipeWriteMessage(server->pipe, message, server->io_event, server->stop_event, W32_PIPE_SERVER_TIMEOUT_MS);
   }

   return result;
}

exported void
PipeServerDisconnect(PipeServer* server) {
   if (server != 0 && server->connected) {
      // DisconnectNamedPipe throws away anything the client has not read yet.
      // So wait for it to hang up first - a read that fails once it has -
      // rather than FlushFileBuffers, which could wait on a stuck client forever.
      U8    byte = 0;
      DWORD bytes = 0;
      DWORD error = 0;

      while (W32_PipeIO(server->pipe, 0, &byte, 1, server->io_event, server->stop_event, W32_PIPE_SERVER_TIMEOUT_MS, &bytes, &error) || error == ERROR_MORE_DATA) {
      }

      DisconnectNamedPipe(server->pipe);
      server->connected = 0;
   }
}

exported void
PipeServerStop(PipeServer* server) {
   if (server != 0) {
      SetEvent(server->stop_event);
   }
}

exported void
PipeServerClose(PipeServer* server) {
   if (server != 0) {
      if (server->connected) {
         DisconnectNamedPipe(server->pipe);
      }

      CloseHandle(server->pipe);
      CloseHandle(server->io_event);
      CloseHandle(server->stop_event);
      MemoryZeroStruct(server);
   }
}

exported PipeRequestResult
PipeRequest(Arena* arena, String8 name, String8 request, U64 timeout_ms) {
   PipeRequestResult result = {0};
   Temp              scratch = scratch_begin(&arena, 1);
   String16          path16 = str16_from_8(scratch.arena, push_str8f(scratch.arena, "\\\\.\\pipe\\%S", name));
   HANDLE            pipe = INVALID_HANDLE_VALUE;

   // Busy: the server is with another client. Wait for it a few times, then
   // give up. Not there at all: no server, at once.
   for (U32 attempt = 0; attempt < 4; attempt += 1) {
      pipe = CreateFileW((WCHAR*)path16.str, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);

      if (pipe != INVALID_HANDLE_VALUE || GetLastError() != ERROR_PIPE_BUSY ||
          !WaitNamedPipeW((WCHAR*)path16.str, timeout_ms != 0 ? (DWORD)timeout_ms : NMPWAIT_WAIT_FOREVER)) {
         break;
      }
   }

   if (pipe != INVALID_HANDLE_VALUE) {
      result.connected = 1;

      DWORD  mode = PIPE_READMODE_MESSAGE;
      HANDLE io_event = CreateEventW(0, TRUE, FALSE, 0);

      result.ok = SetNamedPipeHandleState(pipe, &mode, 0, 0) &&
                  W32_PipeWriteMessage(pipe, request, io_event, 0, timeout_ms) &&
                  W32_PipeReadMessage(arena, pipe, io_event, 0, timeout_ms, &result.reply);

      CloseHandle(io_event);
      CloseHandle(pipe);
   }

   scratch_end(scratch);
   return result;
}

////////////////////////////////////////////////////////////////
//~ bgz: Secrets
//
// Generic credentials, persisted on this machine only (not roamed with the
// user's profile).

#include <wincred.h>

exported B32
SecretWrite(String8 name, String8 value) {
   B32  result = 0;
   Temp scratch = scratch_begin(0, 0);

   if (value.size <= SECRET_VALUE_MAX) {
      String16    name16 = str16_from_8(scratch.arena, name);
      CREDENTIALW credential = {0};
      credential.Type = CRED_TYPE_GENERIC;
      credential.TargetName = (WCHAR*)name16.str;
      credential.CredentialBlobSize = (DWORD)value.size;
      credential.CredentialBlob = value.str;
      credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
      credential.UserName = L"bgz";

      result = CredWriteW(&credential, 0) != 0;
   }

   scratch_end(scratch);
   return result;
}

exported String8
SecretRead(Arena* arena, String8 name) {
   String8      result = str8_zero();
   Temp         scratch = scratch_begin(&arena, 1);
   String16     name16 = str16_from_8(scratch.arena, name);
   CREDENTIALW* credential = 0;

   if (CredReadW((WCHAR*)name16.str, CRED_TYPE_GENERIC, 0, &credential)) {
      result = push_str8_copy(arena, str8(credential->CredentialBlob, credential->CredentialBlobSize));
      SecureZeroMemory(credential->CredentialBlob, credential->CredentialBlobSize);
      CredFree(credential);
   }

   scratch_end(scratch);
   return result;
}

exported B32
SecretDelete(String8 name) {
   Temp     scratch = scratch_begin(0, 0);
   String16 name16 = str16_from_8(scratch.arena, name);
   B32      result = CredDeleteW((WCHAR*)name16.str, CRED_TYPE_GENERIC, 0) || GetLastError() == ERROR_NOT_FOUND;

   scratch_end(scratch);
   return result;
}

////////////////////////////////////////////////////////////////
//~ bgz: Crash Report @Added(CrashReport)
//
// Written by a thread of its own, started from the crash filter below: a fresh
// stack, so a stack overflow does not stop it. Everything is in fixed buffers
// here; dbghelp is loaded when it is needed, as raddebugger's
// win32_exception_filter does, whose stack walk this is.

global char             w32_crash_report_dir[1024]     = {0}; // UTF-8, 0 terminated; empty = off
global char             w32_crash_report_project[1024] = {0};
global volatile LONG    w32_crash_report_state         = 0; // 0 none yet, 1 being written, 2 written
global CONTEXT          w32_crash_report_context;
global EXCEPTION_RECORD w32_crash_report_record;
global DWORD            w32_crash_report_thread_id;
global ULONG_PTR        w32_crash_report_stack_low;
global ULONG_PTR        w32_crash_report_stack_high;
global char             w32_crash_report_text[KB(64)];
global U64              w32_crash_report_size;
global char             w32_crash_report_log[KB(16)]; // the end of editor_log.txt
global DWORD            w32_crash_report_writer_id; // the report thread's
global CrashReportExtraFunction* w32_crash_report_extra = 0;
global char             w32_crash_report_extra_text[KB(32)];

// The stack walk's results: every frame's address - a 1 MB stack holds fewer
// than 64K frames, and untouched pages of this cost nothing - and how often
// each function is on the stack, by its start address.
#define W32_CRASH_REPORT_FRAMES    KB(64)
#define W32_CRASH_REPORT_FUNCTIONS 256
global U64 w32_crash_report_pcs[W32_CRASH_REPORT_FRAMES];
global struct {
   U64 start;
   U64 pc;         // one frame's, to name it by
   U64 count;
   U64 last_frame; // the deepest frame it is in
} w32_crash_report_functions[W32_CRASH_REPORT_FUNCTIONS];

global struct {
   DWORD (WINAPI* SymSetOptions)(DWORD options);
   BOOL (WINAPI* SymInitialize)(HANDLE process, PCSTR search_path, BOOL invade_process);
   BOOL (WINAPI* StackWalk64)(DWORD machine, HANDLE process, HANDLE thread, LPSTACKFRAME64 frame, PVOID context,
                              PREAD_PROCESS_MEMORY_ROUTINE64 read_memory, PFUNCTION_TABLE_ACCESS_ROUTINE64 function_table_access,
                              PGET_MODULE_BASE_ROUTINE64 get_module_base, PTRANSLATE_ADDRESS_ROUTINE64 translate_address);
   PVOID (WINAPI* SymFunctionTableAccess64)(HANDLE process, DWORD64 address_base);
   DWORD64 (WINAPI* SymGetModuleBase64)(HANDLE process, DWORD64 address);
   BOOL (WINAPI* SymFromAddr)(HANDLE process, DWORD64 address, PDWORD64 displacement, PSYMBOL_INFO symbol);
   BOOL (WINAPI* SymGetLineFromAddr64)(HANDLE process, DWORD64 address, PDWORD displacement, PIMAGEHLP_LINE64 line);
   BOOL (WINAPI* SymGetModuleInfo64)(HANDLE process, DWORD64 address, PIMAGEHLP_MODULE64 module);
   BOOL (WINAPI* MiniDumpWriteDump)(HANDLE process, DWORD process_id, HANDLE file, MINIDUMP_TYPE type,
                                    PMINIDUMP_EXCEPTION_INFORMATION exception, PMINIDUMP_USER_STREAM_INFORMATION user_streams,
                                    PMINIDUMP_CALLBACK_INFORMATION callback);
} w32_crash_dbghelp;

internal void
W32_CrashReportF(char* fmt, ...) {
   U64 room = sizeof(w32_crash_report_text) - w32_crash_report_size;

   if (room > 1) {
      va_list args;
      va_start(args, fmt);
      int written = raddbg_vsnprintf(w32_crash_report_text + w32_crash_report_size, (int)room, fmt, args);
      va_end(args);

      if (written > 0) {
         w32_crash_report_size += Min((U64)written, room - 1);
      }
   }
}

internal char*
W32_CrashExceptionName(DWORD code) {
   char* result = "unknown exception";
   switch (code) {
   case EXCEPTION_ACCESS_VIOLATION:      result = "access violation"; break;
   case EXCEPTION_STACK_OVERFLOW:        result = "stack overflow"; break;
   case EXCEPTION_INT_DIVIDE_BY_ZERO:    result = "integer divide by zero"; break;
   case EXCEPTION_INT_OVERFLOW:          result = "integer overflow"; break;
   case EXCEPTION_ILLEGAL_INSTRUCTION:   result = "illegal instruction"; break;
   case EXCEPTION_PRIV_INSTRUCTION:      result = "privileged instruction"; break;
   case EXCEPTION_IN_PAGE_ERROR:         result = "in-page error"; break;
   case EXCEPTION_DATATYPE_MISALIGNMENT: result = "datatype misalignment"; break;
   case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: result = "array bounds exceeded"; break;
   case EXCEPTION_FLT_DIVIDE_BY_ZERO:    result = "float divide by zero"; break;
   case EXCEPTION_FLT_INVALID_OPERATION: result = "float invalid operation"; break;
   case EXCEPTION_BREAKPOINT:            result = "breakpoint"; break;
   case 0xC0000374:                      result = "heap corruption"; break;
   case 0xC0000409:                      result = "stack buffer overrun / fast fail"; break;
   }
   return result;
}

// "module  function +0x1f  file:line", or "module +0x1a2b  0x7ff..." with no
// symbols. lookup is the address the line is looked up by: a return address
// minus one, so the line is the call's rather than the next one's.
internal void
W32_CrashReportAddress(HANDLE process, U64 address, U64 lookup) {
   IMAGEHLP_MODULE64 module = {0};
   module.SizeOfStruct = sizeof(module);
   U64 module_base = w32_crash_dbghelp.SymGetModuleBase64(process, lookup);
   B32 has_module = module_base != 0 && w32_crash_dbghelp.SymGetModuleInfo64(process, module_base, &module);
   W32_CrashReportF("%s", has_module ? module.ModuleName : "?");

   struct {
      SYMBOL_INFO info;
      char        name[MAX_SYM_NAME];
   } symbol = {0};
   symbol.info.SizeOfStruct = sizeof(symbol.info);
   symbol.info.MaxNameLen = MAX_SYM_NAME;

   DWORD64 displacement = 0;
   if (w32_crash_dbghelp.SymFromAddr(process, lookup, &displacement, &symbol.info)) {
      W32_CrashReportF("  %s +0x%llx", symbol.info.Name, (U64)(address - symbol.info.Address));

      IMAGEHLP_LINE64 line = {0};
      line.SizeOfStruct = sizeof(line);
      DWORD line_displacement = 0;
      if (w32_crash_dbghelp.SymGetLineFromAddr64(process, lookup, &line_displacement, &line)) {
         W32_CrashReportF("  %s:%lu", line.FileName, line.LineNumber);
      }
   }
   else {
      W32_CrashReportF(" +0x%llx  0x%llx", (U64)(address - module_base), address);
   }

   W32_CrashReportF("\n");
}

// The module containing address: its file's name, when it was last written -
// so an exe and a dll from different builds show - and whether its pdb was
// found.
internal void
W32_CrashReportModule(HANDLE process, void* address) {
   HMODULE module = 0;
   char    path[MAX_PATH] = {0};

   if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)address, &module) &&
       GetModuleFileNameA(module, path, sizeof(path)) != 0) {
      char* name = path;
      for (char* c = path; *c != 0; c += 1) {
         if (*c == '\\' || *c == '/') {
            name = c + 1;
         }
      }

      WIN32_FILE_ATTRIBUTE_DATA attributes = {0};
      SYSTEMTIME                time = {0};
      FILETIME                  local = {0};
      if (GetFileAttributesExA(path, GetFileExInfoStandard, &attributes) &&
          FileTimeToLocalFileTime(&attributes.ftLastWriteTime, &local)) {
         FileTimeToSystemTime(&local, &time);
      }

      IMAGEHLP_MODULE64 info = {0};
      info.SizeOfStruct = sizeof(info);
      B32 has_pdb = w32_crash_dbghelp.SymGetModuleInfo64(process, (DWORD64)module, &info) && info.SymType == SymPdb;

      W32_CrashReportF("%s %04u-%02u-%02u %02u:%02u:%02u%s", name, time.wYear, time.wMonth, time.wDay,
                       time.wHour, time.wMinute, time.wSecond, has_pdb ? "" : " (no pdb found)");
   }
}

// name in the report folder.
internal HANDLE
W32_CrashReportOpen(char* name, DWORD access, DWORD share, DWORD disposition) {
   HANDLE result = INVALID_HANDLE_VALUE;
   char   path8[1200] = {0};
   WCHAR  path16[1200] = {0};

   raddbg_snprintf(path8, sizeof(path8), "%s\\%s", w32_crash_report_dir, name);
   if (MultiByteToWideChar(CP_UTF8, 0, path8, -1, path16, ArrayCount(path16)) != 0) {
      result = CreateFileW(path16, access, share, 0, disposition, FILE_ATTRIBUTE_NORMAL, 0);
   }
   return result;
}

// size bytes of text onto the end of name in the report folder.
internal void
W32_CrashReportAppend(char* name, char* text, U64 size) {
   HANDLE file = W32_CrashReportOpen(name, FILE_APPEND_DATA, 0, OPEN_EXISTING);
   if (file != INVALID_HANDLE_VALUE) {
      DWORD written = 0;
      WriteFile(file, text, (DWORD)size, &written, 0);
      CloseHandle(file);
   }
}

internal DWORD WINAPI
W32_CrashReportThread(LPVOID unused) {
   UnusedVariable(unused);
   HANDLE            process = GetCurrentProcess();
   HANDLE            thread = OpenThread(THREAD_ALL_ACCESS, FALSE, w32_crash_report_thread_id);
   EXCEPTION_RECORD* record = &w32_crash_report_record;
   CONTEXT           context = w32_crash_report_context;
   B32               symbols = 0;

   HMODULE dbghelp = LoadLibraryA("dbghelp.dll");
   if (dbghelp != 0) {
      *(FARPROC*)&w32_crash_dbghelp.SymSetOptions = GetProcAddress(dbghelp, "SymSetOptions");
      *(FARPROC*)&w32_crash_dbghelp.SymInitialize = GetProcAddress(dbghelp, "SymInitialize");
      *(FARPROC*)&w32_crash_dbghelp.StackWalk64 = GetProcAddress(dbghelp, "StackWalk64");
      *(FARPROC*)&w32_crash_dbghelp.SymFunctionTableAccess64 = GetProcAddress(dbghelp, "SymFunctionTableAccess64");
      *(FARPROC*)&w32_crash_dbghelp.SymGetModuleBase64 = GetProcAddress(dbghelp, "SymGetModuleBase64");
      *(FARPROC*)&w32_crash_dbghelp.SymFromAddr = GetProcAddress(dbghelp, "SymFromAddr");
      *(FARPROC*)&w32_crash_dbghelp.SymGetLineFromAddr64 = GetProcAddress(dbghelp, "SymGetLineFromAddr64");
      *(FARPROC*)&w32_crash_dbghelp.SymGetModuleInfo64 = GetProcAddress(dbghelp, "SymGetModuleInfo64");
      *(FARPROC*)&w32_crash_dbghelp.MiniDumpWriteDump = GetProcAddress(dbghelp, "MiniDumpWriteDump");

      symbols = w32_crash_dbghelp.SymSetOptions != 0 && w32_crash_dbghelp.SymInitialize != 0 && w32_crash_dbghelp.StackWalk64 != 0 &&
                w32_crash_dbghelp.SymFunctionTableAccess64 != 0 && w32_crash_dbghelp.SymGetModuleBase64 != 0 &&
                w32_crash_dbghelp.SymFromAddr != 0 && w32_crash_dbghelp.SymGetLineFromAddr64 != 0 && w32_crash_dbghelp.SymGetModuleInfo64 != 0;
   }

   if (symbols) {
      // Symbols are looked for next to the exe, where the pdbs are built.
      char search_path[MAX_PATH] = {0};
      GetModuleFileNameA(0, search_path, sizeof(search_path));
      for (char* c = search_path + lstrlenA(search_path); c > search_path && c[-1] != '\\' && c[-1] != '/'; c -= 1) {
         c[-1] = 0;
      }

      w32_crash_dbghelp.SymSetOptions(SYMOPT_EXACT_SYMBOLS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);
      // Fails when something in the process initialized dbghelp already - its
      // symbols are there then, so go on.
      w32_crash_dbghelp.SymInitialize(process, search_path, TRUE);
   }

   // crash_<date>_<time>: the report is its .txt, the dump its .dmp.
   SYSTEMTIME now = {0};
   GetLocalTime(&now);

   char name[64] = {0};
   raddbg_snprintf(name, sizeof(name), "crash_%04u-%02u-%02u_%02u-%02u-%02u",
                   now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);

   //- What, where, which thread
   DWORD code = record->ExceptionCode;
   W32_CrashReportF("Editor crash report\n\n");
   W32_CrashReportF("exception: 0x%08lX %s", code, W32_CrashExceptionName(code));

   if ((code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR) && record->NumberParameters >= 2) {
      ULONG_PTR kind = record->ExceptionInformation[0];
      W32_CrashReportF(" %s 0x%llx", kind == 0 ? "reading" : kind == 1 ? "writing" : kind == 8 ? "executing" : "at",
                       (U64)record->ExceptionInformation[1]);
   }

   W32_CrashReportF("\nwhere:     ");
   if (symbols) {
      W32_CrashReportAddress(process, (U64)record->ExceptionAddress, (U64)record->ExceptionAddress);
   }
   else {
      W32_CrashReportF("0x%llx (dbghelp.dll not loaded)\n", (U64)record->ExceptionAddress);
   }

   W32_CrashReportF("thread:    %lu", w32_crash_report_thread_id);
   {
      HRESULT (WINAPI* get_thread_description)(HANDLE, PWSTR*) = 0;
      *(FARPROC*)&get_thread_description = GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetThreadDescription");

      PWSTR description = 0;
      if (thread != 0 && get_thread_description != 0 && SUCCEEDED(get_thread_description(thread, &description)) && description != 0) {
         char name[256] = {0};
         WideCharToMultiByte(CP_UTF8, 0, description, -1, name, sizeof(name) - 1, 0, 0);
         if (name[0] != 0) {
            W32_CrashReportF(" %s", name);
         }
         LocalFree(description);
      }
   }

   //- The stack: walked once, keeping every frame, and how often each
   // function appears - a recursion that overflowed is the function that
   // fills it.
   U64 stack_used = (U64)(w32_crash_report_stack_high - context.Rsp);
   U64 frame_count = 0;
   B32 null_call = 0;

   if (symbols) {
      STACKFRAME64 frame = {0};
      frame.AddrPC.Offset = context.Rip;
      frame.AddrPC.Mode = AddrModeFlat;
      frame.AddrFrame.Offset = context.Rbp;
      frame.AddrFrame.Mode = AddrModeFlat;
      frame.AddrStack.Offset = context.Rsp;
      frame.AddrStack.Mode = AddrModeFlat;

      // A call through a null function pointer: no unwind info at 0, so start
      // from the return address it left on the stack.
      if (frame.AddrPC.Offset == 0 && frame.AddrStack.Offset >= w32_crash_report_stack_low &&
          frame.AddrStack.Offset <= w32_crash_report_stack_high - sizeof(void*)) {
         frame.AddrPC.Offset = *(DWORD64*)frame.AddrStack.Offset;
         frame.AddrStack.Offset += sizeof(void*);
         context.Rip = frame.AddrPC.Offset;
         context.Rsp = frame.AddrStack.Offset;
         null_call = 1;
      }

      for (; frame_count < W32_CRASH_REPORT_FRAMES; frame_count += 1) {
         if (!w32_crash_dbghelp.StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context, 0,
                                           w32_crash_dbghelp.SymFunctionTableAccess64, w32_crash_dbghelp.SymGetModuleBase64, 0) ||
             frame.AddrPC.Offset == 0) {
            break;
         }

         U64 pc = frame.AddrPC.Offset;
         w32_crash_report_pcs[frame_count] = pc;

         // The function by its start, from the unwind data - no symbols needed.
         // A leaf function has none and counts by its address.
         DWORD64           image_base = 0;
         PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(frame_count == 0 ? pc : pc - 1, &image_base, 0);
         U64               start = function != 0 ? image_base + function->BeginAddress : pc;

         for (U64 i = start % W32_CRASH_REPORT_FUNCTIONS, probes = 0; probes < W32_CRASH_REPORT_FUNCTIONS;
              i = (i + 1) % W32_CRASH_REPORT_FUNCTIONS, probes += 1) {
            if (w32_crash_report_functions[i].count == 0) {
               w32_crash_report_functions[i].start = start;
               w32_crash_report_functions[i].pc = pc;
            }
            if (w32_crash_report_functions[i].start == start) {
               w32_crash_report_functions[i].count += 1;
               w32_crash_report_functions[i].last_frame = frame_count;
               break;
            }
         }
      }
   }

   W32_CrashReportF("\nstack:     %llu KB used of %llu KB, %llu frames\n", stack_used / KB(1),
                    (U64)(w32_crash_report_stack_high - w32_crash_report_stack_low) / KB(1), frame_count);

   // The three functions on the stack most often, when one is there at least
   // 8 times. The deepest frame of the first is where a recursion began.
   U64 recursion_end = frame_count;
   for (U64 shown = 0; shown < 3; shown += 1) {
      U64 best = W32_CRASH_REPORT_FUNCTIONS;
      for (U64 i = 0; i < W32_CRASH_REPORT_FUNCTIONS; i += 1) {
         if (w32_crash_report_functions[i].count >= 8 &&
             (best == W32_CRASH_REPORT_FUNCTIONS || w32_crash_report_functions[i].count > w32_crash_report_functions[best].count)) {
            best = i;
         }
      }

      if (best == W32_CRASH_REPORT_FUNCTIONS) {
         break;
      }

      if (shown == 0) {
         recursion_end = w32_crash_report_functions[best].last_frame;
      }

      U64 pc = w32_crash_report_functions[best].pc;
      W32_CrashReportF(shown == 0 ? "repeated:  %llux  " : "           %llux  ", w32_crash_report_functions[best].count);
      W32_CrashReportAddress(process, pc, pc - 1);
      w32_crash_report_functions[best].count = 0;
   }

   //- Which build
   if (symbols) {
      W32_CrashReportF("build:     ");
      W32_CrashReportModule(process, GetModuleHandleA(0));
      W32_CrashReportF(", ");
      W32_CrashReportModule(process, (void*)&W32_CrashReportThread);
      W32_CrashReportF("\n");
   }

   if (w32_crash_report_project[0] != 0) {
      W32_CrashReportF("project:   %s\n", w32_crash_report_project);
   }

   if (w32_crash_dbghelp.MiniDumpWriteDump != 0) {
      W32_CrashReportF("dump:      %s.dmp - open it in a debugger before rebuilding, which replaces the pdbs it needs\n", name);
   }

   //- Call stack: the first 40 frames, the 15 from a recursion's deepest frame
   // on - what started it - and the last 10. Frames between are left out.
   if (symbols) {
      W32_CrashReportF("\ncall stack:\n");

      if (null_call) {
         W32_CrashReportF("        [null function pointer called]\n");
      }

      U64 ranges[3][2] = {
          {0, 40},
          {recursion_end < frame_count ? recursion_end : frame_count, recursion_end < frame_count ? recursion_end + 15 : frame_count},
          {frame_count > 10 ? frame_count - 10 : 0, frame_count},
      };

      // In order of start: the recursion's can come after the last 10's.
      if (ranges[1][0] > ranges[2][0]) {
         U64 swap[2] = {ranges[1][0], ranges[1][1]};
         ranges[1][0] = ranges[2][0], ranges[1][1] = ranges[2][1];
         ranges[2][0] = swap[0], ranges[2][1] = swap[1];
      }

      U64 shown_end = 0;
      for (U64 r = 0; r < ArrayCount(ranges); r += 1) {
         U64 start = Max(ranges[r][0], shown_end);
         U64 end = Min(ranges[r][1], frame_count);

         if (start < end) {
            if (start > shown_end) {
               W32_CrashReportF("  ... %llu frames left out\n", start - shown_end);
            }

            for (U64 i = start; i < end; i += 1) {
               U64 pc = w32_crash_report_pcs[i];
               W32_CrashReportF("  %5llu  ", i);
               W32_CrashReportAddress(process, pc, i == 0 ? pc : pc - 1);
            }

            shown_end = end;
         }
      }
   }

   //- The log's last 50 lines. The editor writes it at the end of each frame,
   // so the crashing frame's own lines are not in it.
   HANDLE log = W32_CrashReportOpen("editor_log.txt", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING);
   if (log != INVALID_HANDLE_VALUE) {
      LARGE_INTEGER size = {0};
      DWORD         read = 0;
      GetFileSizeEx(log, &size);

      U64           read_size = Min((U64)size.QuadPart, sizeof(w32_crash_report_log));
      LARGE_INTEGER offset = {0};
      offset.QuadPart = size.QuadPart - (LONGLONG)read_size;
      if (SetFilePointerEx(log, offset, 0, FILE_BEGIN)) {
         ReadFile(log, w32_crash_report_log, (DWORD)read_size, &read, 0);
      }
      CloseHandle(log);

      // Back from the end, past the last line's own line end, over 50 more.
      U64 start = read > 0 && w32_crash_report_log[read - 1] == '\n' ? read - 1 : read;
      for (U64 lines = 0; start > 0; start -= 1) {
         if (w32_crash_report_log[start - 1] == '\n') {
            lines += 1;
            if (lines == 50) {
               break;
            }
         }
      }

      W32_CrashReportF("\nlog (the end of editor_log.txt):\n");
      for (U64 line_start = start; line_start < read;) {
         U64 line_end = line_start;
         while (line_end < read && w32_crash_report_log[line_end] != '\n') {
            line_end += 1;
         }
         W32_CrashReportF("  %.*s\n", (int)(line_end - line_start), w32_crash_report_log + line_start);
         line_start = line_end + 1;
      }
   }

   //- The report, and last_crash.txt naming it, for the next start to find
   char  file_name[80] = {0};
   DWORD written = 0;
   raddbg_snprintf(file_name, sizeof(file_name), "%s.txt", name);

   HANDLE file = W32_CrashReportOpen(file_name, GENERIC_WRITE, 0, CREATE_ALWAYS);
   if (file != INVALID_HANDLE_VALUE) {
      WriteFile(file, w32_crash_report_text, (DWORD)w32_crash_report_size, &written, 0);
      CloseHandle(file);

      HANDLE last = W32_CrashReportOpen("last_crash.txt", GENERIC_WRITE, 0, CREATE_ALWAYS);
      if (last != INVALID_HANDLE_VALUE) {
         WriteFile(last, file_name, (DWORD)lstrlenA(file_name), &written, 0);
         CloseHandle(last);
      }
   }

   //- The dump, after the report - it is slower, and the report matters more.
   // Every thread's stack and registers, and the memory near each pointer on a
   // stack: a few MB. Not the modules' globals (MiniDumpWithDataSegs): that is
   // every loaded dll's data too, and made a dump 37 MB.
   if (w32_crash_dbghelp.MiniDumpWriteDump != 0) {
      char dump_name[80] = {0};
      raddbg_snprintf(dump_name, sizeof(dump_name), "%s.dmp", name);

      B32    dumped = 0;
      HANDLE dump = W32_CrashReportOpen(dump_name, GENERIC_WRITE, 0, CREATE_ALWAYS);
      if (dump != INVALID_HANDLE_VALUE) {
         EXCEPTION_POINTERS             pointers = {&w32_crash_report_record, &w32_crash_report_context};
         MINIDUMP_EXCEPTION_INFORMATION exception = {0};
         exception.ThreadId = w32_crash_report_thread_id;
         exception.ExceptionPointers = &pointers;
         exception.ClientPointers = FALSE;

         MINIDUMP_TYPE type = (MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo |
                                              MiniDumpWithUnloadedModules);
         dumped = w32_crash_dbghelp.MiniDumpWriteDump(process, GetCurrentProcessId(), dump, type, &exception, 0, 0);
         CloseHandle(dump);
      }

      // Said at the end of the report, which is already written.
      if (!dumped) {
         char line[128] = {0};
         raddbg_snprintf(line, sizeof(line), "\nThe dump could not be written (error %lu).\n", GetLastError());
         W32_CrashReportAppend(file_name, line, (U64)lstrlenA(line));
      }
   }

   //- What the program adds (CrashReportSetExtra), last: it reads the
   // program's own state, which the crash may have broken. If it faults, this
   // thread ends there (W32_CrashReport) - the report and dump are written.
   if (w32_crash_report_extra != 0) {
      U64 size = w32_crash_report_extra(w32_crash_report_extra_text, sizeof(w32_crash_report_extra_text));
      size = Min(size, sizeof(w32_crash_report_extra_text));

      if (size > 0) {
         W32_CrashReportAppend(file_name, "\n", 1);
         W32_CrashReportAppend(file_name, w32_crash_report_extra_text, size);
      }
   }

   if (thread != 0) {
      CloseHandle(thread);
   }
   return 0;
}

// The first thread to crash writes the report, on a thread of its own, waiting
// up to 30 seconds for it and its dump. Any other that crashes meanwhile waits
// until it is written, then goes on without one.
internal void
W32_CrashReport(EXCEPTION_POINTERS* exception_ptrs) {
   // The report thread itself faulting - in CrashReportSetExtra's function,
   // most likely: it ends, and the crash it was reporting goes on as if it had
   // finished.
   if (w32_crash_report_writer_id != 0 && GetCurrentThreadId() == w32_crash_report_writer_id) {
      ExitThread(1);
   }

   if (w32_crash_report_dir[0] != 0) {
      if (InterlockedCompareExchange(&w32_crash_report_state, 1, 0) != 0) {
         while (w32_crash_report_state == 1) {
            Sleep(10);
         }
      }
      else {
         w32_crash_report_context = *exception_ptrs->ContextRecord;
         w32_crash_report_record = *exception_ptrs->ExceptionRecord;
         w32_crash_report_thread_id = GetCurrentThreadId();
         GetCurrentThreadStackLimits(&w32_crash_report_stack_low, &w32_crash_report_stack_high);

         HANDLE thread = CreateThread(0, MB(1), W32_CrashReportThread, 0, 0, &w32_crash_report_writer_id);
         if (thread != 0) {
            WaitForSingleObject(thread, 30 * 1000);
            CloseHandle(thread);
         }

         InterlockedExchange(&w32_crash_report_state, 2);
      }
   }
}

////////////////////////////////////////////////////////////////
//~ bgz: Crash Debugger @Added(CrashDebugger)
//
// The filter runs in a process that has just crashed: everything it needs is
// in fixed buffers, on its stack or here, and nothing is allocated.

global char                         w32_crash_debugger[1024]   = {0}; // UTF-8, 0 terminated; empty = off
global LPTOP_LEVEL_EXCEPTION_FILTER w32_crash_previous_filter  = 0;
global B32                          w32_crash_filter_installed = 0;

// Starts command8 (UTF-8). The caller closes the handles in process.
internal B32
W32_CrashStart(char* command8, PROCESS_INFORMATION* process) {
   WCHAR       command16[2048];
   STARTUPINFOW startup = {.cb = sizeof(startup)};
   B32         result = 0;

   if (MultiByteToWideChar(CP_UTF8, 0, command8, -1, command16, ArrayCount(command16)) != 0) {
      result = CreateProcessW(0, command16, 0, 0, 0, 0, 0, 0, &startup, process);
   }
   return result;
}

internal LONG WINAPI
W32_CrashFilter(EXCEPTION_POINTERS* exception_ptrs) {
   B32 attached = 0;

   // Before the debugger, so there is a report whether one attaches or not.
   W32_CrashReport(exception_ptrs);

   if (w32_crash_debugger[0] != 0 && !IsDebuggerPresent()) {
      char                command8[2048];
      PROCESS_INFORMATION process = {0};

      // An open debugger is told to attach (--ipc), so it keeps its window,
      // layout and breakpoints - found by its exe's name: the end of its path,
      // with .exe when that has none.
      char  exe8[MAX_PATH] = {0};
      WCHAR exe16[MAX_PATH] = {0};
      {
         char* name = w32_crash_debugger;
         for (char* c = w32_crash_debugger; *c != 0; c += 1) {
            if (*c == '\\' || *c == '/') {
               name = c + 1;
            }
         }
         B32 has_dot = 0;
         for (char* c = name; *c != 0; c += 1) {
            has_dot |= (*c == '.');
         }
         raddbg_snprintf(exe8, sizeof(exe8), "%s%s", name, has_dot ? "" : ".exe");
      }

      B32 open = 0;
      if (MultiByteToWideChar(CP_UTF8, 0, exe8, -1, exe16, ArrayCount(exe16)) != 0) {
         HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
         if (snapshot != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W entry = {.dwSize = sizeof(entry)};
            for (BOOL more = Process32FirstW(snapshot, &entry); more && !open; more = Process32NextW(snapshot, &entry)) {
               open = (lstrcmpiW(entry.szExeFile, exe16) == 0);
            }
            CloseHandle(snapshot);
         }
      }

      if (open) {
         AllowSetForegroundWindow(ASFW_ANY);

         raddbg_snprintf(command8, sizeof(command8), "\"%s\" --ipc attach %lu", w32_crash_debugger, GetCurrentProcessId());
         if (W32_CrashStart(command8, &process)) {
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
         }

         raddbg_snprintf(command8, sizeof(command8), "\"%s\" --ipc bring_to_front", w32_crash_debugger);
         if (W32_CrashStart(command8, &process)) {
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
         }

         // The ipc ones only pass the command on and exit: up to 10 seconds
         // for the open debugger to attach, then a new one instead.
         for (U64 i = 0; i < 100 && !attached; i += 1) {
            attached = IsDebuggerPresent();
            if (!attached) {
               Sleep(100);
            }
         }
      }

      if (!attached) {
         raddbg_snprintf(command8, sizeof(command8), "\"%s\" --jit_pid:%lu --jit_code:%lu --jit_addr:0x%llx",
                         w32_crash_debugger, GetCurrentProcessId(),
                         (unsigned long)exception_ptrs->ExceptionRecord->ExceptionCode,
                         (U64)exception_ptrs->ExceptionRecord->ExceptionAddress);

         if (W32_CrashStart(command8, &process)) {
            // Up to a minute, giving up early if the debugger exits instead.
            for (U64 i = 0; i < 600 && !attached; i += 1) {
               attached = IsDebuggerPresent();

               if (!attached && WaitForSingleObject(process.hProcess, 100) == WAIT_OBJECT_0) {
                  break;
               }
            }

            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
         }
      }
   }

   // Attached: the faulting instruction runs again and faults again, which
   // the debugger now catches where it happened.
   LONG result = EXCEPTION_CONTINUE_SEARCH;

   if (attached) {
      result = EXCEPTION_CONTINUE_EXECUTION;
   }
   else if (w32_crash_previous_filter != 0) {
      result = w32_crash_previous_filter(exception_ptrs);
   }

   return result;
}

exported void
CrashDebuggerSet(String8 debugger) {
   U64 size = Min(debugger.size, sizeof(w32_crash_debugger) - 1);
   MemoryCopy(w32_crash_debugger, debugger.str, size);
   w32_crash_debugger[size] = 0;

   if (!w32_crash_filter_installed) {
      w32_crash_filter_installed = 1;
      w32_crash_previous_filter = SetUnhandledExceptionFilter(&W32_CrashFilter);
   }
}

internal void
CrashDebuggerThreadBegin(void) {
   ULONG size = (ULONG)KB(64);
   SetThreadStackGuarantee(&size);
}

exported void
CrashReportSet(String8 report_dir, String8 project) {
   U64 dir_size = Min(report_dir.size, sizeof(w32_crash_report_dir) - 1);
   MemoryCopy(w32_crash_report_dir, report_dir.str, dir_size);
   w32_crash_report_dir[dir_size] = 0;

   U64 project_size = Min(project.size, sizeof(w32_crash_report_project) - 1);
   MemoryCopy(w32_crash_report_project, project.str, project_size);
   w32_crash_report_project[project_size] = 0;

   if (!w32_crash_filter_installed) {
      w32_crash_filter_installed = 1;
      w32_crash_previous_filter = SetUnhandledExceptionFilter(&W32_CrashFilter);
   }
}

exported void
CrashReportSetExtra(CrashReportExtraFunction* function) {
   w32_crash_report_extra = function;
}

