// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

////////////////////////////////////////////////////////////////
//~ rjf: Thread Context Construction & Equipping

root_function TCTX
TCTXAlloc(void)
{
 TCTX tctx = {0};
 for(U64 arena_idx = 0; arena_idx < ArrayCount(tctx.scratch_arenas); arena_idx += 1)
 {
  tctx.scratch_arenas[arena_idx] = ArenaAlloc();
 }
 tctx.log_arena = ArenaAlloc();
 return tctx;
}

root_function void
TCTXRelease(TCTX *tctx)
{
 for(U64 arena_idx = 0; arena_idx < ArrayCount(tctx->scratch_arenas); arena_idx += 1)
 {
  ArenaRelease(tctx->scratch_arenas[arena_idx]);
 }
}

#if !BUILD_SUPPLEMENTARY_UNIT
per_thread TCTX *tl_tctx = 0;

no_name_mangle void
SetTCTX(TCTX *tctx)
{
 tl_tctx = tctx;
}

no_name_mangle TCTX *
GetTCTX(void)
{
 return tl_tctx;
}
#endif

////////////////////////////////////////////////////////////////
//~ rjf: Thread Metadata

root_function void
SetThreadName(String8 string)
{
 TCTX *tctx = GetTCTX();
 tctx->thread_name_size = Min(string.size, sizeof(tctx->thread_name));
 MemoryCopy(tctx->thread_name, string.str, tctx->thread_name_size);
#if defined(TCTXImpl_SetThreadName)
 TCTXImpl_SetThreadName(string);
#endif
 ProfThreadName("%.*s", Str8VArg(string));
}

root_function void
SetThreadNameF(char *fmt, ...)
{
 Temp scratch = ScratchBegin(0, 0);
 va_list args;
 va_start(args, fmt);
 String8 string = PushStr8FV(scratch.arena, fmt, args);
 SetThreadName(string);
 va_end(args);
 ScratchEnd(scratch);
}

root_function String8
GetThreadName(void)
{
 TCTX *tctx = GetTCTX();
 String8 result = Str8(tctx->thread_name, tctx->thread_name_size);
 return result;
}

root_function B32
IsMainThread(void)
{
 TCTX *tctx = GetTCTX();
 return tctx->is_main_thread;
}

//- rjf: source code location set helper

root_function void
SetThreadFileAndLine_(char *file, int line)
{
 TCTX *tctx = GetTCTX();
 tctx->file_name = file;
 tctx->line_number = line;
}

////////////////////////////////////////////////////////////////
//~ rjf: Scratch Memory

root_function Temp
ScratchBegin(Arena **conflicts, U64 conflict_count)
{
 Temp scratch = {0};
 TCTX *tctx = GetTCTX();
 for(U64 tctx_idx = 0; tctx_idx < ArrayCount(tctx->scratch_arenas); tctx_idx += 1)
 {
  B32 is_conflicting = 0;
  for(Arena **conflict = conflicts; conflict < conflicts+conflict_count; conflict += 1)
  {
   if(*conflict == tctx->scratch_arenas[tctx_idx])
   {
    is_conflicting = 1;
    break;
   }
  }
  if(is_conflicting == 0)
  {
   scratch.arena = tctx->scratch_arenas[tctx_idx];
   scratch.pos = scratch.arena->pos;
   break;
  }
 }
 return scratch;
}

////////////////////////////////////////////////////////////////
//~ rjf: Log Building

root_function void
PushLogScope(String8 name)
{
 TCTX *tctx = GetTCTX();
 LogNode *node = PushArray(tctx->log_arena, LogNode, 1);
 if(node != 0)
 {
  node->string = PushStr8Copy(tctx->log_arena, name);
  node->parent = tctx->log_active_parent;
  if(!node->parent)
  {
   tctx->log_root = node;
  }
  else
  {
   QueuePush(node->parent->first, node->parent->last, node);
  }
  tctx->log_active_parent = node;
 }
}

root_function void
PushLogScopeF(char *fmt, ...)
{
 Temp scratch = ScratchBegin(0, 0);
 va_list args;
 va_start(args, fmt);
 String8 string = PushStr8FV(scratch.arena, fmt, args);
 PushLogScope(string);
 va_end(args);
 ScratchEnd(scratch);
}

root_function	void 
PopLogScope(void)
{
 TCTX *tctx = GetTCTX();
 if(tctx->log_active_parent)
 {
  tctx->log_active_parent = tctx->log_active_parent->parent;
 }
}

root_function void
PushLogMsg(LogMsgKind kind, String8 string)
{
 
}

root_function void
PushLogMsgF(LogMsgKind kind, char *fmt, ...)
{
 Temp scratch = ScratchBegin(0, 0);
 va_list args;
 va_start(args, fmt);
 String8 string = PushStr8FV(scratch.arena, fmt, args);
 PushLogMsg(kind, string);
 va_end(args);
 ScratchEnd(scratch);
}

////////////////////////////////////////////////////////////////
//~ rjf: Log Consumption

root_function LogMsgArray
ConsumeLog(Arena *arena, LogMsgKind min_kind)
{
 LogMsgArray msgs = {0};
 return msgs;
}
