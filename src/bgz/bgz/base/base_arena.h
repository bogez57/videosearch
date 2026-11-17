////////////////////////////////
//~ bgz: Arenas
//
// bgz's functions for raddebugger's arenas (rad/base/base_arena.h). Each only
// calls raddebugger's function of the same name. Scratch arenas are in
// base_thread_context.h, where raddebugger has them.

#ifndef BGZ_BASE_ARENA_H
#define BGZ_BASE_ARENA_H

exported Arena* ArenaAlloc_(ArenaParams* params);
exported void   ArenaRelease(Arena* arena);
exported void*  ArenaPush(Arena* arena, U64 size, U64 align, B32 zero);
exported U64    ArenaPos(Arena* arena);
exported void   ArenaClear(Arena* arena);
exported Temp   TempBegin(Arena* arena);
exported void   TempEnd(Temp temp);

// The defaults come first and what the caller passes overrides them, which is
// what clang's -Winitializer-overrides warns about; it is off inside the macro only.
#if COMPILER_CLANG
# define ArenaAlloc(...)                                                                                                     \
   (_Pragma("clang diagnostic push") _Pragma("clang diagnostic ignored \"-Winitializer-overrides\"")                   \
    ArenaAlloc_(&(ArenaParams){.reserve_size = arena_default_reserve_size, .commit_size = arena_default_commit_size,     \
                               .flags = arena_default_flags, .allocation_site_file = __FILE__, .allocation_site_line = __LINE__, \
                               __VA_ARGS__})                                                                            \
    _Pragma("clang diagnostic pop"))
#else
# define ArenaAlloc(...) ArenaAlloc_(&(ArenaParams){.reserve_size = arena_default_reserve_size, .commit_size = arena_default_commit_size, .flags = arena_default_flags, .allocation_site_file = __FILE__, .allocation_site_line = __LINE__, __VA_ARGS__})
#endif

#define PushArrayAligned(a, T, c, align)       (T *)ArenaPush((a), sizeof(T)*(c), (align), (1))
#define PushArrayNoZeroAligned(a, T, c, align) (T *)ArenaPush((a), sizeof(T)*(c), (align), (0))
#define PushArray(a, T, c)                     PushArrayAligned(a, T, c, Max(8, AlignOf(T)))
#define PushArrayNoZero(a, T, c)               PushArrayNoZeroAligned(a, T, c, Max(8, AlignOf(T)))

#endif // BGZ_BASE_ARENA_H
