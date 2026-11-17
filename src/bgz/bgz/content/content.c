
// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

////////////////////////////////////////////////////////////////
//~ rjf: Globals

#if !BUILD_SUPPLEMENTARY_UNIT
C_State *c_state = 0;
#endif

////////////////////////////////////////////////////////////////
//~ rjf: Top-Level API

exported void
C_Init(void)
{
 if(IsMainThread() && c_state == 0)
 {
  Arena *arena = ArenaAlloc();
  c_state = PushArray(arena, C_State, 1);
  c_state->arena = arena;
  c_state->tag2data_table_size = 1024;
  c_state->tag2data_table = PushArray(arena, C_Tag2DataCacheSlot, c_state->tag2data_table_size);
  c_state->tag2data_stripes = StripeTableAlloc(arena, c_state->tag2data_table_size/8);
  for(U64 idx = 0; idx < embed_tables_count; idx += 1)
  {
   EmbedInfoTable *table = embed_tables[idx];
   for(U64 idx = 0; idx < table->count; idx += 1)
   {
    EmbedInfo *info = &table->v[idx];
    C_SubmitStaticData(*info->data, *info->hash);
   }
  }
 }
}

////////////////////////////////////////////////////////////////
//~ rjf: Scopes

exported C_Scope *
C_ScopeOpen(void)
{
 C_Scope *scope = (C_Scope *)1;
 return scope;
}

exported void
C_ScopeClose(C_Scope *scope)
{
}

////////////////////////////////////////////////////////////////
//~ rjf: Hashes

exported U128
C_HashFromString(String8 string)
{
 U128 hash = {0};
 {
  meow_u128 meow_hash = MeowHash(MeowDefaultSeed, string.size, string.str);
  MemoryCopy(&hash, &meow_hash, Min(sizeof(meow_hash), sizeof(hash)));
 }
 return hash;
}

////////////////////////////////////////////////////////////////
//~ rjf: Cache Interaction

exported U128
C_SubmitData(Arena **permanent_arena, String8 data)
{
 //- rjf: produce hash from hash
 U128 hash = C_HashFromString(data);
 
 //- rjf: hash -> slot/stripe info
 U64 slot_idx = hash.u64[1] % c_state->tag2data_table_size;
 U64 stripe_idx = slot_idx % c_state->tag2data_stripes->count;
 Stripe *stripe = &c_state->tag2data_stripes->stripes[stripe_idx];
 C_Tag2DataCacheSlot *slot = &c_state->tag2data_table[slot_idx];
 
 //- rjf: register in (hash) -> (data) data structure
 B32 is_new = 0;
 MutexScope(stripe->mutex)
 {
  C_Tag2DataCacheNode *existing_node = 0;
  for(C_Tag2DataCacheNode *n = slot->first; n != 0; n = n->hash_next)
  {
   if(U128Match(n->hash, hash))
   {
    existing_node = n;
    break;
   }
  }
  if(existing_node == 0)
  {
   C_Tag2DataCacheNode *node = PushArray(stripe->arena, C_Tag2DataCacheNode, 1);
   U64 arena_size = data.size;
   arena_size += Kilobytes(64)-1;
   arena_size -= arena_size%Kilobytes(64);
   node->arena = *permanent_arena;
   node->hash = hash;
   node->data = data;
   ProtectMemory(data.str, data.size, AccessFlag_Read);
   DLLPushBack_NPZ(slot->first, slot->last, node, hash_next, hash_prev, CheckNull, SetNull);
   is_new = 1;
  }
 }
 
 //- rjf: new -> signal
 if(is_new)
 {
  CondVarBroadcast(stripe->cv);
 }
 
 //- rjf: not new -> release arena
 if(is_new == 0)
 {
  Arena *arena = *permanent_arena;
  ArenaRelease(arena);
 }
 *permanent_arena = 0;
 
 return hash;
}

exported void
C_SubmitStaticData(String8 data, U128 hash)
{
 //- rjf: hash -> slot/stripe info
 U64 slot_idx = hash.u64[1] % c_state->tag2data_table_size;
 U64 stripe_idx = slot_idx % c_state->tag2data_stripes->count;
 Stripe *stripe = &c_state->tag2data_stripes->stripes[stripe_idx];
 C_Tag2DataCacheSlot *slot = &c_state->tag2data_table[slot_idx];
 
 //- rjf: register in (hash) -> (data) data structure
 B32 is_new = 0;
 MutexScope(stripe->mutex)
 {
  C_Tag2DataCacheNode *existing_node = 0;
  for(C_Tag2DataCacheNode *n = slot->first; n != 0; n = n->hash_next)
  {
   if(U128Match(n->hash, hash))
   {
    existing_node = n;
    break;
   }
  }
  if(existing_node == 0)
  {
   C_Tag2DataCacheNode *node = PushArray(stripe->arena, C_Tag2DataCacheNode, 1);
   node->arena = 0;
   node->hash = hash;
   node->data = data;
   DLLPushBack_NPZ(slot->first, slot->last, node, hash_next, hash_prev, CheckNull, SetNull);
   is_new = 1;
  }
 }
 
 //- rjf: new -> signal
 if(is_new)
 {
  CondVarBroadcast(stripe->cv);
 }
}

exported String8
C_DataFromHash(C_Scope *scope, U128 hash)
{
 String8 data = {0};
 U64 slot_idx = hash.u64[1] % c_state->tag2data_table_size;
 U64 stripe_idx = slot_idx % c_state->tag2data_stripes->count;
 Stripe *stripe = &c_state->tag2data_stripes->stripes[stripe_idx];
 C_Tag2DataCacheSlot *slot = &c_state->tag2data_table[slot_idx];
 MutexScope(stripe->mutex)
 {
  C_Tag2DataCacheNode *existing_node = 0;
  for(C_Tag2DataCacheNode *n = slot->first; n != 0; n = n->hash_next)
  {
   if(U128Match(n->hash, hash))
   {
    existing_node = n;
    break;
   }
  }
  if(existing_node != 0)
  {
   data = existing_node->data;
   existing_node->last_accessed_time_us = NowTimeUS();
  }
 }
 return data;
}
