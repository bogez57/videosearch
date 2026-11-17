////////////////////////////////
//~ bgz: spall profiler
//
// The app's profiler: compiled into the app only (main.c defines
// BGZ_PROFILE_SPALL and BGZ_PROFILE_SPALL_IMPL), never into core.dll, whose
// zones stay raddebugger's (rad/base/base_profile.h - off). It takes over the
// Prof* macros raddebugger's base_profile.h defined, so include it after
// core_exports.h.

#ifndef BGZ_BASE_PROFILE_H
#define BGZ_BASE_PROFILE_H

#if !defined(BGZ_PROFILE_SPALL)
#define BGZ_PROFILE_SPALL 0
#endif

#if BGZ_PROFILE_SPALL
#if defined(__clang__)
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Weverything"
#endif
#include "third_party/spall.h"
#if defined(__clang__)
# pragma clang diagnostic pop
#endif
#undef ProfBegin
#undef ProfBeginDynamic
#undef ProfEnd
#undef ProfTick
#undef ProfIsCapturing
#undef ProfBeginCapture
#undef ProfEndCapture
#undef ProfThreadName
#undef ProfMsg
#undef ProfBeginLockWait
#undef ProfEndLockWait
#undef ProfLockTake
#undef ProfLockDrop
#undef ProfColor
#undef ProfBeginV
#undef ProfNoteV
#undef ProfBeginFunction
#undef ProfScope
#endif

#if BGZ_PROFILE_SPALL

global B32             spall_initialized = 0;
global U64             spall_capturing = 0;
global U64             spall_capture_id = 0; // counts captures; a thread's buffer belongs to one
global SpallProfile    spall_profile = {0};
global U64             spall_bytes_written = 0; // the capture file's size so far, header included
global U32             spall_main_tid = 0; // the thread that starts captures: given the big buffer
thread_static SpallBuffer spall_buffer = {0};
thread_static U64         spall_buffer_capture_id = 0;
thread_static U64         spall_open_zones = 0; // Begins this thread wrote in its capture, less Ends
thread_static U32         spall_tid = 0;
thread_static U32         spall_pid = 0;

// @Changed(const_fmt): const - ProfBeginFunction passes __func__, which is
// const char[], and the app builds with incompatible pointers as errors.
// file and line are where the zone began, which go in the Begin's args for
// the Profiler view to show.
static inline void SpallBegin(const char* file, int line, const char* fmt, ...);
static inline void SpallEnd(void);
// A whole zone, from begin_us to end_us (NowTimeUS), written at once: for a
// span timed somewhere zones are not, such as inside core.dll.
static inline void SpallZone(const char* file, int line, U64 begin_us, U64 end_us, const char* fmt, ...);
static inline void SpallFlushThread(void);

// The capture's size so far: what is in the file, and what the calling thread
// has buffered for it. Main's buffer is 16 MB and only reaches the file when
// it fills (about every 8 s), so the file's size alone moves in 16 MB steps;
// counting main's buffer, the Profiler view's "capturing" size grows every
// frame. Other threads' buffers (1 MB each) are not counted.
static inline U64 SpallCaptureBytes(void) {
   U64 bytes = spall_bytes_written;
   if (spall_buffer_capture_id == spall_capture_id && spall_buffer.data != 0 && spall_buffer.head > sizeof(SpallBufferHeader)) {
      bytes += spall_buffer.head;
   }
   return bytes;
}

static inline B32  SpallInit(char* filename);
static inline void SpallShutdown(void);

#define ProfBegin(...) (spall_capturing ? (SpallBegin(__FILE__, __LINE__, __VA_ARGS__), 0) : 0)
#define ProfBeginDynamic(...) (spall_capturing ? (SpallBegin(__FILE__, __LINE__, __VA_ARGS__), 0) : 0)
#define ProfEnd(...) (spall_capturing ? (SpallEnd(), 0) : 0)
#define ProfZone(begin_us, end_us, ...) (spall_capturing ? (SpallZone(__FILE__, __LINE__, (begin_us), (end_us), __VA_ARGS__), 0) : 0)

// Writes this thread's buffered events to the capture file now. The main
// thread's buffer is flushed when the capture ends; any other thread's is only
// written when its buffer fills, so a worker thread should call this
// regularly (e.g. once per async tick) or its last events never reach the file.
#define ProfFlushThread(...) (spall_capturing ? (SpallFlushThread(), 0) : 0)

#define ProfTick(...)
#define ProfIsCapturing(...) (!!spall_capturing)
#define ProfCaptureBytes(...) SpallCaptureBytes()
#define ProfBeginCapture(filename) SpallInit(filename)
#define ProfEndCapture(...) SpallShutdown()

#define ProfThreadName(...)
#define ProfMsg(...)
#define ProfBeginLockWait(...)
#define ProfEndLockWait(...)
#define ProfLockTake(...)
#define ProfLockDrop(...)
#define ProfColor(color)
#define ProfBeginV(...)
#define ProfNoteV(...)

#endif // BGZ_PROFILE_SPALL

#if !defined(ProfBegin)
#define ProfBegin(...) ((void)0)
#define ProfBeginDynamic(...) ((void)0)
#define ProfEnd(...) ((void)0)
#define ProfZone(begin_us, end_us, ...) ((void)(begin_us), (void)(end_us))
#define ProfFlushThread(...) ((void)0)
#define ProfTick(...) ((void)0)
#define ProfIsCapturing(...) (0)
#define ProfCaptureBytes(...) ((U64)0)
#define ProfBeginCapture(...) (0) // tested in an if: no capture was started
#define ProfEndCapture(...) ((void)0)
#define ProfThreadName(...) ((void)0)
#define ProfMsg(...) ((void)0)
#define ProfBeginLockWait(...) ((void)0)
#define ProfEndLockWait(...) ((void)0)
#define ProfLockTake(...) ((void)0)
#define ProfLockDrop(...) ((void)0)
#define ProfColor(...) ((void)0)
#define ProfBeginV(...) ((void)0)
#define ProfNoteV(...) ((void)0)
#endif

#define ProfBeginFunction(...) ProfBegin(this_function_name)
#define ProfScope(...) DeferLoop(ProfBeginDynamic(__VA_ARGS__), ProfEnd())

#endif // BGZ_BASE_PROFILE_H

////////////////////////////////
//~ bgz: Implementation

#if defined(BGZ_PROFILE_SPALL_IMPL)
#if BGZ_PROFILE_SPALL

// The capture file, shared by every thread's buffer. Each thread flushes its
// own buffer on its own schedule, and main closes the file at the end of the
// capture, so all three file operations go through this mutex - otherwise a
// worker flushing at the moment of the close writes to a closed FILE.
global Mutex spall_file_mutex = {0};
global FILE* spall_file = 0;

static SPALL_NOINSTRUMENT bool
SpallWrite(SpallProfile* self, const void* data, size_t length) {
   UnusedVariable(self);
   MutexTake(spall_file_mutex);
   bool ok = spall_file != 0 && fwrite(data, length, 1, spall_file) == 1;
   if (ok) {
      spall_bytes_written += length;
   }
   MutexDrop(spall_file_mutex);
   return ok;
}

static SPALL_NOINSTRUMENT bool
SpallFlush(SpallProfile* self) {
   UnusedVariable(self);
   MutexTake(spall_file_mutex);
   bool ok = spall_file != 0 && fflush(spall_file) == 0;
   MutexDrop(spall_file_mutex);
   return ok;
}

static SPALL_NOINSTRUMENT void
SpallClose(SpallProfile* self) {
   UnusedVariable(self);
   MutexTake(spall_file_mutex);
   if (spall_file != 0) {
      fclose(spall_file);
      spall_file = 0;
   }
   MutexDrop(spall_file_mutex);
}

// This thread's buffer, made on its first zone and started again on its first
// zone of each capture.
static inline void
SpallBufferReady(void) {
   if (spall_buffer.data == 0) {
      // A full buffer is written to the file on the thread that filled it, in
      // the middle of whatever zone was open, and a slow write stalled a frame
      // 92 ms (J's run24). Main fills its buffer most, so it gets 16 MB: about
      // one write per 8 s of typing instead of every 0.6 s. Other threads keep
      // 1 MB; each buffer is committed in full.
      spall_buffer.length = TID() == spall_main_tid ? MB(16) : MB(1);
      spall_buffer.data = ReserveMemory(spall_buffer.length);
      CommitMemory(spall_buffer.data, spall_buffer.length);

      // Spall files each thread's events under the tid in its buffer, which
      // every chunk this buffer writes carries in its header. Without it every
      // thread is tid 0, and zones from different threads nest into each other.
      spall_buffer.tid = (U32)TID();
   }

   // The thread's first Begin in this capture. Its buffer may still hold the
   // end of the last capture - a worker's events since its last flush - which
   // is dropped, not written into this file.
   if (spall_buffer_capture_id != spall_capture_id) {
      spall_buffer_capture_id = spall_capture_id;
      spall_open_zones = 0;
      spall_buffer_init(&spall_profile, &spall_buffer);

      // Label this thread's row in spall with the name SetThreadName gave it
      // ("MainThread", "lane_3") instead of its bare thread id. Once
      // per thread per capture: each capture is its own file.
      String8 thread_name = TCTXGetThreadName();
      if (thread_name.size != 0) {
         spall_buffer_name_thread(&spall_profile, &spall_buffer, (char*)thread_name.str, (int32_t)thread_name.size);
      }
   }
}

static inline void
SpallBegin(const char* file, int line, const char* fmt, ...) {
   SpallBufferReady();

   Temp scratch = ScratchBegin(0, 0);

   va_list args;
   va_start(args, fmt);
   String8 string = PushStr8FV(scratch.arena, (char*)fmt, args);
   String8 where  = PushStr8F(scratch.arena, "%s:%d", file, line);

   spall_buffer_begin_args(&spall_profile,
                           &spall_buffer,
                           (char*)string.str,
                           (int32_t)string.size,
                           (char*)where.str,
                           (int32_t)where.size,
                           NowTimeUS());

   va_end(args);

   ScratchEnd(scratch);
   spall_open_zones += 1;
}

static inline void
SpallZone(const char* file, int line, U64 begin_us, U64 end_us, const char* fmt, ...) {
   SpallBufferReady();

   Temp scratch = ScratchBegin(0, 0);

   va_list args;
   va_start(args, fmt);
   String8 string = PushStr8FV(scratch.arena, (char*)fmt, args);
   String8 where  = PushStr8F(scratch.arena, "%s:%d", file, line);
   va_end(args);

   spall_buffer_begin_args(&spall_profile, &spall_buffer, (char*)string.str, (int32_t)string.size, (char*)where.str, (int32_t)where.size, begin_us);
   spall_buffer_end(&spall_profile, &spall_buffer, Max(begin_us, end_us));

   ScratchEnd(scratch);
}

// Only the End of a zone begun in this capture. A capture started by a
// command starts part way through the frame, inside zones begun before it,
// and their Ends would be Ends with no Begin.
static inline void
SpallEnd(void) {
   if (spall_buffer_capture_id == spall_capture_id && spall_open_zones > 0) {
      spall_buffer_end(&spall_profile, &spall_buffer, NowTimeUS());
      spall_open_zones -= 1;
   }
}

static inline void
SpallFlushThread(void) {
   if (spall_buffer_capture_id == spall_capture_id && spall_buffer.data != 0) {
      spall_buffer_flush(&spall_profile, &spall_buffer);
   }
}

// Every named thread's name, each in a buffer of its own under its tid, so
// the viewer shows a row for every thread, even one that does no profiled
// work in the capture. A thread that does writes its name again with its
// first zone (SpallBegin); that is harmless.
static inline void
SpallNameAllThreads(void) {
   U64          count = 0;
   NamedThread* named = NamedThreads(&count);

   for (U64 i = 0; i < count; i += 1) {
      if (InsAtomicU64Eval(&named[i].ready) == 0) {
         continue;
      }

      struct {
         SpallBufferHeader       header;
         SpallNameContainerEvent event;
         U8                      name[64];
      } chunk = {0};

      U64 name_size           = Min(named[i].name_size, sizeof(chunk.name));
      chunk.header.size       = (uint32_t)(sizeof(chunk.event) + name_size);
      chunk.header.tid        = named[i].tid;
      chunk.header.first_ts   = NowTimeUS();
      chunk.event.type        = SpallEventType_NameThread;
      chunk.event.name_length = (uint8_t)name_size;
      MemoryCopy(chunk.name, named[i].name, name_size);
      SpallWrite(&spall_profile, &chunk, sizeof(chunk.header) + sizeof(chunk.event) + name_size);
   }
}

static inline B32
SpallInit(char* filename) {
   spall_main_tid = (U32)TID();

   if (!spall_initialized) {
      if (spall_file_mutex.u64[0] == 0) {
         spall_file_mutex = MutexAlloc();
      }

      FILE* file = fopen(filename, "wb");
      if (file != 0) {
         MutexTake(spall_file_mutex);
         spall_file          = file;
         spall_bytes_written = 0;
         MutexDrop(spall_file_mutex);

         // Our callbacks instead of spall_init_file's plain FILE ones - see spall_file_mutex.
         spall_initialized = spall_init_callbacks(1000, SpallWrite, SpallFlush, SpallClose, 0, &spall_profile);
      }
   }

   // A new capture before capturing is seen, so no thread writes into it
   // from a buffer of the last one.
   if (spall_initialized) {
      InsAtomicU64IncEval(&spall_capture_id);
      SpallNameAllThreads();
   }
   InsAtomicU64EvalAssign(&spall_capturing, spall_initialized);
   return spall_capturing;
}

static inline void
SpallShutdown(void) {
   InsAtomicU64EvalAssign(&spall_capturing, 0);

   // This thread's events so far. The buffer is kept for the next capture.
   if (spall_buffer.data != 0 && spall_buffer_capture_id == spall_capture_id) {
      spall_buffer_quit(&spall_profile, &spall_buffer);
   }

   // Close the file, but keep spall_profile's callbacks: not spall_quit, which
   // zeroes them while a worker thread may still be about to call one. After
   // the close they just fail. A worker's events since its last
   // ProfFlushThread are lost - at most one async tick's worth.
   if (spall_initialized) {
      SpallClose(&spall_profile);
      spall_initialized = 0;
   }
}

#endif
#endif // BGZ_PROFILE_SPALL_IMPL