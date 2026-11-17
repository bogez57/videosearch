// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

#ifndef BASE_ARENA_H
#define BASE_ARENA_H

////////////////////////////////
//~ rjf: Constants

#define ARENA_HEADER_SIZE 64

////////////////////////////////
//~ rjf: Types

typedef U32 ArenaFlags;
enum
{
 ArenaFlag_NoChain = (1<<0),
};

typedef struct ArenaParams ArenaParams;
struct ArenaParams
{
 ArenaFlags flags;
 U64 reserve_size;
 U64 commit_size;
 void *optional_backing_buffer;
};

typedef struct Arena Arena;
struct Arena
{
 Arena *prev;    // previous arena in chain
 Arena *current; // current arena in chain
 ArenaFlags flags;
 U32 cmt_size;
 U64 res_size;
 U64 base_pos;
 U64 pos;
 U64 cmt;
 U64 res;
};
StaticAssert(sizeof(Arena) <= ARENA_HEADER_SIZE, arena_header_size_check);

typedef struct Temp Temp;
struct Temp
{
 Arena *arena;
 U64 pos;
};

////////////////////////////////
//~ rjf: Global Defaults

global U64 arena_default_reserve_size = Megabytes(64);
global U64 arena_default_commit_size  = Kilobytes(64);
global ArenaFlags arena_default_flags = 0;

////////////////////////////////
//~ rjf: Arena Functions

//- rjf: arena creation/destruction
root_function Arena *ArenaAlloc_(ArenaParams *params);
#define ArenaAlloc(...) ArenaAlloc_(&(ArenaParams){.reserve_size = arena_default_reserve_size, .commit_size = arena_default_commit_size, .flags = arena_default_flags, __VA_ARGS__})
root_function void ArenaRelease(Arena *arena);

//- rjf: arena push/pop/pos core functions
root_function void *ArenaPush(Arena *arena, U64 size, U64 align);
root_function U64   ArenaPos(Arena *arena);
root_function void  ArenaPopTo(Arena *arena, U64 pos);

//- rjf: arena push/pop helpers
root_function void ArenaClear(Arena *arena);
root_function void ArenaPop(Arena *arena, U64 amt);

//- rjf: temporary arena scopes
root_function Temp TempBegin(Arena *arena);
root_function void TempEnd(Temp temp);

//- rjf: push helper macros
#define PushArrayNoZeroAligned(a, T, c, align) (T *)ArenaPush((a), sizeof(T)*(c), (align))
#define PushArrayAligned(a, T, c, align) (T *)MemoryZero(PushArrayNoZeroAligned(a, T, c, align), sizeof(T)*(c))
#define PushArrayNoZero(a, T, c) PushArrayNoZeroAligned(a, T, c, Max(8, AlignOf(T)))
#define PushArray(a, T, c) PushArrayAligned(a, T, c, Max(8, AlignOf(T)))

#endif // BASE_ARENA_H
