////////////////////////////////
//~ bgz: Characters

exported B32
CharIsSpace(U8 c) {
   B32 result = char_is_space(c);
   return result;
}

exported B32
CharIsAlpha(U8 c) {
   B32 result = char_is_alpha(c);
   return result;
}

exported B32
CharIsDigit(U8 c, U32 base) {
   // rad's char_is_digit indexes a 128-entry table, so bytes >= 0x80 (UTF-8) would read past it.
   B32 result = (c < 0x80) ? char_is_digit(c, base) : 0;
   return result;
}

exported U8
LowerFromChar(U8 c) {
   U8 result = lower_from_char(c);
   return result;
}

////////////////////////////////
//~ bgz: Constructors

exported String8
Str8(U8* str, U64 size) {
   String8 result = str8(str, size);
   return result;
}

exported String8
Str8Zero(void) {
   String8 result = str8_zero();
   return result;
}

exported String8
Str8CString(const char* c) {
   String8 result = str8_cstring(c);
   return result;
}

exported String32
Str32(U32* str, U64 size) {
   String32 result = str32(str, size);
   return result;
}

////////////////////////////////
//~ bgz: Matching

exported B32
Str8Match(String8 a, String8 b, StringMatchFlags flags) {
   B32 result = str8_match(a, b, flags);
   return result;
}

exported U64
Str8FindNeedle(String8 string, U64 start_pos, String8 needle, StringMatchFlags flags) {
   U64 result = str8_find_needle(string, start_pos, needle, flags);
   return result;
}

////////////////////////////////
//~ bgz: Slicing

exported String8
Str8Substr(String8 str, Rng1U64 range) {
   String8 result = str8_substr(str, range);
   return result;
}

exported String8
Str8Prefix(String8 str, U64 size) {
   String8 result = str8_prefix(str, size);
   return result;
}

exported String8
Str8Skip(String8 str, U64 amt) {
   String8 result = str8_skip(str, amt);
   return result;
}

exported String8
Str8Postfix(String8 str, U64 size) {
   String8 result = str8_postfix(str, size);
   return result;
}

exported String8
Str8SkipChopWhitespace(String8 string) {
   String8 result = str8_skip_chop_whitespace(string);
   return result;
}

exported String8
Str8SkipLastSlash(String8 string) {
   String8 result = str8_skip_last_slash(string);
   return result;
}

exported String8
Str8ChopLastSlash(String8 string) {
   String8 result = str8_chop_last_slash(string);
   return result;
}

exported String8
Str8SkipLastDot(String8 string) {
   String8 result = str8_skip_last_dot(string);
   return result;
}

////////////////////////////////
//~ bgz: Allocation

exported String8
PushStr8Copy(Arena* arena, String8 s) {
   String8 result = str8_copy(arena, s);
   return result;
}

exported String8
PushStr8FV(Arena* arena, char* fmt, va_list args) {
   String8 result = str8fv(arena, fmt, args);
   return result;
}

exported String8
PushStr8F(Arena* arena, char* fmt, ...) {
   va_list args;
   va_start(args, fmt);
   String8 result = str8fv(arena, fmt, args);
   va_end(args);
   return result;
}

////////////////////////////////
//~ bgz: Lists

exported String8Node*
Str8ListPush(Arena* arena, String8List* list, String8 string) {
   String8Node* result = str8_list_push(arena, list, string);
   return result;
}

exported String8Node*
Str8ListPushF(Arena* arena, String8List* list, char* fmt, ...) {
   va_list args;
   va_start(args, fmt);
   String8 string = str8fv(arena, fmt, args);
   va_end(args);
   String8Node* result = str8_list_push(arena, list, string);
   return result;
}

exported String8List
Str8ListCopy(Arena* arena, String8List* list) {
   String8List result = str8_list_copy(arena, list);
   return result;
}

exported String8
Str8ListJoin(Arena* arena, String8List* list, StringJoin* optional_params) {
   String8 result = str8_list_join(arena, list, optional_params);
   return result;
}

exported String8Array
Str8ArrayFromList(Arena* arena, String8List* list) {
   String8Array result = str8_array_from_list(arena, list);
   return result;
}

////////////////////////////////
//~ bgz: Paths

exported String8
PathNormalizedFromString(Arena* arena, String8 path) {
   String8 result = path_normalized_from_string(arena, path);
   return result;
}

////////////////////////////////
//~ bgz: Unicode

exported String32
Str32From8(Arena* arena, String8 in) {
   String32 result = str32_from_8(arena, in);
   return result;
}

exported String8
Str8From32(Arena* arena, String32 in) {
   String8 result = str8_from_32(arena, in);
   return result;
}

////////////////////////////////
//~ bgz: Hashes

exported U64
U64HashFromSeedStr8(U64 seed, String8 string) {
   U64 result = u64_hash_from_seed_str8(seed, string);
   return result;
}

exported U64
U64HashFromStr8(String8 string) {
   U64 result = u64_hash_from_str8(string);
   return result;
}

////////////////////////////////
//~ bgz: Fuzzy Matching

exported FuzzyMatchRangeList
FuzzyMatchFind(Arena* arena, String8 needle, String8 haystack) {
   FuzzyMatchRangeList result = fuzzy_match_find(arena, needle, haystack);
   return result;
}

exported FuzzyMatchRangeList
FuzzyMatchRangeListCopy(Arena* arena, FuzzyMatchRangeList* src) {
   FuzzyMatchRangeList result = fuzzy_match_range_list_copy(arena, src);
   return result;
}
