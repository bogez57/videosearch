////////////////////////////////
//~ bgz: embed: CLAP's tokenizer
//
// Words into the numbers CLAP's text half reads. It is RoBERTa's tokenizer,
// which is GPT-2's: the same byte-pair encoding as CLIP's
// (embed_tokenizer.c, whose pieces this file uses), with these differences:
//   - A word's piece starts with the space before it, instead of ending
//     with </w>. A space is the character Ġ in the vocabulary.
//   - Capitals stay capitals.
//   - Digits are cut in runs, as letters are, not one by one.
//   - A piece's number is looked up in the vocabulary file (vocab.json),
//     not counted from the order of the merges.
//
// What is a letter is decided as in embed_tokenizer.c.

// The text of a JSON string that starts after the quote at json[*pos], with
// its escapes undone. *pos is left after the closing quote.
internal String8
_EMB_JsonString(Arena* arena, String8 json, U64* pos) {
   U64 at = *pos;
   U64 end = at;
   while (end < json.size && json.str[end] != '"') {
      end += (json.str[end] == '\\') ? 2 : 1;
   }
   end = Min(end, json.size);

   // Undone, it is never longer than it was.
   U8* out = PushArrayNoZero(arena, U8, end - at + 1);
   U64 size = 0;
   while (at < end) {
      U8 c = json.str[at++];
      if (c != '\\' || at >= end) {
         out[size++] = c;
         continue;
      }
      U8 e = json.str[at++];
      if (e == 'u' && at + 4 <= end) {
         U32 code = 0;
         for (U64 i = 0; i < 4; i += 1) {
            U8 h = json.str[at++];
            code = code * 16 + ((h >= '0' && h <= '9') ? (U32)(h - '0') : (U32)((h | 0x20) - 'a' + 10));
         }
         Temp    scratch = ScratchBegin(&arena, 1);
         String8 utf8 = _EMB_Utf8FromCodepoint(scratch.arena, code);
         MemoryCopy(out + size, utf8.str, utf8.size);
         size += utf8.size;
         ScratchEnd(scratch);
      } else {
         out[size++] = (e == 'n') ? '\n' : (e == 't') ? '\t' : (e == 'r') ? '\r' : (e == 'b') ? '\b' : (e == 'f') ? '\f' : e;
      }
   }
   *pos = Min(end + 1, json.size);
   return Str8(out, size);
}

internal EMB_Tokenizer*
EMB_SoundTokenizerFromVocab(Arena* arena, String8 vocab_json, String8 merges_text) {
   EMB_Tokenizer* tok = PushArray(arena, EMB_Tokenizer, 1);
   tok->ids.cap = 1 << 17;
   tok->ids.slots = PushArray(arena, _EMB_Slot, tok->ids.cap);
   tok->ranks.cap = 1 << 17;
   tok->ranks.slots = PushArray(arena, _EMB_Slot, tok->ranks.cap);

   U8 order[256];
   _EMB_SetByteChars(arena, tok, order);

   //- bgz: the vocabulary: one object, each piece to its number
   U64 pos = 0;
   while (pos < vocab_json.size) {
      if (vocab_json.str[pos] != '"') {
         pos += 1;
         continue;
      }
      pos += 1;
      String8 key = _EMB_JsonString(arena, vocab_json, &pos);
      while (pos < vocab_json.size && (vocab_json.str[pos] == ':' || CharIsSpace(vocab_json.str[pos]))) pos += 1;
      S32 id = 0;
      B32 has_id = 0;
      while (pos < vocab_json.size && vocab_json.str[pos] >= '0' && vocab_json.str[pos] <= '9') {
         id = id * 10 + (vocab_json.str[pos] - '0');
         has_id = 1;
         pos += 1;
      }
      if (has_id && key.size != 0 && id < 100000) {
         _EMB_MapPut(&tok->ids, key, id);
      }
   }

   //- bgz: the merges: the file's lines after its first, earliest first
   pos = 0;
   while (pos < merges_text.size && merges_text.str[pos] != '\n') pos += 1;
   pos += 1;
   for (S32 rank = 0; pos < merges_text.size && rank < 100000; rank += 1) {
      U64 start = pos;
      while (pos < merges_text.size && merges_text.str[pos] != '\n') pos += 1;
      String8 line = Str8Substr(merges_text, R1U64(start, pos));
      pos += 1;
      if (line.size != 0 && line.str[line.size - 1] == '\r') line.size -= 1;
      if (line.size != 0) {
         _EMB_MapPut(&tok->ranks, PushStr8Copy(arena, line), rank);
      }
   }
   return tok;
}

internal U64
EMB_SoundTokenize(EMB_Tokenizer* tok, String8 text, S32 ids_out[EMB_TEXT_TOKENS]) {
   // Room for the start and the end.
   U64 count = 0;
   U64 cap = EMB_TEXT_TOKENS - 1;
   ids_out[count++] = EMB_SOUND_TOKEN_START;

   // The pieces, as GPT-2's pattern cuts them:
   //    's 't 're 've 'm 'll 'd
   //    a run of letters, of digits, or of what is neither nor a space,
   //       with the one space before it if there is one
   //    spaces: all at the end of the text, and all but the last of a run
   //       of them before a word (the last goes with the word)
   U64 pos = 0;
   while (pos < text.size && count < cap) {
      U64           size = 0;
      _EMB_CharKind kind = _EMB_KindAt(text, pos, &size);
      U64           start = pos;

      if (text.str[pos] == '\'') {
         String8 rest = Str8Skip(text, pos + 1);
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
            _EMB_EncodePiece(tok, Str8Substr(text, R1U64(start, pos)), ids_out, &count, cap);
            continue;
         }
      }

      if (kind == _EMB_CharKind_Space) {
         U64 run_end = pos;
         U64 last = pos; // where the run's last space starts
         while (run_end < text.size) {
            U64 next_size = 0;
            if (_EMB_KindAt(text, run_end, &next_size) != _EMB_CharKind_Space) break;
            last = run_end;
            run_end += next_size;
         }
         if (run_end == text.size) {
            pos = run_end;
         } else if (last != pos) {
            pos = last;
         } else if (text.str[pos] == ' ') {
            // The one space before a word: the piece goes on through the word.
            pos += 1;
            kind = _EMB_KindAt(text, pos, &size);
         } else {
            pos = run_end;
         }
      }
      if (kind != _EMB_CharKind_Space) {
         while (pos < text.size) {
            U64 next_size = 0;
            if (_EMB_KindAt(text, pos, &next_size) != kind) break;
            pos += next_size;
         }
      }
      _EMB_EncodePiece(tok, Str8Substr(text, R1U64(start, pos)), ids_out, &count, cap);
   }

   ids_out[count++] = EMB_SOUND_TOKEN_END;
   return count;
}
