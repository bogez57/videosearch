////////////////////////////////
//~ bgz: embed: CLIP's tokenizer
//
// Words into the numbers the text model reads: OpenAI CLIP's
// simple_tokenizer.py, done in C. The text is made lower case and cut into
// pieces the way its pattern cuts it - runs of letters, single digits, runs
// of anything else, and 's 't 're 've 'm 'll 'd - and each piece is
// byte-pair encoded with the merges in data\clip\bpe_merges.txt.
//
// Where it differs from the Python, on purpose:
//   - No ftfy and no html unescaping: what is typed in a search bar is
//     neither broken Unicode nor HTML.
//   - Only A to Z are made lower case.
//   - A character outside ASCII is a letter, unless it is punctuation from
//     Latin-1 (U+00A1 to U+00BF, but not the letters ª µ º) or the General
//     Punctuation block (U+2000 to U+206F). Python asks Unicode's tables.
//   - Too long a text is cut to fit, as open_clip does, instead of refused.

// The merges the vocabulary is made of: the file's lines after its first.
#define _EMB_MERGE_COUNT (49152 - 256 - 2)

typedef struct _EMB_Slot _EMB_Slot;
struct _EMB_Slot {
   String8 key;   // empty: free
   S32     value;
};

// String to number, by open addressing. cap is a power of two.
typedef struct _EMB_Map _EMB_Map;
struct _EMB_Map {
   _EMB_Slot* slots;
   U64        cap;
};

internal void
_EMB_MapPut(_EMB_Map* map, String8 key, S32 value) {
   U64 idx = U64HashFromStr8(key) & (map->cap - 1);
   while (map->slots[idx].key.size != 0 && !Str8Match(map->slots[idx].key, key, 0)) {
      idx = (idx + 1) & (map->cap - 1);
   }
   map->slots[idx].key = key;
   map->slots[idx].value = value;
}

// -1 if key is not there.
internal S32
_EMB_MapGet(_EMB_Map* map, String8 key) {
   S32 result = -1;
   U64 idx = U64HashFromStr8(key) & (map->cap - 1);
   while (map->slots[idx].key.size != 0) {
      if (Str8Match(map->slots[idx].key, key, 0)) {
         result = map->slots[idx].value;
         break;
      }
      idx = (idx + 1) & (map->cap - 1);
   }
   return result;
}

struct EMB_Tokenizer {
   String8  byte_chars[256]; // each byte as the character that stands for it, in UTF-8
   _EMB_Map ids;             // a piece of a word to its number
   _EMB_Map ranks;           // "a b" to how early the merge of a and b comes
};

internal String8
_EMB_Utf8FromCodepoint(Arena* arena, U32 c) {
   U8* s = PushArray(arena, U8, 4);
   U64 size = 0;
   if (c < 0x80) {
      s[0] = (U8)c;
      size = 1;
   } else if (c < 0x800) {
      s[0] = (U8)(0xC0 | (c >> 6));
      s[1] = (U8)(0x80 | (c & 0x3F));
      size = 2;
   } else {
      s[0] = (U8)(0xE0 | (c >> 12));
      s[1] = (U8)(0x80 | ((c >> 6) & 0x3F));
      s[2] = (U8)(0x80 | (c & 0x3F));
      size = 3;
   }
   return Str8(s, size);
}

internal EMB_Tokenizer*
EMB_TokenizerFromMerges(Arena* arena, String8 merges_text) {
   EMB_Tokenizer* tok = PushArray(arena, EMB_Tokenizer, 1);
   tok->ids.cap = 1 << 17;
   tok->ids.slots = PushArray(arena, _EMB_Slot, tok->ids.cap);
   tok->ranks.cap = 1 << 17;
   tok->ranks.slots = PushArray(arena, _EMB_Slot, tok->ranks.cap);

   //- bgz: the bytes' characters, in the Python's order: the printable ones
   // stand for themselves, the rest for 256 and up.
   U8  order[256];
   U64 order_count = 0;
   {
      B32 is_printable[256] = {0};
      for (U32 b = '!'; b <= '~'; b += 1) is_printable[b] = 1;
      for (U32 b = 0xA1; b <= 0xAC; b += 1) is_printable[b] = 1;
      for (U32 b = 0xAE; b <= 0xFF; b += 1) is_printable[b] = 1;
      for (U32 b = 0; b < 256; b += 1) {
         if (is_printable[b]) {
            tok->byte_chars[b] = _EMB_Utf8FromCodepoint(arena, b);
            order[order_count++] = (U8)b;
         }
      }
      U32 n = 0;
      for (U32 b = 0; b < 256; b += 1) {
         if (!is_printable[b]) {
            tok->byte_chars[b] = _EMB_Utf8FromCodepoint(arena, 256 + n);
            order[order_count++] = (U8)b;
            n += 1;
         }
      }
   }

   //- bgz: the vocabulary: each byte, each byte ending a word, each merge,
   // and the start and end of the text.
   S32 id = 0;
   for (U64 i = 0; i < 256; i += 1) {
      _EMB_MapPut(&tok->ids, tok->byte_chars[order[i]], id++);
   }
   for (U64 i = 0; i < 256; i += 1) {
      _EMB_MapPut(&tok->ids, PushStr8F(arena, "%S</w>", tok->byte_chars[order[i]]), id++);
   }

   U64 pos = 0;
   while (pos < merges_text.size && merges_text.str[pos] != '\n') pos += 1; // the version line
   pos += 1;
   for (S32 rank = 0; rank < _EMB_MERGE_COUNT && pos < merges_text.size; rank += 1) {
      U64 start = pos;
      while (pos < merges_text.size && merges_text.str[pos] != '\n') pos += 1;
      String8 line = Str8Substr(merges_text, R1U64(start, pos));
      pos += 1;
      if (line.size != 0 && line.str[line.size - 1] == '\r') line.size -= 1;

      U64 space = Str8FindNeedle(line, 0, Str8Lit(" "), 0);
      if (space >= line.size) {
         continue;
      }
      String8 a = Str8Prefix(line, space);
      String8 b = Str8Skip(line, space + 1);
      _EMB_MapPut(&tok->ranks, PushStr8Copy(arena, line), rank);
      _EMB_MapPut(&tok->ids, PushStr8F(arena, "%S%S", a, b), id++);
   }
   return tok;
}

//- bgz: what a character is, for cutting the text into pieces

typedef enum _EMB_CharKind {
   _EMB_CharKind_Space,
   _EMB_CharKind_Letter,
   _EMB_CharKind_Digit,
   _EMB_CharKind_Other,
} _EMB_CharKind;

internal _EMB_CharKind
_EMB_KindFromCodepoint(U32 c) {
   _EMB_CharKind result = _EMB_CharKind_Other;
   if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') {
      result = _EMB_CharKind_Space;
   } else if (c >= '0' && c <= '9') {
      result = _EMB_CharKind_Digit;
   } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
      result = _EMB_CharKind_Letter;
   } else if (c >= 0x80) {
      B32 is_punctuation = ((c >= 0xA0 && c <= 0xBF && c != 0xAA && c != 0xB5 && c != 0xBA) ||
                            (c >= 0x2000 && c <= 0x206F) || c == 0xD7 || c == 0xF7);
      result = is_punctuation ? _EMB_CharKind_Other : _EMB_CharKind_Letter;
   }
   return result;
}

// The character at text[pos], and how many bytes it is. A byte that does not
// start a character is one by itself.
internal U32
_EMB_DecodeUtf8(String8 text, U64 pos, U64* size_out) {
   U8  c = text.str[pos];
   U32 result = c;
   U64 size = 1;
   if (c >= 0xC0 && c < 0xE0 && pos + 1 < text.size) {
      result = ((U32)(c & 0x1F) << 6) | (text.str[pos + 1] & 0x3F);
      size = 2;
   } else if (c >= 0xE0 && c < 0xF0 && pos + 2 < text.size) {
      result = ((U32)(c & 0x0F) << 12) | ((U32)(text.str[pos + 1] & 0x3F) << 6) | (text.str[pos + 2] & 0x3F);
      size = 3;
   } else if (c >= 0xF0 && pos + 3 < text.size) {
      result = ((U32)(c & 0x07) << 18) | ((U32)(text.str[pos + 1] & 0x3F) << 12) |
               ((U32)(text.str[pos + 2] & 0x3F) << 6) | (text.str[pos + 3] & 0x3F);
      size = 4;
   }
   *size_out = size;
   return result;
}

internal _EMB_CharKind
_EMB_KindAt(String8 text, U64 pos, U64* size_out) {
   return _EMB_KindFromCodepoint(_EMB_DecodeUtf8(text, pos, size_out));
}

// Byte-pair encodes one piece and puts its numbers after ids[*count], up to
// cap of them.
internal void
_EMB_EncodePiece(EMB_Tokenizer* tok, String8 piece, S32* ids, U64* count, U64 cap) {
   Temp scratch = ScratchBegin(0, 0);

   // The piece as symbols, one a byte, the last marked as a word's end.
   U64      symbol_count = piece.size;
   String8* symbols = PushArray(scratch.arena, String8, symbol_count);
   for (U64 i = 0; i < piece.size; i += 1) {
      symbols[i] = tok->byte_chars[piece.str[i]];
   }
   symbols[symbol_count - 1] = PushStr8F(scratch.arena, "%S</w>", symbols[symbol_count - 1]);

   // The pair whose merge comes first is merged wherever it is, until no
   // pair has a merge.
   while (symbol_count > 1) {
      S32 best_rank = -1;
      U64 best_idx = 0;
      for (U64 i = 0; i + 1 < symbol_count; i += 1) {
         String8 pair = PushStr8F(scratch.arena, "%S %S", symbols[i], symbols[i + 1]);
         S32     rank = _EMB_MapGet(&tok->ranks, pair);
         if (rank >= 0 && (best_rank < 0 || rank < best_rank)) {
            best_rank = rank;
            best_idx = i;
         }
      }
      if (best_rank < 0) {
         break;
      }

      String8 first = symbols[best_idx];
      String8 second = symbols[best_idx + 1];
      String8 merged = PushStr8F(scratch.arena, "%S%S", first, second);
      U64     out = 0;
      for (U64 i = 0; i < symbol_count;) {
         if (i + 1 < symbol_count && Str8Match(symbols[i], first, 0) && Str8Match(symbols[i + 1], second, 0)) {
            symbols[out++] = merged;
            i += 2;
         } else {
            symbols[out++] = symbols[i];
            i += 1;
         }
      }
      symbol_count = out;
   }

   for (U64 i = 0; i < symbol_count && *count < cap; i += 1) {
      S32 id = _EMB_MapGet(&tok->ids, symbols[i]);
      if (id >= 0) {
         ids[(*count)++] = id;
      }
   }

   ScratchEnd(scratch);
}

internal void
EMB_Tokenize(EMB_Tokenizer* tok, String8 text, S32 ids_out[EMB_TEXT_TOKENS]) {
   MemoryZero(ids_out, sizeof(S32) * EMB_TEXT_TOKENS);
   Temp scratch = ScratchBegin(0, 0);

   // Lower case, A to Z only.
   String8 lower = PushStr8Copy(scratch.arena, text);
   for (U64 i = 0; i < lower.size; i += 1) {
      if (lower.str[i] >= 'A' && lower.str[i] <= 'Z') lower.str[i] += 'a' - 'A';
   }

   // Room for the start and the end.
   U64 count = 0;
   U64 cap = EMB_TEXT_TOKENS - 1;
   ids_out[count++] = EMB_TOKEN_START;

   U64 pos = 0;
   while (pos < lower.size && count < cap) {
      U64           size = 0;
      _EMB_CharKind kind = _EMB_KindAt(lower, pos, &size);
      U64           start = pos;

      if (kind == _EMB_CharKind_Space) {
         pos += size;
         continue;
      }

      // 's 't 're 've 'm 'll 'd, which come before the other kinds in the
      // pattern.
      if (lower.str[pos] == '\'') {
         String8 rest = Str8Skip(lower, pos + 1);
         String8 endings[] = {Str8Lit("s"), Str8Lit("t"), Str8Lit("re"), Str8Lit("ve"), Str8Lit("m"), Str8Lit("ll"), Str8Lit("d")};
         U64     ending_size = 0;
         for (U64 i = 0; i < ArrayCount(endings); i += 1) {
            if (rest.size >= endings[i].size && Str8Match(Str8Prefix(rest, endings[i].size), endings[i], 0)) {
               ending_size = endings[i].size;
               break;
            }
         }
         if (ending_size != 0) {
            pos += 1 + ending_size;
            _EMB_EncodePiece(tok, Str8Substr(lower, R1U64(start, pos)), ids_out, &count, cap);
            continue;
         }
      }

      if (kind == _EMB_CharKind_Digit) {
         pos += size;
      } else {
         // A run of the same kind: letters, or what is neither a letter, a
         // digit nor a space.
         while (pos < lower.size) {
            U64           next_size = 0;
            _EMB_CharKind next_kind = _EMB_KindAt(lower, pos, &next_size);
            if (next_kind != kind) break;
            pos += next_size;
         }
      }
      _EMB_EncodePiece(tok, Str8Substr(lower, R1U64(start, pos)), ids_out, &count, cap);
   }

   ids_out[count++] = EMB_TOKEN_END;
   ScratchEnd(scratch);
}
