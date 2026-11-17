////////////////////////////////
//~ bgz: Arenas

exported Arena*
ArenaAlloc_(ArenaParams* params) {
   Arena* result = arena_alloc_(params);
   return result;
}

exported void
ArenaRelease(Arena* arena) {
   arena_release(arena);
}

exported void*
ArenaPush(Arena* arena, U64 size, U64 align, B32 zero) {
   void* result = arena_push(arena, size, align, zero);
   return result;
}

exported U64
ArenaPos(Arena* arena) {
   U64 result = arena_pos(arena);
   return result;
}

exported void
ArenaClear(Arena* arena) {
   arena_clear(arena);
}

exported Temp
TempBegin(Arena* arena) {
   Temp result = temp_begin(arena);
   return result;
}

exported void
TempEnd(Temp temp) {
   temp_end(temp);
}
