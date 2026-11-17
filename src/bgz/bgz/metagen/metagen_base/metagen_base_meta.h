// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

#ifndef BASE_META_H
#define BASE_META_H

////////////////////////////////////////////////////////////////
//~ rjf: Generated Table Types

typedef struct TweakB32Info TweakB32Info;
struct TweakB32Info
{
 String8 name;
 B32 default_value;
 B32 *value_ptr;
};

typedef struct TweakF32Info TweakF32Info;
struct TweakF32Info
{
 String8 name;
 F32 default_value;
 Rng1F32 value_range;
 F32 *value_ptr;
};

typedef struct TweakB32InfoTable TweakB32InfoTable;
struct TweakB32InfoTable
{
 TweakB32Info *v;
 U64 count;
};

typedef struct TweakF32InfoTable TweakF32InfoTable;
struct TweakF32InfoTable
{
 TweakF32Info *v;
 U64 count;
};

typedef struct EmbedInfo EmbedInfo;
struct EmbedInfo
{
 String8 name;
 String8 *data;
 U128 *hash;
};

typedef struct EmbedInfoTable EmbedInfoTable;
struct EmbedInfoTable
{
 EmbedInfo *v;
 U64 count;
};

////////////////////////////////////////////////////////////////
//~ rjf: Run-Time Type Information Types

typedef enum TypeKind
{
 TypeKind_Null,
 
 // rjf: leaves
 TypeKind_Void, TypeKind_FirstLeaf = TypeKind_Void,
 TypeKind_U8,
 TypeKind_U16,
 TypeKind_U32,
 TypeKind_U64,
 TypeKind_S8,
 TypeKind_S16,
 TypeKind_S32,
 TypeKind_S64,
 TypeKind_B8,
 TypeKind_B16,
 TypeKind_B32,
 TypeKind_B64,
 TypeKind_F32,
 TypeKind_F64, TypeKind_LastLeaf = TypeKind_F64,
 
 // rjf: operators
 TypeKind_Ptr,
 TypeKind_Array,
 
 // rjf: user-defined-types
 TypeKind_Struct,
 TypeKind_Union,
 TypeKind_Enum,
 
 TypeKind_COUNT
}
TypeKind;

typedef U32 TypeFlags;
enum
{
 TypeFlag_IsExternal  = (1<<0),
 TypeFlag_IsPlainText = (1<<1),
 TypeFlag_IsCodeText  = (1<<2),
 TypeFlag_IsPathText  = (1<<3),
};

typedef U32 MemberFlags;
enum
{
 MemberFlag_DoNotSerialize  = (1<<0),
};

typedef struct Type Type;
typedef struct Member Member;
struct Member
{
 String8 name;
 String8 pretty_name;
 Type *type;
 U64 value;
 MemberFlags flags;
};

typedef struct Type Type;
struct Type
{
 TypeKind kind;
 TypeFlags flags;
 U64 size;
 Type *direct;
 String8 name;
 String8 count_delimiter_name; // gathered from surrounding members, turns *->[1] into *->[N]
 U64 count;
 Member *members;
};

////////////////////////////////////////////////////////////////
//~ rjf: Type Serialization Parameters

typedef struct TypeSerializePtrRefInfo TypeSerializePtrRefInfo;
struct TypeSerializePtrRefInfo
{
 Type *type;           // pointers to this
 void *indexify_base;  // can be indexified using this
 void *offsetify_base; // can be offsetified using this
 void *nil_ptr;        // is terminal if matching 0 or this
};

typedef struct TypeSerializeParams TypeSerializeParams;
struct TypeSerializeParams
{
 U64 *advance_out;
 TypeSerializePtrRefInfo *ptr_ref_infos;
 U64 ptr_ref_infos_count;
};

////////////////////////////////////////////////////////////////
//~ rjf: Type Name -> Type Info

#define Type(T) (&T##__type)

////////////////////////////////////////////////////////////////
//~ rjf: Type Info Table Initializer Helpers

#define MemberLitComp(S, ti, m, ...) {Str8LitComp(#m), {0}, (ti), OffsetOf(S, m), __VA_ARGS__}
#define StructMembers(S) read_only global Member S##__members[] =
#define StructType(S, ...) read_only global Type S##__type = {TypeKind_Struct, 0, sizeof(S), &type_nil, Str8LitComp(#S), {0}, ArrayCount(S##__members), S##__members, __VA_ARGS__}
#define named_struct_type(name, S, ...) read_only global Type name##__type = {TypeKind_Struct, 0, sizeof(S), &type_nil, Str8LitComp(#name), {0}, ArrayCount(name##__members), name##__members, __VA_ARGS__}
#define PtrType(name, ti, ...) read_only global Type name = {TypeKind_Ptr, 0, sizeof(void *), (ti), __VA_ARGS__}

////////////////////////////////////////////////////////////////
//~ rjf: Globals

read_only global Type type_nil   = {TypeKind_Null, 0, 0, &type_nil};
read_only global Member member_nil = {{0}, {0}, &type_nil};

////////////////////////////////////////////////////////////////
//~ rjf: Built-In Types

//- rjf: leaves
read_only global Type void__type = {TypeKind_Void, 0, 0,           &type_nil, Str8LitComp("void")};
read_only global Type U8__type   = {TypeKind_U8,   0, sizeof(U8),  &type_nil, Str8LitComp("U8")};
read_only global Type U16__type  = {TypeKind_U16,  0, sizeof(U16), &type_nil, Str8LitComp("U16")};
read_only global Type U32__type  = {TypeKind_U32,  0, sizeof(U32), &type_nil, Str8LitComp("U32")};
read_only global Type U64__type  = {TypeKind_U64,  0, sizeof(U64), &type_nil, Str8LitComp("U64")};
read_only global Type S8__type   = {TypeKind_S8,   0, sizeof(S8),  &type_nil, Str8LitComp("S8")};
read_only global Type S16__type  = {TypeKind_S16,  0, sizeof(S16), &type_nil, Str8LitComp("S16")};
read_only global Type S32__type  = {TypeKind_S32,  0, sizeof(S32), &type_nil, Str8LitComp("S32")};
read_only global Type S64__type  = {TypeKind_S64,  0, sizeof(S64), &type_nil, Str8LitComp("S64")};
read_only global Type B8__type   = {TypeKind_B8,   0, sizeof(B8),  &type_nil, Str8LitComp("B8")};
read_only global Type B16__type  = {TypeKind_B16,  0, sizeof(B16), &type_nil, Str8LitComp("B16")};
read_only global Type B32__type  = {TypeKind_B32,  0, sizeof(B32), &type_nil, Str8LitComp("B32")};
read_only global Type B64__type  = {TypeKind_B64,  0, sizeof(B64), &type_nil, Str8LitComp("B64")};
read_only global Type F32__type  = {TypeKind_F32,  0, sizeof(F32), &type_nil, Str8LitComp("F32")};
read_only global Type F64__type  = {TypeKind_F64,  0, sizeof(F64), &type_nil, Str8LitComp("F64")};

//- rjf: Rng1U64
StructMembers(Rng1U64)
{
 MemberLitComp(Rng1U64, Type(U64), min),
 MemberLitComp(Rng1U64, Type(U64), max),
};
StructType(Rng1U64);

//- rjf: String8
PtrType(String8__str_ptr_type, Type(U8), Str8LitComp("size"));
StructMembers(String8)
{
 MemberLitComp(String8, &String8__str_ptr_type, str),
 MemberLitComp(String8, Type(U64),              size),
};
StructType(String8);

//- rjf: String8Node
extern Type String8Node__type;
Type String8Node__ptr_type = {TypeKind_Ptr, 0, sizeof(void *), &String8Node__type};
Member String8Node__members[] =
{
 {Str8LitComp("next"),   {0}, &String8Node__ptr_type,     OffsetOf(String8Node, next)},
 {Str8LitComp("string"), {0}, Type(String8),              OffsetOf(String8Node, string)},
};
Type String8Node__type =
{
 TypeKind_Struct,
 0,
 sizeof(String8Node),
 &type_nil,
 Str8LitComp("String8Node"),
 {0},
 ArrayCount(String8Node__members),
 String8Node__members,
};

//- rjf: String8List
Member String8List__members[] =
{
 {Str8LitComp("first"),      {0}, &String8Node__ptr_type,     OffsetOf(String8List, first)},
 {Str8LitComp("last"),       {0}, &String8Node__ptr_type,     OffsetOf(String8List, last), MemberFlag_DoNotSerialize},
 {Str8LitComp("node_count"), {0}, Type(U64), OffsetOf(String8List, node_count)},
 {Str8LitComp("total_size"), {0}, Type(U64), OffsetOf(String8List, total_size)},
};
Type String8List__type =
{
 TypeKind_Struct,
 0,
 sizeof(String8List),
 &type_nil,
 Str8LitComp("String8List"),
 {0},
 ArrayCount(String8List__members),
 String8List__members,
};

////////////////////////////////////////////////////////////////
//~ rjf: Type Info Lookups

root_function Member *MemberFromName(Type *type, String8 name);
#define EachMember(T, it) (Member *it = (type(T))->members; it != 0 && it < (type(T))->members + (type(T))->count; it += 1)

////////////////////////////////////////////////////////////////
//~ rjf: Type Info * Instance Operations

root_function void TypedDataRebasePtrs(Type *type, String8 data, void *base_ptr);
root_function String8 SerializedFromTypedData(Arena *arena, Type *type, String8 data, TypeSerializeParams *params);
root_function String8 DeserializedFromTypedData(Arena *arena, Type *type, String8 data, TypeSerializeParams *params);
root_function String8 DeepCopyFromTypedData(Arena *arena, Type *type, String8 data, TypeSerializeParams *params);
#define StructRebasePtrs(T, ptr, base)                   TypedDataRebasePtrs(type(T), str8_struct(ptr), (base))
#define SerializedFromStruct(arena, T, ptr, ...)         SerializedFromTypedData((arena), type(T), str8_struct(ptr), &(TypeSerializeParams){.ptr_ref_infos = 0, __VA_ARGS__})
#define StructFromSerialized(arena, T, string, ...) (T *)DeserializedFromTypedData((arena), type(T), (string), &(TypeSerializeParams){.ptr_ref_infos = 0, __VA_ARGS__}).str
#define DeepCopyFromStruct(arena, T, ptr, ...)     (T *)DeepCopyFromTypedData((arena), type(T), str8_struct(ptr), &(TypeSerializeParams){.ptr_ref_infos = 0, __VA_ARGS__}).str

#endif // BASE_META_H
