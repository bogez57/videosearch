////////////////////////////////
//~ bgz: Memory

exported void*
ReserveMemory(U64 size) {
   void* result = reserve_memory(size);
   return result;
}

exported B32
CommitMemory(void* ptr, U64 size) {
   B32 result = commit_memory(ptr, size);
   return result;
}
