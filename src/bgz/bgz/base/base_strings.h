////////////////////////////////
//~ bgz: Strings
//
// bgz's functions for raddebugger's strings (rad/base/base_strings.h). Each
// only calls raddebugger's function of the same name.

#ifndef BGZ_BASE_STRINGS_H
#define BGZ_BASE_STRINGS_H

exported B32                 CharIsSpace(U8 c);
exported B32                 CharIsAlpha(U8 c);
exported B32                 CharIsDigit(U8 c, U32 base);
exported U8                  LowerFromChar(U8 c);

exported String8             Str8(U8* str, U64 size);
exported String8             Str8Zero(void);
exported String8             Str8CString(const char* c);
exported String32            Str32(U32* str, U64 size);

exported B32                 Str8Match(String8 a, String8 b, StringMatchFlags flags);
exported U64                 Str8FindNeedle(String8 string, U64 start_pos, String8 needle, StringMatchFlags flags);

exported String8             Str8Substr(String8 str, Rng1U64 range);
exported String8             Str8Prefix(String8 str, U64 size);
exported String8             Str8Skip(String8 str, U64 amt);
exported String8             Str8Postfix(String8 str, U64 size);
exported String8             Str8SkipChopWhitespace(String8 string);
exported String8             Str8SkipLastSlash(String8 string);
exported String8             Str8ChopLastSlash(String8 string);
exported String8             Str8SkipLastDot(String8 string);

exported String8             PushStr8Copy(Arena* arena, String8 s);
exported String8             PushStr8FV(Arena* arena, char* fmt, va_list args);
exported String8             PushStr8F(Arena* arena, char* fmt, ...);

exported String8Node*        Str8ListPush(Arena* arena, String8List* list, String8 string);
exported String8Node*        Str8ListPushF(Arena* arena, String8List* list, char* fmt, ...);
exported String8List         Str8ListCopy(Arena* arena, String8List* list);
exported String8             Str8ListJoin(Arena* arena, String8List* list, StringJoin* optional_params);
exported String8Array        Str8ArrayFromList(Arena* arena, String8List* list);

exported String8             PathNormalizedFromString(Arena* arena, String8 path);

exported String32            Str32From8(Arena* arena, String8 in);
exported String8             Str8From32(Arena* arena, String32 in);

exported U64                 U64HashFromSeedStr8(U64 seed, String8 string);
exported U64                 U64HashFromStr8(String8 string);

exported FuzzyMatchRangeList FuzzyMatchFind(Arena* arena, String8 needle, String8 haystack);
exported FuzzyMatchRangeList FuzzyMatchRangeListCopy(Arena* arena, FuzzyMatchRangeList* src);

// A compound literal, not Str8(): it is used to initialize globals.
#define Str8Lit(S)     ((String8){(U8 *)(S), sizeof(S) - 1})
#define Str8LitComp(S) str8_lit_comp(S)
#define Str8Struct(S)  Str8((U8 *)(S), sizeof(*(S)))
#define Str8VArg(S)    str8_varg(S)

#endif // BGZ_BASE_STRINGS_H
