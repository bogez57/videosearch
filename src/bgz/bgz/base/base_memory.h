////////////////////////////////
//~ bgz: Memory
//
// bgz's functions for raddebugger's memory (rad/base/base_memory.h). Each only
// calls raddebugger's function of the same name.

#ifndef BGZ_BASE_MEMORY_H
#define BGZ_BASE_MEMORY_H

exported void* ReserveMemory(U64 size);
exported B32   CommitMemory(void* ptr, U64 size);

#endif // BGZ_BASE_MEMORY_H
