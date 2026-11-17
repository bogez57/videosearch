////////////////////////////////
//~ bgz: Threads
//
// bgz's functions for threads. Like each bgz area mirroring one of
// raddebugger's, the PascalCase functions here are the ones bgz, core and the
// app call: most only call raddebugger's function of the same name, and one
// bgz adds something to says so. raddebugger's are called only from here.

#ifndef BGZ_BASE_THREADS_H
#define BGZ_BASE_THREADS_H

// raddebugger's thread_launch, through a start of bgz's own that first keeps
// the thread room for the crash filter (CrashDebuggerThreadBegin) - so every
// thread started, the async lanes and the app's alike, has it.
exported Thread ThreadLaunch(ThreadEntryPointFunctionType *f, void *p);

exported B32       ThreadJoin(Thread thread, U64 endt_us);
exported U32       TID(void);
exported void      SetThreadName(String8 string);
exported void      SetThreadNameF(char* fmt, ...);
exported void      SetPlatformThreadName(String8 name);

// Every name SetThreadName has given, so a profile capture can list the
// threads that do no profiled work in it (base_profile.h). A slot is only
// filled once ready reads 1; a thread named twice has two slots, the later
// one its name now.
typedef struct NamedThread NamedThread;
struct NamedThread {
   U64 ready;
   U32 tid;
   U8  name[64];
   U64 name_size;
};
exported NamedThread* NamedThreads(U64* out_count);

exported Mutex     MutexAlloc(void);
exported void      MutexRelease(Mutex mutex);
exported void      MutexTake(Mutex mutex);
exported void      MutexDrop(Mutex mutex);

exported CondVar   CondVarAlloc(void);
exported void      CondVarRelease(CondVar cv);
exported B32       CondVarWait(CondVar cv, Mutex mutex, U64 endt_us);
exported void      CondVarSignal(CondVar cv);
exported void      CondVarBroadcast(CondVar cv);

exported Semaphore SemaphoreAlloc(U32 initial_count, U32 max_count, String8 name);
exported void      SemaphoreRelease(Semaphore semaphore);
exported B32       SemaphoreTake(Semaphore semaphore, U64 endt_us);
exported void      SemaphoreDrop(Semaphore semaphore);

#endif // BGZ_BASE_THREADS_H
