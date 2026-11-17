
// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

////////////////////////////////////////////////////////////////
//~ rjf: Globals

#if !BUILD_SUPPLEMENTARY_UNIT
TEX_Shared *tex_shared = 0;
per_thread TEX_TCTX *tex_tctx = 0;
#endif

////////////////////////////////////////////////////////////////
//~ rjf: Basic Type Functions

exported TEX_Topology
TEX_TopologyZero(void)
{
 TEX_Topology top = {R_Tex2DFormat_Null};
 return top;
}

exported B32
TEX_TopologyMatch(TEX_Topology *a, TEX_Topology *b)
{
 B32 result = (a->fmt == b->fmt && a->size.x == b->size.x && a->size.y == b->size.y);
 return result;
}

////////////////////////////////////////////////////////////////
//~ rjf: Layer Initialization

exported void
TEX_Init(void)
{
 if(IsMainThread() && tex_shared == 0)
 {
  Arena *arena = ArenaAlloc(Gigabytes(64));
  tex_shared = PushArray(arena, TEX_Shared, 1);
  tex_shared->arena = arena;
  U32 invalid_tex_rgba[] =
  {
   0xff0000ff, 0x000000ff, 0xff0000ff, 0x000000ff, 0xff0000ff, 0x000000ff, 0xff0000ff, 0x000000ff,
   0x000000ff, 0xff0000ff, 0x000000ff, 0xff0000ff, 0x000000ff, 0xff0000ff, 0x000000ff, 0xff0000ff,
   0xff0000ff, 0x000000ff, 0xff0000ff, 0x000000ff, 0xff0000ff, 0x000000ff, 0xff0000ff, 0x000000ff,
   0x000000ff, 0xff0000ff, 0x000000ff, 0xff0000ff, 0x000000ff, 0xff0000ff, 0x000000ff, 0xff0000ff,
   0xff0000ff, 0x000000ff, 0xff0000ff, 0x000000ff, 0xff0000ff, 0x000000ff, 0xff0000ff, 0x000000ff,
   0x000000ff, 0xff0000ff, 0x000000ff, 0xff0000ff, 0x000000ff, 0xff0000ff, 0x000000ff, 0xff0000ff,
   0xff0000ff, 0x000000ff, 0xff0000ff, 0x000000ff, 0xff0000ff, 0x000000ff, 0xff0000ff, 0x000000ff,
   0x000000ff, 0xff0000ff, 0x000000ff, 0xff0000ff, 0x000000ff, 0xff0000ff, 0x000000ff, 0xff0000ff,
  };
  tex_shared->invalid_tex = R_Tex2DAlloc(V2S64(8, 8), R_Tex2DFormat_RGBA8, R_Tex2DKind_Static, &invalid_tex_rgba[0]);
  tex_shared->slots_count = 4096;
  tex_shared->stripes_count = LogicalProcessorCount();
  tex_shared->slots = PushArray(arena, TEX_Slot, tex_shared->slots_count);
  tex_shared->stripes = PushArray(arena, TEX_Stripe, tex_shared->stripes_count);
  for(U64 idx = 0; idx < tex_shared->stripes_count; idx += 1)
  {
   tex_shared->stripes[idx].arena = ArenaAlloc(Gigabytes(8));
   tex_shared->stripes[idx].rw_mutex = RWMutexAlloc();
   tex_shared->stripes[idx].cv = CondVarAlloc();
  }
  tex_shared->u2x_ring_size = Kilobytes(64);
  tex_shared->u2x_ring_base = PushArrayNoZero(arena, U8, tex_shared->u2x_ring_size);
  tex_shared->u2x_ring_cv = CondVarAlloc();
  tex_shared->u2x_ring_mutex = MutexAlloc();
  tex_shared->xfer_thread_count = Clamp(1, LogicalProcessorCount()-1, 4);
  tex_shared->xfer_threads = PushArray(arena, SysHandle, tex_shared->xfer_thread_count);
  for(U64 idx = 0; idx < tex_shared->xfer_thread_count; idx += 1)
  {
   tex_shared->xfer_threads[idx] = ThreadLaunch((void *)idx, TEX_XferThreadEntryPoint);
  }
  tex_shared->evictor_thread = ThreadLaunch(0, TEX_EvictorThreadEntryPoint);
 }
}

////////////////////////////////////////////////////////////////
//~ rjf: Accessors

exported R_Handle
TEX_InvalidTex(void)
{
 return tex_shared->invalid_tex;
}

////////////////////////////////////////////////////////////////
//~ rjf: Scoped Access Functions

exported TEX_Scope *
TEX_ScopeOpen(void)
{
 if(tex_tctx == 0)
 {
  Arena *arena = ArenaAlloc(Megabytes(64));
  tex_tctx = PushArray(arena, TEX_TCTX, 1);
  tex_tctx->arena = arena;
 }
 TEX_Scope *scope = tex_tctx->free_scope;
 if(scope != 0)
 {
  StackPop(tex_tctx->free_scope);
 }
 else
 {
  scope = PushArrayNoZero(tex_tctx->arena, TEX_Scope, 1);
 }
 MemoryZeroStruct(scope);
 return scope;
}

exported void
TEX_ScopeClose(TEX_Scope *scope)
{
 for(TEX_Touch *t = scope->top_touch, *next = 0; t != 0; t = next)
 {
  next = t->next;
  AtomicDecEval64(&t->node->scope_touch_count);
  StackPush(tex_tctx->free_touch, t);
 }
 StackPush(tex_tctx->free_scope, scope);
}

////////////////////////////////////////////////////////////////
//~ rjf: Cache Lookups

exported R_Slice2F32
TEX_SliceFromHash(TEX_Scope *scope, U128 hash, TEX_Topology top, U64 endt_us)
{
 //- rjf: fill invalid texture by default
 R_Slice2F32 result = {0};
 {
  Vec2S64 invalid_tex_size = R_SizeFromTex2D(tex_shared->invalid_tex);
  result.tex = tex_shared->invalid_tex;
  result.region = R2F32(V2F32(0, 0), Vec2F32FromVec(invalid_tex_size));
 }
 
 //- rjf: unpack hash
 U64 slot_idx = hash.u64[1]%tex_shared->slots_count;
 U64 stripe_idx = slot_idx%tex_shared->stripes_count;
 TEX_Slot *slot = &tex_shared->slots[slot_idx];
 TEX_Stripe *stripe = &tex_shared->stripes[stripe_idx];
 
 //- rjf: loop & try to read
 RWMutexScope_R(stripe->rw_mutex) for(;;)
 {
  //- rjf: read existing node
  TEX_Node *node = 0;
  U64 last_time_requested_us = 0;
  B32 node_found = 0;
  B32 is_stale = 0;
  {
   for(TEX_Node *n = slot->first; n != 0; n = n->next)
   {
    if(U128Match(hash, n->hash) && TEX_TopologyMatch(&top, &n->top))
    {
     node = n;
     last_time_requested_us = n->last_time_requested_us;
     node_found = 1;
     is_stale = R_HandleMatch(R_HandleZero(), n->slice.tex);
     if(!is_stale)
     {
      result = n->slice;
     }
     break;
    }
   }
  }
  
  //- rjf: no node? -> allocate new node in cache; no node or is stale & need request? -> request
  if(!node_found || (is_stale && last_time_requested_us+100000 < NowTimeUS()))
  {
   RWMutexScope_RWPromote(stripe->rw_mutex)
   {
    TEX_Node *node = 0;
    for(TEX_Node *n = slot->first; n != 0; n = n->next)
    {
     if(U128Match(hash, n->hash) && TEX_TopologyMatch(&top, &n->top))
     {
      node = n;
      last_time_requested_us = n->last_time_requested_us;
      break;
     }
    }
    if(!node)
    {
     node = PushArray(stripe->arena, TEX_Node, 1);
     DLLPushBack(slot->first, slot->last, node);
     node->hash = hash;
     node->top = top;
    }
    if(TEX_U2XEnqueueReq(hash, &top, endt_us))
    {
     node->last_time_requested_us = NowTimeUS();
    }
   }
  }
  
  //- rjf: got result? touch node
  if(node_found && !is_stale)
  {
   TEX_Touch *touch = tex_tctx->free_touch;
   if(touch != 0)
   {
    StackPop(tex_tctx->free_touch);
   }
   else
   {
    touch = PushArray(tex_tctx->arena, TEX_Touch, 1);
   }
   touch->node = node;
   AtomicIncEval64(&node->scope_touch_count);
   AtomicEvalSet64(&node->last_time_accessed_us, NowTimeUS());
   StackPush(scope->top_touch, touch);
  }
  
  //- rjf: time out, or got result? exit
  if(NowTimeUS() >= endt_us || (node_found && !is_stale))
  {
   break;
  }
  
  //- rjf: wait on condition variable
  CondVarWaitRW_R(stripe->cv, stripe->rw_mutex, endt_us);
 }
 
 return result;
}

////////////////////////////////////////////////////////////////
//~ rjf: Transfer Threads

#if !defined(STB_IMAGE_INCLUDED)
#define STB_IMAGE_INCLUDED
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_PSD
#define STBI_ONLY_TGA
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb/stb_image.h"
#endif

exported B32
TEX_U2XEnqueueReq(U128 hash, TEX_Topology *top, U64 endt_us)
{
 B32 result = 0;
 MutexScope(tex_shared->u2x_ring_mutex) for(;;)
 {
  U64 unconsumed_size = tex_shared->u2x_ring_write_pos - tex_shared->u2x_ring_read_pos;
  U64 available_size = tex_shared->u2x_ring_size - unconsumed_size;
  if(available_size >= sizeof(hash))
  {
   result = 1;
   tex_shared->u2x_ring_write_pos += RingWriteStruct(tex_shared->u2x_ring_base, tex_shared->u2x_ring_size, tex_shared->u2x_ring_write_pos, &hash);
   tex_shared->u2x_ring_write_pos += RingWriteStruct(tex_shared->u2x_ring_base, tex_shared->u2x_ring_size, tex_shared->u2x_ring_write_pos, top);
   break;
  }
  CondVarWait(tex_shared->u2x_ring_cv, tex_shared->u2x_ring_mutex, endt_us);
 }
 if(result)
 {
  CondVarBroadcast(tex_shared->u2x_ring_cv);
 }
 return result;
}

exported void
TEX_U2XDequeueReq(U128 *hash_out, TEX_Topology *top_out)
{
 MutexScope(tex_shared->u2x_ring_mutex) for(;;)
 {
  U64 unconsumed_size = tex_shared->u2x_ring_write_pos - tex_shared->u2x_ring_read_pos;
  if(unconsumed_size >= sizeof(*hash_out) + sizeof(*top_out))
  {
   tex_shared->u2x_ring_read_pos += RingReadStruct(tex_shared->u2x_ring_base, tex_shared->u2x_ring_size, tex_shared->u2x_ring_read_pos, hash_out);
   tex_shared->u2x_ring_read_pos += RingReadStruct(tex_shared->u2x_ring_base, tex_shared->u2x_ring_size, tex_shared->u2x_ring_read_pos, top_out);
   break;
  }
  CondVarWait(tex_shared->u2x_ring_cv, tex_shared->u2x_ring_mutex, U64Max);
 }
 CondVarBroadcast(tex_shared->u2x_ring_cv);
}

exported void
TEX_XferThreadEntryPoint(void *p)
{
 SetThreadNameF("[TEX] Xfer Thread #%I64u", (U64)p);
 for(;;)
 {
  Temp scratch = ScratchBegin(0, 0);
  C_Scope *scope = C_ScopeOpen();
  
  //- rjf: get next request
  U128 hash = {0};
  TEX_Topology top = {0};
  TEX_U2XDequeueReq(&hash, &top);
  U64 slot_idx = hash.u64[1]%tex_shared->slots_count;
  U64 stripe_idx = slot_idx%tex_shared->stripes_count;
  TEX_Slot *slot = &tex_shared->slots[slot_idx];
  TEX_Stripe *stripe = &tex_shared->stripes[stripe_idx];
  
  //- rjf: take task
  B32 got_task = 0;
  RWMutexScope_R(stripe->rw_mutex)
  {
   for(TEX_Node *n = slot->first; n != 0; n = n->next)
   {
    if(U128Match(n->hash, hash) && TEX_TopologyMatch(&n->top, &top))
    {
     got_task = !!AtomicEvalCompareExchange64(&n->working_count, 1, 0);
     break;
    }
   }
  }
  
  //- rjf: get data for hash
  String8 data = {0};
  if(got_task)
  {
   data = C_DataFromHash(scope, hash);
  }
  
  //- rjf: analyze data format
  B32 data_is_png = 0;
  B32 data_is_ase = 0;
  if(got_task)
  {
   if(data.size >= 8)
   {
    data_is_png = (data.str[0] == 137 && data.str[1] == 80  && data.str[2] == 78  &&
                   data.str[3] == 71  && data.str[4] == 13  && data.str[5] == 10  &&
                   data.str[6] == 26  && data.str[7] == 10);
   }
   if(data.size >= sizeof(ASE_Header))
   {
    ASE_Header *header = (ASE_Header *)data.str;
    data_is_ase = (header->magic == 0xA5E0);
   }
  }
  
  //- rjf: map data to raw pixel data, given format
  R_Tex2DFormat fmt = R_Tex2DFormat_RGBA8;
  Vec2S64 dim = {0};
  String8 raw_data = {0};
  if(got_task)
  {
   //- rjf: png -> use stb image
   if(data_is_png)
   {
    int width = 0;
    int height = 0;
    int components = 0;
    // TODO(rjf): hack stb image to use arena push directly here, to avoid copy
    U8 *raw_data_stbi = stbi_load_from_memory(data.str, data.size, &width, &height, &components, 4);
    raw_data.size = width*height*components;
    raw_data.str = PushArrayNoZero(scratch.arena, U8, raw_data.size);
    MemoryCopy(raw_data.str, raw_data_stbi, raw_data.size);
    dim = V2S64((S64)width, (S64)height);
    stbi_image_free(raw_data_stbi);
   }
   
   //- rjf: ase -> use ase decoder
   else if(data_is_ase)
   {
    ASE_RawImage raw_image = ASE_RawImageFromData(scratch.arena, data);
    dim = raw_image.dim;
    raw_data = Str8(raw_image.data, raw_image.data_size);
   }
  }
  
  //- rjf: upload raw pixel data, obtain render texture handle
  R_Handle handle = {0};
  if(got_task)
  {
   handle = R_Tex2DAlloc(dim, fmt, R_Tex2DKind_Static, raw_data.str);
  }
  
  //- rjf: commit handle to cache
  if(got_task)
  {
   RWMutexScope_W(stripe->rw_mutex)
   {
    for(TEX_Node *n = slot->first; n != 0; n = n->next)
    {
     if(U128Match(n->hash, hash) && TEX_TopologyMatch(&n->top, &top))
     {
      if(!R_HandleMatch(n->slice.tex, R_HandleZero()))
      {
       R_Tex2DRelease(n->slice.tex);
      }
      n->slice.tex = handle;
      n->slice.region = R2F32(V2F32(0, 0), V2F32((F32)dim.x, (F32)dim.y));
      n->working_count = 0;
     }
    }
   }
   CondVarBroadcast(stripe->cv);
  }
  
  C_ScopeClose(scope);
  ScratchEnd(scratch);
 }
}

////////////////////////////////////////////////////////////////
//~ rjf: Eviction Threads

exported void
TEX_EvictorThreadEntryPoint(void *p)
{
 for(;;)
 {
  SleepMS(1000);
 }
}
