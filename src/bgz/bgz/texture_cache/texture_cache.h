
// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

#ifndef TEXTURE_CACHE_H
#define TEXTURE_CACHE_H

////////////////////////////////////////////////////////////////
//~ rjf: Extra Cache Parameters

typedef struct TEX_Topology TEX_Topology;
struct TEX_Topology
{
 R_Tex2DFormat fmt;
 Vec2S64 size;
};

////////////////////////////////////////////////////////////////
//~ rjf: Cache Types

typedef struct TEX_Node TEX_Node;
struct TEX_Node
{
 TEX_Node *next;
 TEX_Node *prev;
 U128 hash;
 TEX_Topology top;
 R_Slice2F32 slice;
 U64 scope_touch_count;
 U64 last_time_accessed_us;
 U64 last_time_requested_us;
 U64 working_count;
};

typedef struct TEX_Slot TEX_Slot;
struct TEX_Slot
{
 TEX_Node *first;
 TEX_Node *last;
};

typedef struct TEX_Stripe TEX_Stripe;
struct TEX_Stripe
{
 Arena *arena;
 SysHandle rw_mutex;
 SysHandle cv;
 TEX_Node *free_node;
};

////////////////////////////////////////////////////////////////
//~ rjf: Scoped Access Types

typedef struct TEX_Touch TEX_Touch;
struct TEX_Touch
{
 TEX_Touch *next;
 TEX_Node *node;
};

typedef struct TEX_Scope TEX_Scope;
struct TEX_Scope
{
 TEX_Scope *next;
 TEX_Touch *top_touch;
};

typedef struct TEX_TCTX TEX_TCTX;
struct TEX_TCTX
{
 Arena *arena;
 TEX_Scope *free_scope;
 TEX_Touch *free_touch;
};

////////////////////////////////////////////////////////////////
//~ rjf: Shared State Type

typedef struct TEX_Shared TEX_Shared;
struct TEX_Shared
{
 Arena *arena;
 R_Handle invalid_tex;
 
 //- rjf: cache
 U64 slots_count;
 U64 stripes_count;
 TEX_Slot *slots;
 TEX_Stripe *stripes;
 
 //- rjf: user -> xfer thread ring buffer
 U64 u2x_ring_size;
 U8 *u2x_ring_base;
 U64 u2x_ring_write_pos;
 U64 u2x_ring_read_pos;
 SysHandle u2x_ring_cv;
 SysHandle u2x_ring_mutex;
 
 //- rjf: xfer threads
 U64 xfer_thread_count;
 SysHandle *xfer_threads;
 
 //- rjf: evictor thread
 SysHandle evictor_thread;
};

////////////////////////////////////////////////////////////////
//~ rjf: Globals

root_global TEX_Shared *tex_shared;

////////////////////////////////////////////////////////////////
//~ rjf: Basic Type Functions

exported TEX_Topology TEX_TopologyZero(void);
exported B32 TEX_TopologyMatch(TEX_Topology *a, TEX_Topology *b);

////////////////////////////////////////////////////////////////
//~ rjf: Layer Initialization

exported void TEX_Init(void);

////////////////////////////////////////////////////////////////
//~ rjf: Accessors

exported R_Handle TEX_InvalidTex(void);

////////////////////////////////////////////////////////////////
//~ rjf: Scoped Access Functions

exported TEX_Scope *TEX_ScopeOpen(void);
exported void TEX_ScopeClose(TEX_Scope *scope);

////////////////////////////////////////////////////////////////
//~ rjf: Cache Lookups

exported R_Slice2F32 TEX_SliceFromHash(TEX_Scope *scope, U128 hash, TEX_Topology top, U64 endt_us);

////////////////////////////////////////////////////////////////
//~ rjf: Transfer Threads

exported B32 TEX_U2XEnqueueReq(U128 hash, TEX_Topology *top, U64 endt_us);
exported void TEX_U2XDequeueReq(U128 *hash_out, TEX_Topology *top_out);
exported void TEX_XferThreadEntryPoint(void *p);

////////////////////////////////////////////////////////////////
//~ rjf: Eviction Threads

exported void TEX_EvictorThreadEntryPoint(void *p);

#endif // TEXTURE_CACHE_H
