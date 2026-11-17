
// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

#ifndef FILE_STREAM_H
#define FILE_STREAM_H

////////////////////////////////////////////////////////////////
//~ rjf: Cache Types

typedef struct FS_Val FS_Val;
struct FS_Val
{
 U128 hash;
 U64 timestamp;
};

typedef struct FS_Node FS_Node;
struct FS_Node
{
 FS_Node *hash_next;
 String8 path;
 FS_Val val;
 S64 task_taken;
};

typedef struct FS_Slot FS_Slot;
struct FS_Slot
{
 FS_Node *first;
 FS_Node *last;
};

typedef struct FS_Stripe FS_Stripe;
struct FS_Stripe
{
 Arena *arena;
 SysHandle cv;
 SysHandle rw_mutex;
};

////////////////////////////////////////////////////////////////
//~ rjf: Shared State Bundle

typedef struct FS_Shared FS_Shared;
struct FS_Shared
{
 Arena *arena;
 
 //- rjf: user -> stream thread ring buffer
 U8 *u2s_ring_base;
 U64 u2s_ring_size;
 U64 u2s_ring_write_pos;
 U64 u2s_ring_read_pos;
 SysHandle u2s_ring_mutex;
 SysHandle u2s_ring_cv;
 
 //- rjf: shared striped cache of path -> info
 U64 slots_count;
 U64 stripes_count;
 FS_Slot *slots;
 FS_Stripe *stripes;
 
 //- rjf: threads
 U64 stream_thread_count;
 SysHandle *stream_threads;
 SysHandle scanner_thread;
};

////////////////////////////////////////////////////////////////
//~ rjf: Globals

root_global FS_Shared *fs_shared;

////////////////////////////////////////////////////////////////
//~ rjf: Basic Helpers

exported U64 FS_HashFromString(String8 string);

////////////////////////////////////////////////////////////////
//~ rjf: Top-Level API

exported void FS_Init(void);

////////////////////////////////////////////////////////////////
//~ rjf: Cache Lookups

exported FS_Val FS_ValFromPath(String8 path, U64 endt_us);
#define FS_DataHashFromPath(path, endt_us) (FS_ValFromPath((path), (endt_us)).hash)

////////////////////////////////////////////////////////////////
//~ rjf: Streaming Thread

exported B32 FS_U2SEnqueueRequest(String8 path, U64 endt_us);
exported String8 FS_U2SDequeueRequest(Arena *arena);
exported void FS_StreamThreadEntryPoint(void *p);

////////////////////////////////////////////////////////////////
//~ rjf: Scanner Thread

exported void FS_ScannerThreadEntryPoint(void *p);

#endif // FILE_STREAM_H
