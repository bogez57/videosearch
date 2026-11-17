// Standalone process integration tests. The same executable acts as the child,
// so the tests do not depend on cmd, PowerShell, or an installed formatter.
#include "base/base_inc.h"
#include "window_manager/window_manager_inc.h" // @Changed(no_os_layer) was os/os_inc.h
#include "shell/shell_inc.h"
#include "socket/socket_inc.h"
#include <stdio.h>
#include <string.h>

static int failures;
static int checks;
#define CHECK(expr) do { checks += 1; if (!(expr)) { printf("FAILED line %d: %s\n", __LINE__, #expr); failures += 1; } } while (0)

static void WriteBytes(HANDLE handle, void* bytes, DWORD size) {
   U8* data = (U8*)bytes;
   while (size != 0) {
      DWORD written = 0;
      if (!WriteFile(handle, data, size, &written, 0) || written == 0) ExitProcess(90);
      data += written;
      size -= written;
   }
}

static int Child(int argc, char** argv) {
   HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
   HANDLE err = GetStdHandle(STD_ERROR_HANDLE);
   if (strcmp(argv[1], "--args") == 0) {
      for (int i = 2; i < argc; i += 1) {
         WriteBytes(out, argv[i], (DWORD)strlen(argv[i]));
         WriteBytes(out, "\n", 1);
      }
      return 0;
   }
   if (strcmp(argv[1], "--cwd") == 0) {
      WCHAR directory[32768];
      DWORD size = GetCurrentDirectoryW(ArrayCount(directory), directory);
      WriteBytes(out, directory, size * sizeof(WCHAR));
      return 0;
   }
   if (strcmp(argv[1], "--sleep") == 0) {
      WriteBytes(out, "started", 7);
      Sleep(30000);
      return 0;
   }
   if (strcmp(argv[1], "--descendant") == 0) {
      // Exit while a descendant still holds all three pipes. A deadline must
      // cover pipe draining after the direct child exits, and reap this child.
      WCHAR self[32768];
      WCHAR command[32768];
      GetModuleFileNameW(0, self, ArrayCount(self));
      _snwprintf(command, ArrayCount(command), L"\"%ls\" --sleep", self);
      STARTUPINFOW startup = {0};
      startup.cb = sizeof(startup);
      startup.dwFlags = STARTF_USESTDHANDLES;
      startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
      startup.hStdOutput = out;
      startup.hStdError = err;
      PROCESS_INFORMATION info = {0};
      if (!CreateProcessW(self, command, 0, 0, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, 0, 0, &startup, &info)) return 91;
      WriteBytes(out, &info.dwProcessId, sizeof(info.dwProcessId));
      ResumeThread(info.hThread);
      CloseHandle(info.hThread);
      CloseHandle(info.hProcess);
      return 0;
   }
   if (strcmp(argv[1], "--exit") == 0) {
      WriteBytes(out, "output", 6);
      WriteBytes(err, "diagnostic", 10);
      return 37;
   }
   if (strcmp(argv[1], "--close-input") == 0) {
      CloseHandle(GetStdHandle(STD_INPUT_HANDLE));
      Sleep(50);
      return 0;
   }
   if (strcmp(argv[1], "--flood") == 0) {
      U8 block[16384];
      memset(block, 'O', sizeof(block));
      for (int i = 0; i < 32; i += 1) WriteBytes(out, block, sizeof(block));
      memset(block, 'E', sizeof(block));
      for (int i = 0; i < 32; i += 1) WriteBytes(err, block, sizeof(block));
   }
   U8 buffer[16384];
   DWORD size = 0;
   while (ReadFile(GetStdHandle(STD_INPUT_HANDLE), buffer, sizeof(buffer), &size, 0) && size != 0) {
      WriteBytes(out, buffer, size);
   }
   return 0;
}

static ProcessRunResult Run(Arena* arena, String8 self, char* mode, String8 input, U64 timeout_us) {
   ProcessRunParams params = {0};
   params.executable = self;
   params.input = input;
   params.timeout_us = timeout_us;
   Str8ListPush(arena, &params.arguments, Str8C(mode));
   return ProcessRun(arena, &params);
}

static B32 Succeeded(ProcessRunResult r) {
   return !r.launch_failed && !r.io_failed && !r.timed_out && r.exited && r.exit_code == 0;
}

int wmain(int argc, WCHAR** wide_argv) {
   if (argc > 1) {
      char** argv = (char**)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)argc * sizeof(char*));
      for (int i = 0; i < argc; i += 1) {
         int size = WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, 0, 0, 0, 0);
         argv[i] = (char*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)size);
         WideCharToMultiByte(CP_UTF8, 0, wide_argv[i], -1, argv[i], size, 0, 0);
      }
      return Child(argc, argv);
   }
   TCTX context = TCTXAlloc();
   SetTCTX(&context);
   PlatformInit();
   Arena* arena = ArenaAlloc();
   WCHAR self16[32768];
   DWORD self_size = GetModuleFileNameW(0, self16, ArrayCount(self16));
   String8 self = Str8From16(arena, (String16){.str = (U16*)self16, .size = self_size});

   ProcessRunResult r = Run(arena, self, "--echo", Str8Lit(""), 5000000);
   CHECK(Succeeded(r));
   CHECK(r.output.size == 0 && r.error.size == 0);
   r = Run(arena, self, "--echo", Str8Lit("a\0b\r\nc"), 0);
   CHECK(Succeeded(r));
   CHECK(Str8Match(r.output, Str8Lit("a\0b\r\nc"), 0));

   String8 input = {.str = PushArrayNoZero(arena, U8, Megabytes(1)), .size = Megabytes(1)};
   for (U64 i = 0; i < input.size; i += 1) input.str[i] = (U8)(i % 251);
   r = Run(arena, self, "--flood", input, 10000000);
   CHECK(Succeeded(r));
   CHECK(r.output.size == Kilobytes(512) + input.size);
   CHECK(r.error.size == Kilobytes(512));
   B32 correct = r.output.size == Kilobytes(512) + input.size && r.error.size == Kilobytes(512);
   if (correct) {
      for (U64 i = 0; i < Kilobytes(512); i += 1) {
         if (r.output.str[i] != 'O' || r.error.str[i] != 'E') correct = 0;
      }
      if (memcmp(r.output.str + Kilobytes(512), input.str, input.size) != 0) correct = 0;
   }
   CHECK(correct);

   ProcessRunParams params = {0};
   params.executable = self;
   params.timeout_us = 5000000;
   Str8ListPush(arena, &params.arguments, Str8Lit("--args"));
   String8 args[] = {Str8Lit(""), Str8Lit("two words"), Str8Lit("a\tb"), Str8Lit("a\"b"),
                     Str8Lit("C:\\trailing path\\"), Str8Lit("\\\\\""), Str8Lit("&|<>%^!"), Str8Lit("caf\xc3\xa9")};
   String8List expected = {0};
   for (U64 i = 0; i < ArrayCount(args); i += 1) {
      Str8ListPush(arena, &params.arguments, args[i]);
      Str8ListPush(arena, &expected, args[i]);
      Str8ListPush(arena, &expected, Str8Lit("\n"));
   }
   // The child converts its wide arguments to UTF-8 for comparison.
   r = ProcessRun(arena, &params);
   CHECK(Succeeded(r));
   CHECK(Str8Match(r.output, Str8ListJoin(arena, expected, 0), 0));

   r = Run(arena, self, "--exit", Str8Lit(""), 5000000);
   CHECK(!r.launch_failed && !r.io_failed && !r.timed_out && r.exited && r.exit_code == 37);
   CHECK(Str8Match(r.output, Str8Lit("output"), 0));
   CHECK(Str8Match(r.error, Str8Lit("diagnostic"), 0));
   r = Run(arena, Str8Lit("Z:\\bgz-missing-tool-749291.exe"), "--echo", Str8Lit(""), 5000000);
   CHECK(r.launch_failed && r.os_error != 0 && !r.exited);
   r = Run(arena, self, "--close-input", input, 5000000);
   CHECK(!r.launch_failed && r.io_failed && !r.timed_out);

   U64 before = GetTickCount64();
   r = Run(arena, self, "--sleep", input, 100000);
   CHECK(!r.launch_failed && r.timed_out && r.exited);
   CHECK(GetTickCount64() - before < 5000);
   before = GetTickCount64();
   r = Run(arena, self, "--descendant", Str8Lit(""), 300000);
   CHECK(!r.launch_failed && r.timed_out && r.exited);
   CHECK(GetTickCount64() - before < 5000);

   CHECK(r.output.size >= sizeof(DWORD));
   if (r.output.size >= sizeof(DWORD)) {
      DWORD descendant_pid = 0;
      memcpy(&descendant_pid, r.output.str, sizeof(descendant_pid));
      HANDLE descendant = OpenProcess(SYNCHRONIZE, FALSE, descendant_pid);
      if (descendant != 0) {
         CHECK(WaitForSingleObject(descendant, 2000) == WAIT_OBJECT_0);
         CloseHandle(descendant);
      }
      else CHECK(GetLastError() == ERROR_INVALID_PARAMETER);
   }

   // A working directory with spaces and a Unicode name, also used to launch
   // a copied executable. Neither path should pass through shell parsing.
   WCHAR temp16[32768];
   DWORD temp_size = GetCurrentDirectoryW(ArrayCount(temp16), temp16);
   String8 directory = PushStr8F(arena, "%S/process test caf\xc3\xa9", Str8From16(arena, (String16){.str = (U16*)temp16, .size = temp_size}));
   String16 directory16 = Str16From8(arena, directory);
   CreateDirectoryW((WCHAR*)directory16.str, 0);
   String16 copy16 = Str16From8(arena, PushStr8F(arena, "%S/tool.exe", directory));
   CHECK(CopyFileW(self16, (WCHAR*)copy16.str, FALSE));
   String16 dll16 = Str16From8(arena, PushStr8F(arena, "%S/bgz.dll", directory));
   CHECK(CopyFileW(L"bgz.dll", (WCHAR*)dll16.str, FALSE));
   params = (ProcessRunParams){0};
   params.executable = Str8From16(arena, copy16);
   params.working_directory = directory;
   params.timeout_us = 5000000;
   Str8ListPush(arena, &params.arguments, Str8Lit("--cwd"));
   r = ProcessRun(arena, &params);
   CHECK(Succeeded(r));
   String16 returned = {.str = (U16*)r.output.str, .size = r.output.size / sizeof(U16)};
   CHECK(Str8Match(PathNormalizedFromString(arena, Str8From16(arena, returned)),
                  PathNormalizedFromString(arena, directory), 0));
   DeleteFileW((WCHAR*)copy16.str);
   DeleteFileW((WCHAR*)dll16.str);
   RemoveDirectoryW((WCHAR*)directory16.str);

   params = (ProcessRunParams){0};
   params.executable = self;
   params.working_directory = Str8Lit("Z:\\bgz-missing-directory-749291");
   r = ProcessRun(arena, &params);
   CHECK(r.launch_failed && r.os_error != 0);
   params.working_directory = Str8Lit("");
   Str8ListPush(arena, &params.arguments, Str8Lit("invalid\0argument"));
   r = ProcessRun(arena, &params);
   CHECK(r.launch_failed && r.os_error == ERROR_INVALID_PARAMETER);

   // Repeated use must not leak process, thread, pipe, or job handles.
   DWORD handles_before = 0, handles_after = 0;
   GetProcessHandleCount(GetCurrentProcess(), &handles_before);
   for (int i = 0; i < 20; i += 1) {
      r = Run(arena, self, "--echo", Str8Lit("repeat"), 5000000);
      CHECK(Succeeded(r));
      r = Run(arena, Str8Lit("Z:\\bgz-missing-tool-749291.exe"), "--echo", Str8Lit(""), 5000000);
      CHECK(r.launch_failed);
   }
   GetProcessHandleCount(GetCurrentProcess(), &handles_after);
   CHECK(handles_after == handles_before);

   // Optional end-to-end smoke test with a locally installed formatter. It
   // uses this project's .clang-format and never writes a source file.
   WCHAR formatter16[32768];
   DWORD formatter_size = GetEnvironmentVariableW(L"BGZ_TEST_FORMATTER", formatter16, ArrayCount(formatter16));
   if (formatter_size != 0 && formatter_size < ArrayCount(formatter16)) {
      params = (ProcessRunParams){0};
      params.executable = Str8From16(arena, (String16){.str = (U16*)formatter16, .size = formatter_size});
      params.input = Str8Lit("void f(){\nint x=1;\n}\n");
      params.timeout_us = 5000000;
      Str8ListPush(arena, &params.arguments, Str8Lit("--style=file"));
      Str8ListPush(arena, &params.arguments, Str8Lit("--assume-filename=../../../process_probe.c"));
      r = ProcessRun(arena, &params);
      CHECK(Succeeded(r));
      CHECK(r.error.size == 0);
      CHECK(Str8Match(r.output, Str8Lit("void f() {\n   int x = 1;\n}\n"), 0));
   }
   printf("PROCESS TESTS: %d checks, %d failures\n", checks, failures);
   ArenaRelease(arena);
   TCTXRelease(&context);
   return failures != 0;
}
