// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

#if !defined(ArenaImpl_Reserve)
#error ArenaImpl_Reserve must be defined to use base memory.
#endif
#if !defined(ArenaImpl_Release)
#error ArenaImpl_Release must be defined to use base memory.
#endif
#if !defined(ArenaImpl_Commit)
#error ArenaImpl_Commit must be defined to use base memory.
#endif
#if !defined(ArenaImpl_Decommit)
#error ArenaImpl_Decommit must be defined to use base memory.
#endif

////////////////////////////////
//~ rjf: Arena Functions

//- rjf: arena creation/destruction

root_function Arena *
ArenaAlloc_(ArenaParams *params)
{
 // rjf: round up reserve/commit sizes
 U64 reserve_size = params->reserve_size;
 U64 commit_size = params->commit_size;
 reserve_size = AlignPow2(reserve_size, OS_PageSize());
 commit_size  = AlignPow2(commit_size,  OS_PageSize());
 
 // rjf: reserve/commit initial block
 void *base = params->optional_backing_buffer;
 if(base == 0)
 {
  base = ArenaImpl_Reserve(reserve_size);
  ArenaImpl_Commit(base, commit_size);
 }
 
 // rjf: panic on arena creation failure
#if OS_FEATURE_GFX
 if(Unlikely(base == 0))
 {
  OS_GraphicalMessage(1, Str8Lit("Fatal Allocation Failure"), Str8Lit("Unexpected memory allocation failure."));
  OS_Abort();
 }
#endif
 
 // rjf: extract arena header & fill
 Arena *arena = (Arena *)base;
 arena->current = arena;
 arena->flags = params->flags;
 arena->cmt_size = (U32)params->commit_size;
 arena->res_size = params->reserve_size;
 arena->base_pos = 0;
 arena->pos = ARENA_HEADER_SIZE;
 arena->cmt = commit_size;
 arena->res = reserve_size;
 AsanPoisonMemoryRegion(base, commit_size);
 AsanUnpoisonMemoryRegion(base, ARENA_HEADER_SIZE);
 return arena;
}

root_function void
ArenaRelease(Arena *arena)
{
 for(Arena *n = arena->current, *prev = 0; n != 0; n = prev)
 {
  prev = n->prev;
  ArenaImpl_Release(n, n->res);
 }
}

//- rjf: arena push/pop core functions

root_function void *
ArenaPush(Arena *arena, U64 size, U64 align)
{
 Arena *current = arena->current;
 U64 pos_pre = AlignPow2(current->pos, align);
 U64 pos_pst = pos_pre + size;
 
 // rjf: chain, if needed
 if(current->res < pos_pst && !(arena->flags & ArenaFlag_NoChain))
 {
  Arena *new_block = 0;
  if(new_block == 0)
  {
   U64 res_size = current->res_size;
   U64 cmt_size = current->cmt_size;
   if(size + ARENA_HEADER_SIZE > res_size)
   {
    res_size = AlignPow2(size + ARENA_HEADER_SIZE, align);
    cmt_size = AlignPow2(size + ARENA_HEADER_SIZE, align);
   }
   new_block = ArenaAlloc(.reserve_size = res_size,
                          .commit_size  = cmt_size,
                          .flags        = current->flags);
  }
  
  new_block->base_pos = current->base_pos + current->res;
  StackPush_N(arena->current, new_block, prev);
  
  current = new_block;
  pos_pre = AlignPow2(current->pos, align);
  pos_pst = pos_pre + size;
 }
 
 // rjf: commit new pages, if needed
 if(current->cmt < pos_pst)
 {
  U64 cmt_pst_aligned = pos_pst + current->cmt_size-1;
  cmt_pst_aligned -= cmt_pst_aligned%current->cmt_size;
  U64 cmt_pst_clamped = ClampTop(cmt_pst_aligned, current->res);
  U64 cmt_size = cmt_pst_clamped - current->cmt;
  U8 *cmt_ptr = (U8 *)current + current->cmt;
  ArenaImpl_Commit(cmt_ptr, cmt_size);
  current->cmt = cmt_pst_clamped;
 }
 
 // rjf: push onto current block
 void *result = 0;
 if(current->cmt >= pos_pst)
 {
  result = (U8 *)current+pos_pre;
  current->pos = pos_pst;
  AsanUnpoisonMemoryRegion(result, size);
 }
 
 // rjf: panic on failure
#if OS_FEATURE_GFX
 if(Unlikely(result == 0))
 {
  OS_GraphicalMessage(1, Str8Lit("Fatal Allocation Failure"), Str8Lit("Unexpected memory allocation failure."));
  OS_Abort();
 }
#endif
 
 return result;
}

root_function U64
ArenaPos(Arena *arena)
{
 Arena *current = arena->current;
 U64 pos = current->base_pos + current->pos;
 return pos;
}

root_function void
ArenaPopTo(Arena *arena, U64 pos)
{
 U64 big_pos = ClampBot(ARENA_HEADER_SIZE, pos);
 Arena *current = arena->current;
 for(Arena *prev = 0; current->base_pos >= big_pos; current = prev)
 {
  prev = current->prev;
  ArenaImpl_Release(current, current->res);
 }
 arena->current = current;
 U64 new_pos = big_pos - current->base_pos;
 AssertAlways(new_pos <= current->pos);
 AsanPoisonMemoryRegion((U8*)current + new_pos, (current->pos - new_pos));
 current->pos = new_pos;
}

//- rjf: arena push/pop helpers

root_function void
ArenaClear(Arena *arena)
{
 ArenaPopTo(arena, 0);
}

root_function void
ArenaPop(Arena *arena, U64 amt)
{
 U64 pos_old = ArenaPos(arena);
 U64 pos_new = pos_old;
 if(amt < pos_old)
 {
  pos_new = pos_old - amt;
 }
 ArenaPopTo(arena, pos_new);
}

//- rjf: temporary arena scopes

root_function Temp
TempBegin(Arena *arena)
{
 U64 pos = ArenaPos(arena);
 Temp temp = {arena, pos};
 return temp;
}

root_function void
TempEnd(Temp temp)
{
 ArenaPopTo(temp.arena, temp.pos);
}
