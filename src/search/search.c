////////////////////////////////
//~ bgz: search
//
// See search.h. For now a stand-in that matches file names: a video is a hit
// for each word of the query that is in its name, and the whole video is
// the clip. It is replaced by the index of what is in the frames - see
// Claude outputs\clip_search_plan.md.

internal SRCH_Results
SRCH_Run(Arena* arena, LIB_Library* library, String8 query) {
   SRCH_Results result = {0};
   Temp         scratch = ScratchBegin(&arena, 1);

   // The query's words: what is between spaces.
   String8List words = {0};
   {
      U64 pos = 0;
      while (pos < query.size) {
         while (pos < query.size && CharIsSpace(query.str[pos])) pos += 1;
         U64 start = pos;
         while (pos < query.size && !CharIsSpace(query.str[pos])) pos += 1;
         if (pos > start) {
            Str8ListPush(scratch.arena, &words, Str8Substr(query, R1U64(start, pos)));
         }
      }
   }

   if (words.node_count != 0) {
      SRCH_Hit* hits = PushArray(arena, SRCH_Hit, SRCH_MAX_HITS);
      U64       count = 0;

      for (U64 video_idx = 0; video_idx < library->video_count; video_idx += 1) {
         String8 name = library->videos[video_idx].name;

         U64 found = 0;
         for (String8Node* word = words.first; word != 0; word = word->next) {
            if (Str8FindNeedle(name, 0, word->string, StringMatchFlag_CaseInsensitive) < name.size) {
               found += 1;
            }
         }
         if (found == 0) {
            continue;
         }

         SRCH_Hit hit = {0};
         hit.video_idx = video_idx;
         hit.score = (F32)found / (F32)words.node_count;

         // Into its place, best first. With the hits full, one no better
         // than the last of them is left out.
         U64 at = count;
         while (at > 0 && hits[at - 1].score < hit.score) at -= 1;
         if (at < SRCH_MAX_HITS) {
            U64 move = Min(count, SRCH_MAX_HITS - 1) - at;
            MemoryCopy(&hits[at + 1], &hits[at], move * sizeof(SRCH_Hit));
            hits[at] = hit;
            count = Min(count + 1, SRCH_MAX_HITS);
         }
      }

      result.v = hits;
      result.count = count;
   }

   ScratchEnd(scratch);
   return result;
}
