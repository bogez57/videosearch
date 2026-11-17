////////////////////////////////
//~ bgz: Threads

// A thread ThreadLaunch starts: its function and parameter, kept until the
// thread has read them. On an arena never cleared - a few bytes a thread, and
// few threads are started. Made on the first call, which is on the main
// thread before any other thread exists (AsyncLanesBegin), so it cannot race.
typedef struct ThreadStart ThreadStart;
struct ThreadStart {
   ThreadEntryPointFunctionType* f;
   void*                         p;
};

global Arena* bgz_thread_starts_arena = 0;
global Mutex  bgz_thread_starts_mutex = {0};

// Keeps 64 KB of this thread's stack back for the crash filter, handed out
// only while a stack overflow is being reported. Without it the filter gets
// the little Windows keeps, too little to start the debugger when the stack
// ran out in small frames. Every thread gets it first thing: the main thread
// in entry_point (base_entry_point.c), every other one in ThreadLaunch
// (here). Defined in win32_base.c; core's own, so declared here rather than
// in base_processes.h, which the app includes.
internal void CrashDebuggerThreadBegin(void);

internal void
ThreadEntryPoint(void* params) {
   ThreadStart start = *(ThreadStart*)params;
   CrashDebuggerThreadBegin();
   start.f(start.p);
}

exported Thread
ThreadLaunch(ThreadEntryPointFunctionType* f, void* p) {
   if (bgz_thread_starts_arena == 0) {
      bgz_thread_starts_arena = arena_alloc();
      bgz_thread_starts_mutex = mutex_alloc();
   }

   ThreadStart* start = 0;
   MutexScope(bgz_thread_starts_mutex) {
      start = push_array(bgz_thread_starts_arena, ThreadStart, 1);
   }
   start->f = f;
   start->p = p;

   Thread result = thread_launch(ThreadEntryPoint, start);
   return result;
}

exported B32
ThreadJoin(Thread thread, U64 endt_us) {
   B32 result = thread_join(thread, endt_us);
   return result;
}

exported U32
TID(void) {
   U32 result = tid();
   return result;
}

// See NamedThreads. A slot is claimed with an atomic add, so naming takes no
// lock and can't race the first thread named; past the last slot, names are
// not kept.
global NamedThread bgz_named_threads[128] = {0};
global U64         bgz_named_thread_count = 0;

// Also into the thread context, where TCTXGetThreadName (and so a spall
// capture's thread names, base_profile.h) reads it: raddebugger's
// set_thread_name only names the thread for the OS and its own profiler.
exported void
SetThreadName(String8 string) {
   tctx_set_thread_name(string);
   set_thread_name(string);

   U64 slot = ins_atomic_u64_inc_eval(&bgz_named_thread_count) - 1;
   if (slot < ArrayCount(bgz_named_threads)) {
      NamedThread* named = &bgz_named_threads[slot];
      named->tid         = tid();
      named->name_size   = Min(string.size, sizeof(named->name));
      MemoryCopy(named->name, string.str, named->name_size);
      ins_atomic_u64_eval_assign(&named->ready, 1);
   }
}

exported NamedThread*
NamedThreads(U64* out_count) {
   *out_count = Min(ins_atomic_u64_eval(&bgz_named_thread_count), ArrayCount(bgz_named_threads));
   return bgz_named_threads;
}

exported void
SetThreadNameF(char* fmt, ...) {
   Temp scratch = scratch_begin(0, 0);
   va_list args;
   va_start(args, fmt);
   String8 string = str8fv(scratch.arena, fmt, args);
   va_end(args);
   SetThreadName(string);
   scratch_end(scratch);
}

exported void
SetPlatformThreadName(String8 name) {
   set_platform_thread_name(name);
}

////////////////////////////////
//~ bgz: Mutexes

exported Mutex
MutexAlloc(void) {
   Mutex result = mutex_alloc();
   return result;
}

exported void
MutexRelease(Mutex mutex) {
   mutex_release(mutex);
}

exported void
MutexTake(Mutex mutex) {
   mutex_take(mutex);
}

exported void
MutexDrop(Mutex mutex) {
   mutex_drop(mutex);
}

////////////////////////////////
//~ bgz: Condition Variables

exported CondVar
CondVarAlloc(void) {
   CondVar result = cond_var_alloc();
   return result;
}

exported void
CondVarRelease(CondVar cv) {
   cond_var_release(cv);
}

exported B32
CondVarWait(CondVar cv, Mutex mutex, U64 endt_us) {
   B32 result = cond_var_wait(cv, mutex, endt_us);
   return result;
}

exported void
CondVarSignal(CondVar cv) {
   cond_var_signal(cv);
}

exported void
CondVarBroadcast(CondVar cv) {
   cond_var_broadcast(cv);
}

////////////////////////////////
//~ bgz: Semaphores

exported Semaphore
SemaphoreAlloc(U32 initial_count, U32 max_count, String8 name) {
   Semaphore result = semaphore_alloc(initial_count, max_count, name);
   return result;
}

exported void
SemaphoreRelease(Semaphore semaphore) {
   semaphore_release(semaphore);
}

exported B32
SemaphoreTake(Semaphore semaphore, U64 endt_us) {
   B32 result = semaphore_take(semaphore, endt_us);
   return result;
}

exported void
SemaphoreDrop(Semaphore semaphore) {
   semaphore_drop(semaphore);
}
