////////////////////////////////
//~ bgz: search
//
// See search.h.
//
// The loaded frames are kept per video, for the library's generation: a new
// one empties them and they are loaded again as the indexer says each
// video's file is up to date. The vectors stay F16, as on disk; a table
// turns each into F32 as it is multiplied.

typedef struct _SRCH_Video _SRCH_Video;
struct _SRCH_Video {
   B32  loaded;
   U64  count;
   F32* times;
   U16* vectors; // count by EMB_DIM
};

struct SRCH_Search {
   Arena*     arena;  // the loaded frames', emptied with each library generation
   String8    index_folder;
   EMB_Model* model;
   B32        model_loaded;

   U64          library_gen;
   _SRCH_Video* videos;
   U64          video_count;
   U64          frame_count;
   U64          generation;

   F32 f32_from_f16[1 << 16];
};

internal SRCH_Search*
SRCH_Alloc(String8 index_folder, EMB_Model* model) {
   Arena*       arena = ArenaAlloc();
   SRCH_Search* search = PushArray(arena, SRCH_Search, 1);
   search->arena = ArenaAlloc();
   search->index_folder = PushStr8Copy(arena, index_folder);
   search->model = model;
   search->library_gen = max_U64;
   for (U64 h = 0; h < ArrayCount(search->f32_from_f16); h += 1) {
      search->f32_from_f16[h] = INX_F32FromF16((U16)h);
   }
   return search;
}

internal void
SRCH_Update(SRCH_Search* search, LIB_Library* library, IDX_Indexer* indexer) {
   if (search->library_gen != library->generation) {
      ArenaClear(search->arena);
      search->library_gen = library->generation;
      search->video_count = library->video_count;
      search->videos = PushArray(search->arena, _SRCH_Video, search->video_count);
      search->frame_count = 0;
      search->generation += 1;
   }

   if (!search->model_loaded && EMB_IsLoaded(search->model)) {
      search->model_loaded = 1;
      search->generation += 1;
   }

   for (U64 idx = 0; idx < search->video_count; idx += 1) {
      _SRCH_Video* video = &search->videos[idx];
      if (!video->loaded && IDX_IsIndexed(indexer, library->generation, idx)) {
         Temp    scratch = ScratchBegin(0, 0);
         String8 path = INX_FilePath(scratch.arena, search->index_folder, library->videos[idx].path);
         // Loaded or not, it is not tried again: a file the indexer has just
         // written that can't be read would only fail again.
         video->loaded = 1;
         if (INX_Read(search->arena, path, &video->count, &video->times, &video->vectors)) {
            search->frame_count += video->count;
            search->generation += 1;
         } else {
            LogUserErrorF("Could not read %S, the index of %S\n", path, library->videos[idx].path);
         }
         ScratchEnd(scratch);
      }
   }
}

internal U64
SRCH_Generation(SRCH_Search* search) {
   return search->generation;
}

// A frame of a video and how well it matched, for sorting best first.
typedef struct _SRCH_Frame _SRCH_Frame;
struct _SRCH_Frame {
   U64 idx;
   F32 score;
};

internal int
_SRCH_FrameIsWorse(_SRCH_Frame* a, _SRCH_Frame* b) {
   return (a->score < b->score) ? 1 : (a->score > b->score) ? -1 : 0;
}

// Into its place in hits, best first. With the hits full, one no better than
// the last of them is left out.
internal void
_SRCH_Insert(SRCH_Hit* hits, U64* count, SRCH_Hit hit) {
   U64 at = *count;
   while (at > 0 && hits[at - 1].score < hit.score) at -= 1;
   if (at < SRCH_MAX_HITS) {
      U64 move = Min(*count, SRCH_MAX_HITS - 1) - at;
      MemoryCopy(&hits[at + 1], &hits[at], move * sizeof(SRCH_Hit));
      hits[at] = hit;
      *count = Min(*count + 1, SRCH_MAX_HITS);
   }
}

// A video is a hit for each word of the query that is in its name, and the
// whole video is the clip.
internal SRCH_Results
_SRCH_ByName(Arena* arena, LIB_Library* library, String8 query) {
   SRCH_Results result = {0};
   result.by_name = 1;
   Temp scratch = ScratchBegin(&arena, 1);

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
         if (found != 0) {
            SRCH_Hit hit = {0};
            hit.video_idx = video_idx;
            hit.score = (F32)found / (F32)words.node_count;
            _SRCH_Insert(hits, &count, hit);
         }
      }

      result.v = hits;
      result.count = count;
   }

   ScratchEnd(scratch);
   return result;
}

internal SRCH_Results
SRCH_Run(Arena* arena, SRCH_Search* search, LIB_Library* library, String8 query) {
   SRCH_Results result = {0};
   F32          q[EMB_DIM];
   if (query.size == 0) {
      // No hits.
   } else if (search->frame_count == 0 || search->library_gen != library->generation ||
              !EMB_TextVector(search->model, query, q)) {
      result = _SRCH_ByName(arena, library, query);
   } else {
      SRCH_Hit* hits = PushArray(arena, SRCH_Hit, SRCH_MAX_HITS);
      U64       count = 0;
      F32*      table = search->f32_from_f16;
      for (U64 video_idx = 0; video_idx < search->video_count; video_idx += 1) {
         _SRCH_Video* video = &search->videos[video_idx];
         if (video->count == 0) {
            continue;
         }
         Temp scratch = ScratchBegin(&arena, 1);

         _SRCH_Frame* frames = PushArrayNoZero(scratch.arena, _SRCH_Frame, video->count);
         S64*         clip_of = PushArrayNoZero(scratch.arena, S64, video->count);
         for (U64 i = 0; i < video->count; i += 1) {
            U16* v = video->vectors + i * EMB_DIM;
            F32  score = 0;
            for (U64 j = 0; j < EMB_DIM; j += 1) score += q[j] * table[v[j]];
            frames[i].idx = i;
            frames[i].score = score;
            clip_of[i] = -1;
         }

         // The clips: the best frame not yet in one starts one, which takes
         // in its neighbours on each side for as long as they score nearly
         // as well as it does. So each frame is in one clip, and a clip's
         // score is its best frame's.
         quick_sort(frames, video->count, sizeof(_SRCH_Frame), _SRCH_FrameIsWorse);
         F32* scores = PushArrayNoZero(scratch.arena, F32, video->count);
         for (U64 i = 0; i < video->count; i += 1) scores[frames[i].idx] = frames[i].score;

         for (U64 k = 0; k < video->count; k += 1) {
            U64 best = frames[k].idx;
            F32 score = frames[k].score;
            if (clip_of[best] != -1) {
               continue;
            }
            if (count == SRCH_MAX_HITS && score <= hits[count - 1].score) {
               break; // the rest are worse still
            }
            U64 first = best;
            U64 last = best;
            clip_of[best] = (S64)k;
            while (first > 0 && clip_of[first - 1] == -1 && scores[first - 1] >= score - SRCH_CLIP_MARGIN) {
               first -= 1;
               clip_of[first] = (S64)k;
            }
            while (last + 1 < video->count && clip_of[last + 1] == -1 && scores[last + 1] >= score - SRCH_CLIP_MARGIN) {
               last += 1;
               clip_of[last] = (S64)k;
            }

            // From its first frame to the end of its last (the next frame's
            // time), padded by a second each side so the clip shows what led
            // up to it and what came after.
            F64 end = (last + 1 < video->count) ? video->times[last + 1] : video->times[last] + IDX_SAMPLE_SECONDS;
            F64 video_end = video->times[video->count - 1] + IDX_SAMPLE_SECONDS;

            SRCH_Hit hit = {0};
            hit.video_idx = video_idx;
            hit.start_s = Max(0.0, (F64)video->times[first] - 1.0);
            hit.end_s = Min(video_end, end + 1.0);
            hit.best_s = video->times[best];
            hit.score = score;
            _SRCH_Insert(hits, &count, hit);
         }
         ScratchEnd(scratch);
      }
      result.v = hits;
      result.count = count;
   }
   return result;
}
