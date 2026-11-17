////////////////////////////////
//~ bgz: search
//
// See search.h.
//
// The loaded frames are kept per video, for the library's generation: a new
// one empties them and they are loaded again as the indexer says each
// video's file is up to date. The vectors stay F16, as on disk; a table
// turns each into F32 as it is multiplied.
//
// A video's sound is kept the same way beside its frames, and searched the
// same way with the sound model's vector of the query: there a "frame" is a
// stretch of sound.

// Words that say nothing about a sound. What a stretch of sound scores for
// them, it scores for anything, and that is taken from its score
// (sound_base, search.h).
read_only global String8 _srch_sound_base_texts[] = {
    Str8LitComp("a sound"),
    Str8LitComp("audio"),
    Str8LitComp("a recording"),
    Str8LitComp("something"),
    Str8LitComp("a video"),
    Str8LitComp("a"),
    Str8LitComp("the"),
    Str8LitComp("noise"),
    Str8LitComp("a photo of something"),
    Str8LitComp("this is a sound"),
};

// One index file, loaded: a video's frames, or its stretches of sound.
typedef struct _SRCH_Track _SRCH_Track;
struct _SRCH_Track {
   B32  loaded;
   U64  count;
   F32* times;
   U16* vectors; // count by EMB_DIM
};

typedef struct _SRCH_Video _SRCH_Video;
struct _SRCH_Video {
   _SRCH_Track pictures;
   _SRCH_Track sound;
};

struct SRCH_Search {
   Arena*     arena;  // the loaded frames', emptied with each library generation
   String8    index_folder;
   EMB_Model* model;
   B32        model_loaded;
   B32        sound_model_loaded;
   F32        sound_base[EMB_DIM]; // the mean of _srch_sound_base_texts' vectors

   U64          library_gen;
   _SRCH_Video* videos;
   U64          video_count;
   U64          frame_count;
   U64          sound_count; // stretches of sound
   F64          picture_sum[EMB_DIM]; // of every loaded frame's vector
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
      MemoryZeroArray(search->picture_sum);
      search->sound_count = 0;
      search->generation += 1;
   }

   if (!search->model_loaded && EMB_IsLoaded(search->model)) {
      search->model_loaded = 1;
      search->generation += 1;
   }

   if (!search->sound_model_loaded && EMB_SoundIsLoaded(search->model)) {
      search->sound_model_loaded = 1;
      search->generation += 1;
      MemoryZeroArray(search->sound_base);
      for (U64 i = 0; i < ArrayCount(_srch_sound_base_texts); i += 1) {
         F32 v[EMB_DIM];
         if (EMB_SoundTextVector(search->model, _srch_sound_base_texts[i], v)) {
            for (U64 j = 0; j < EMB_DIM; j += 1) search->sound_base[j] += v[j] / (F32)ArrayCount(_srch_sound_base_texts);
         }
      }
   }

   for (U64 idx = 0; idx < search->video_count; idx += 1) {
      _SRCH_Video* video = &search->videos[idx];
      for (U64 is_sound = 0; is_sound < 2; is_sound += 1) {
         _SRCH_Track* track = is_sound ? &video->sound : &video->pictures;
         B32          indexed = is_sound ? IDX_SoundIsIndexed(indexer, library->generation, idx)
                                         : IDX_IsIndexed(indexer, library->generation, idx);
         if (!track->loaded && indexed) {
            Temp    scratch = ScratchBegin(0, 0);
            String8 path = INX_FilePath(scratch.arena, search->index_folder, library->videos[idx].path,
                                        is_sound ? INX_SOUND : INX_PICTURES);
            // Loaded or not, it is not tried again: a file the indexer has just
            // written that can't be read would only fail again.
            track->loaded = 1;
            if (INX_Read(search->arena, path, &track->count, &track->times, &track->vectors)) {
               *(is_sound ? &search->sound_count : &search->frame_count) += track->count;
               for (U64 i = 0; !is_sound && i < track->count * EMB_DIM; i += 1) {
                  search->picture_sum[i % EMB_DIM] += search->f32_from_f16[track->vectors[i]];
               }
               search->generation += 1;
            } else {
               LogUserErrorF("Could not read %S, the index of %S\n", path, library->videos[idx].path);
            }
            ScratchEnd(scratch);
         }
      }
   }
}

internal void
SRCH_Forget(SRCH_Search* search) {
   search->library_gen = max_U64;
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

// The clips of one video's frames, or of its stretches of sound, that match
// q, into hits. base is taken from each one's dot product with q. A score is the dot product stretched so that 0 is no match
// and 1 a good one (search.h), which is what lets clips found by picture and
// by sound go in one list.
internal void
_SRCH_Clips(SRCH_Search* search, SRCH_Hit* hits, U64* count, U64 video_idx, _SRCH_Track* track, F32* q, F32 base, B32 is_sound) {
   Temp scratch = ScratchBegin(0, 0);
   F32* table = search->f32_from_f16;
   F32  low = is_sound ? SRCH_SOUND_LOW : SRCH_PICTURE_LOW;
   F32  high = is_sound ? SRCH_SOUND_HIGH : SRCH_PICTURE_HIGH;
   F32  margin = (is_sound ? SRCH_SOUND_MARGIN : SRCH_CLIP_MARGIN) / (high - low);
   F64  step = is_sound ? IDX_SOUND_SECONDS : IDX_SAMPLE_SECONDS;

   _SRCH_Frame* frames = PushArrayNoZero(scratch.arena, _SRCH_Frame, track->count);
   S64*         clip_of = PushArrayNoZero(scratch.arena, S64, track->count);
   for (U64 i = 0; i < track->count; i += 1) {
      U16* v = track->vectors + i * EMB_DIM;
      F32  dot = 0;
      for (U64 j = 0; j < EMB_DIM; j += 1) dot += q[j] * table[v[j]];
      frames[i].idx = i;
      frames[i].score = (dot - base - low) / (high - low);
      clip_of[i] = -1;
   }

   // The clips: the best frame not yet in one starts one, which takes in its
   // neighbours on each side for as long as they score nearly as well as it
   // does and come straight after one another (silence is left out of the
   // sound, so its neighbours may not). So each frame is in one clip, and a
   // clip's score is its best frame's.
   quick_sort(frames, track->count, sizeof(_SRCH_Frame), _SRCH_FrameIsWorse);
   F32* scores = PushArrayNoZero(scratch.arena, F32, track->count);
   for (U64 i = 0; i < track->count; i += 1) scores[frames[i].idx] = frames[i].score;
   F32* times = track->times;
   F32  gap = (F32)(step * 1.5);

   for (U64 k = 0; k < track->count; k += 1) {
      U64 best = frames[k].idx;
      F32 score = frames[k].score;
      if (clip_of[best] != -1) {
         continue;
      }
      if (*count == SRCH_MAX_HITS && score <= hits[*count - 1].score) {
         break; // the rest are worse still
      }
      U64 first = best;
      U64 last = best;
      clip_of[best] = (S64)k;
      while (first > 0 && clip_of[first - 1] == -1 && scores[first - 1] >= score - margin &&
             times[first] - times[first - 1] <= gap) {
         first -= 1;
         clip_of[first] = (S64)k;
      }
      while (last + 1 < track->count && clip_of[last + 1] == -1 && scores[last + 1] >= score - margin &&
             times[last + 1] - times[last] <= gap) {
         last += 1;
         clip_of[last] = (S64)k;
      }

      // From its first frame to the end of its last. A clip found by its
      // frames is padded by a second each side, so it shows what led up to
      // it and what came after; a stretch of sound is long enough as it is.
      F64 pad = is_sound ? 0.0 : 1.0;
      F64 video_end = times[track->count - 1] + step;

      SRCH_Hit hit = {0};
      hit.video_idx = video_idx;
      hit.start_s = Max(0.0, (F64)times[first] - pad);
      hit.end_s = Min(video_end, (F64)times[last] + step + pad);
      hit.best_s = is_sound ? times[best] + step * 0.5 : times[best];
      hit.score = score;
      hit.by_sound = is_sound;
      _SRCH_Insert(hits, count, hit);
   }
   ScratchEnd(scratch);
}

internal SRCH_Results
SRCH_Run(Arena* arena, SRCH_Search* search, LIB_Library* library, String8 query) {
   SRCH_Results result = {0};
   F32          q[EMB_DIM];
   F32          sound_q[EMB_DIM];
   B32          current = (search->library_gen == library->generation);
   B32          by_pictures = (query.size != 0 && current && search->frame_count != 0 && EMB_TextVector(search->model, query, q));
   B32          by_sound =
       (query.size != 0 && current && search->sound_count != 0 && EMB_SoundTextVector(search->model, query, sound_q));
   if (query.size == 0) {
      // No hits.
   } else if (!by_pictures && !by_sound) {
      result = _SRCH_ByName(arena, library, query);
   } else {
      Temp      scratch = ScratchBegin(&arena, 1);
      SRCH_Hit* found = PushArray(scratch.arena, SRCH_Hit, SRCH_MAX_HITS);
      U64       found_count = 0;
      for (U64 j = 0; j < EMB_DIM; j += 1) sound_q[j] -= search->sound_base[j];
      // What the frames score for the query on average: its dot product
      // with their mean.
      F64 mean = 0;
      for (U64 j = 0; by_pictures && j < EMB_DIM; j += 1) mean += q[j] * search->picture_sum[j] / (F64)search->frame_count;
      F32 picture_base = (F32)(SRCH_PICTURE_MEAN_SHARE * mean);
      for (U64 video_idx = 0; video_idx < search->video_count; video_idx += 1) {
         _SRCH_Video* video = &search->videos[video_idx];
         if (by_pictures && video->pictures.count != 0) {
            _SRCH_Clips(search, found, &found_count, video_idx, &video->pictures, q, picture_base, 0);
         }
         if (by_sound && video->sound.count != 0) {
            _SRCH_Clips(search, found, &found_count, video_idx, &video->sound, sound_q, 0, 1);
         }
      }

      // A clip of frames and a clip of sound that both match well and are at
      // the same time in the same video are one hit: each with the best of
      // the other kind that it overlaps, the best clips choosing first. It
      // runs from where the first of them starts to where the last ends,
      // and its best moment is the frames'.
      SRCH_Hit* hits = PushArray(arena, SRCH_Hit, SRCH_MAX_HITS);
      U64       count = 0;
      B8*       taken = PushArray(scratch.arena, B8, SRCH_MAX_HITS);
      for (U64 i = 0; i < found_count; i += 1) {
         if (taken[i]) {
            continue;
         }
         SRCH_Hit hit = found[i];
         for (U64 k = i + 1; k < found_count && found[k].score >= SRCH_BOTH_FLOOR; k += 1) {
            SRCH_Hit* other = &found[k];
            if (!taken[k] && other->video_idx == hit.video_idx && other->by_sound != hit.by_sound &&
                other->start_s < hit.end_s && hit.start_s < other->end_s) {
               taken[k] = 1;
               hit.best_s = hit.by_sound ? other->best_s : hit.best_s;
               hit.start_s = Min(hit.start_s, other->start_s);
               hit.end_s = Max(hit.end_s, other->end_s);
               hit.score += SRCH_BOTH_SHARE * other->score;
               hit.by_sound = 0;
               hit.by_both = 1;
               break;
            }
         }
         _SRCH_Insert(hits, &count, hit);
      }
      ScratchEnd(scratch);

      // The clips far below the best found the way they were are not hits:
      // by its frames, by its sound and by both are each their own way, so a
      // good clip of sound does not put out the best of the frames.
      F32 best[3] = {0};
      B32 has_best[3] = {0};
      U64 kept = 0;
      for (U64 i = 0; i < count; i += 1) {
         U64 way = hits[i].by_both ? 2 : hits[i].by_sound ? 1 : 0;
         if (!has_best[way]) {
            has_best[way] = 1;
            best[way] = hits[i].score;
         }
         if (hits[i].score >= SRCH_KEEP * Min(best[way], 1.0f)) {
            hits[kept++] = hits[i];
         }
      }
      count = kept;
      result.v = hits;
      result.count = count;
   }
   return result;
}
