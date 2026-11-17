////////////////////////////////
//~ bgz: indexer
//
// See indexer.h.
//
// A run is one pass over the library's videos as they were when it started:
// their paths are copied onto the run's arena, so the library can change
// while the thread reads them. A change stops the run (cancel, then join)
// and starts a new one. What the thread writes and the main thread reads are
// the U64s below, through atomics, and the logs, under log_mutex; the paths
// do not change while it runs.
//
// The model is loaded by the first run's thread, since that takes a second
// or so, and kept for the runs after it.

struct IDX_Indexer {
   Arena*   arena; // the run's: emptied for each
   Thread   thread;
   B32      running; // a thread was started and not joined yet
   U64      library_gen;
   U64      started_us;
   U64      ended_us; // 0 until the run is over
   B32      reported; // the run's end has been logged

   String8* paths;
   String8* names; // pieces of paths
   U64      count;

   String8    index_folder;
   String8    model_name;
   String8    sound_model_name;
   B32        use_gpu; // videos are decoded on the graphics card where it can
   EMB_Model* model;       // 0: none could be opened
   B32        model_tried; // only the thread reads or writes it, and one runs at a time

   // What the thread has logged and the main thread has not yet. thread_log
   // is the thread's log, kept for every run's thread: there is no letting
   // go of one.
   Log*        thread_log;
   Mutex       log_mutex;
   Arena*      log_arena;
   String8List logs[LogMsgKind_COUNT];

   // Written by the thread, read by the main thread.
   // Only the thread writes these but cancel, so an add is a read and a write.
   U64 cancel;  // set by the main thread: the thread stops at the next frame
   U64 done;
   U64 failed;
   U64 up_to_date;
   U64 frames;
   U64 no_model;
   U64  current; // the video under way, plus one. 0: none
   U64* indexed; // one a video, set when its index file is up to date
   U64* sound_indexed; // and when its sound's is
};

internal void
_IDX_PassOnLogs(IDX_Indexer* indexer, LogScopeResult logs) {
   MutexTake(indexer->log_mutex);
   for (U64 kind = 0; kind < LogMsgKind_COUNT; kind += 1) {
      if (logs.strings[kind].size != 0) {
         Str8ListPush(indexer->log_arena, &indexer->logs[kind], PushStr8Copy(indexer->log_arena, logs.strings[kind]));
      }
   }
   MutexDrop(indexer->log_mutex);
}

// The batch's frames through the model, and their times and vectors (as F16)
// onto the lists.
internal B32
_IDX_Encode(IDX_Indexer* indexer, Arena* arena, F32* images, F32* times, U64 count, F32* vectors, String8List* times_out,
            String8List* vectors_out) {
   B32 ok = EMB_ImageVectors(indexer->model, images, count, vectors);
   if (ok) {
      F32* t = PushArrayNoZero(arena, F32, count);
      MemoryCopy(t, times, count * sizeof(F32));
      Str8ListPush(arena, times_out, Str8((U8*)t, count * sizeof(F32)));

      U16* v = PushArrayNoZero(arena, U16, count * EMB_DIM);
      for (U64 i = 0; i < count * EMB_DIM; i += 1) {
         v[i] = INX_F16FromF32(vectors[i]);
      }
      Str8ListPush(arena, vectors_out, Str8((U8*)v, count * EMB_DIM * sizeof(U16)));
      InsAtomicU64EvalAssign(&indexer->frames, InsAtomicU64Eval(&indexer->frames) + count);
   }
   return ok;
}

// A stretch of sound through the sound model, and its time and vector (as
// F16) onto the lists. One too quiet to be anything is left out.
internal B32
_IDX_EncodeSound(IDX_Indexer* indexer, Arena* arena, F32* samples, U64 count, F32 time_s, F32* features, String8List* times_out,
                 String8List* vectors_out) {
   B32 ok = 1;
   F32 peak = 0;
   for (U64 i = 0; i < count; i += 1) {
      peak = Max(peak, fabsf(samples[i]));
   }
   if (peak >= IDX_SOUND_SILENCE) {
      F32 vector[EMB_DIM];
      EMB_SoundFeatures(samples, count, features);
      ok = EMB_SoundVector(indexer->model, features, vector);
      if (ok) {
         U16 v[EMB_DIM];
         for (U64 i = 0; i < EMB_DIM; i += 1) {
            v[i] = INX_F16FromF32(vector[i]);
         }
         Str8ListPush(arena, times_out, PushStr8Copy(arena, Str8((U8*)&time_s, sizeof(time_s))));
         Str8ListPush(arena, vectors_out, PushStr8Copy(arena, Str8((U8*)v, sizeof(v))));
      }
   }
   return ok;
}

internal void
_IDX_ThreadEntry(void* p) {
   IDX_Indexer* indexer = (IDX_Indexer*)p;
   SetThreadName(Str8Lit("Indexer"));

   Arena* arena = ArenaAlloc();
   LogSelect(indexer->thread_log);

   if (indexer->model != 0 && !indexer->model_tried) {
      indexer->model_tried = 1;
      LogScopeBegin();
      U64 begin_us = NowTimeUS();
      if (EMB_Load(indexer->model)) {
         LogInfoF("Loaded the model in %.1f s\n", (F64)(NowTimeUS() - begin_us) / 1000000.0);
         begin_us = NowTimeUS();
         if (EMB_LoadSound(indexer->model)) {
            LogInfoF("Loaded the sound model in %.1f s\n", (F64)(NowTimeUS() - begin_us) / 1000000.0);
         }
      }
      _IDX_PassOnLogs(indexer, LogScopeEnd(arena));
      ArenaClear(arena);
   }
   B32 loaded = EMB_IsLoaded(indexer->model);
   if (!loaded) {
      InsAtomicU64EvalAssign(&indexer->no_model, 1);
   }
   B32 sound_on = EMB_SoundIsLoaded(indexer->model);

   for (U64 idx = 0; loaded && idx < indexer->count && !InsAtomicU64Eval(&indexer->cancel); idx += 1) {
      InsAtomicU64EvalAssign(&indexer->current, idx + 1);
      Temp temp = TempBegin(arena);
      LogScopeBegin();

      String8        path = indexer->paths[idx];
      FileProperties props = PropertiesFromFilePath(path);
      INX_Header     header = INX_HeaderFor(path, props, indexer->model_name, IDX_SAMPLE_SECONDS);
      INX_Header     sound_header = INX_HeaderFor(path, props, indexer->sound_model_name, IDX_SOUND_SECONDS);
      String8        file_path = INX_FilePath(arena, indexer->index_folder, path, INX_PICTURES);
      String8        sound_file_path = INX_FilePath(arena, indexer->index_folder, path, INX_SOUND);
      U64            have = 0;
      U64            sound_have = 0;
      B32            pictures_done = INX_IsUpToDate(file_path, &header, &have);
      B32            sound_done = !sound_on || INX_IsUpToDate(sound_file_path, &sound_header, &sound_have);
      if (pictures_done) {
         InsAtomicU64EvalAssign(&indexer->frames, InsAtomicU64Eval(&indexer->frames) + have);
         InsAtomicU64EvalAssign(&indexer->indexed[idx], 1);
      }
      if (sound_on && sound_done) {
         InsAtomicU64EvalAssign(&indexer->sound_indexed[idx], 1);
      }

      if (pictures_done && sound_done) {
         InsAtomicU64IncEval(&indexer->up_to_date);
      } else {
         U64 begin_us = NowTimeUS();

         // The sound is only asked for if it is to be indexed.
         VID_DecoderInfo info = {0};
         S32             sound_rate = sound_done ? 0 : EMB_SOUND_RATE;
         VID_Decoder*    decoder = indexer->use_gpu ? VID_DecoderOpenOnCard(arena, path, IDX_FRAME_DIM, sound_rate, &info)
                                                    : VID_DecoderOpen(arena, path, IDX_FRAME_DIM, sound_rate, &info);
         B32             ok = (decoder != 0);
         if (ok) {
            U8*         pixels = PushArrayNoZero(arena, U8, (U64)info.size.x * (U64)info.size.y * 4);
            F32*        images = PushArrayNoZero(arena, F32, IDX_BATCH * EMB_IMAGE_FLOATS);
            F32*        vectors = PushArrayNoZero(arena, F32, IDX_BATCH * EMB_DIM);
            F32         batch_times[IDX_BATCH];
            U64         batch_count = 0;
            String8List times = {0};
            String8List vector_list = {0};

            // The sound, gathered into one channel a stretch at a time.
            F32*        stretch = PushArrayNoZero(arena, F32, IDX_SOUND_SAMPLES);
            F32*        features = PushArrayNoZero(arena, F32, EMB_SOUND_FLOATS);
            F32         chunk[2 * 4096];
            U64         stretch_count = 0;
            F32         stretch_s = 0;
            F64         sound_s = 0; // how far the sound has been read
            B32         sound_more = (!sound_done && info.has_sound);
            String8List sound_times = {0};
            String8List sound_vectors = {0};

            // Every frame is decoded - a codec can't skip to the next second
            // without decoding what is between - but only the first at or
            // after each whole step is made into pixels and taken
            // (VID_DecoderNextFrameFrom). That goes for a video whose
            // frames are indexed already too, when it is here for its sound:
            // the decoder keeps what it reads of the one while asked for the
            // other, so the two are read side by side.
            F64 next_s = 0;
            F64 time_s = 0;
            B32 frames_more = 1;

            // Where the time goes, for the log: in the two models. The rest
            // is the decoder's.
            U64 pictures_us = 0;
            U64 sound_us = 0;
            U64 t = 0;
            while (ok && !InsAtomicU64Eval(&indexer->cancel) && (frames_more || sound_more)) {
               if (frames_more) {
                  frames_more = VID_DecoderNextFrameFrom(decoder, next_s, pixels, &time_s);
                  next_s = time_s + IDX_SAMPLE_SECONDS;
               }
               if (frames_more && !pictures_done) {
                  t = NowTimeUS();
                  EMB_ImageFromFrame(pixels, info.size, images + batch_count * EMB_IMAGE_FLOATS);
                  batch_times[batch_count] = (F32)time_s;
                  batch_count += 1;
                  if (batch_count == IDX_BATCH) {
                     ok = _IDX_Encode(indexer, arena, images, batch_times, batch_count, vectors, &times, &vector_list);
                     batch_count = 0;
                  }
                  pictures_us += NowTimeUS() - t;
               }

               // The sound up to this frame, or all the rest of it once the
               // frames have ended.
               while (ok && sound_more && (!frames_more || sound_s < time_s) && !InsAtomicU64Eval(&indexer->cancel)) {
                  F64 chunk_s = 0;
                  U64 got = VID_DecoderNextSound(decoder, chunk, ArrayCount(chunk) / 2, &chunk_s);
                  sound_more = (got != 0);
                  sound_s = chunk_s + (F64)got / (F64)EMB_SOUND_RATE;
                  for (U64 i = 0; ok && i < got; i += 1) {
                     if (stretch_count == 0) {
                        stretch_s = (F32)(chunk_s + (F64)i / (F64)EMB_SOUND_RATE);
                     }
                     stretch[stretch_count++] = 0.5f * (chunk[2 * i] + chunk[2 * i + 1]);
                     if (stretch_count == IDX_SOUND_SAMPLES) {
                        t = NowTimeUS();
                        ok = _IDX_EncodeSound(indexer, arena, stretch, stretch_count, stretch_s, features, &sound_times,
                                              &sound_vectors);
                        sound_us += NowTimeUS() - t;
                        stretch_count = 0;
                     }
                  }
               }
            }
            if (ok && batch_count != 0) {
               ok = _IDX_Encode(indexer, arena, images, batch_times, batch_count, vectors, &times, &vector_list);
            }
            // What is left at the end, if it is a second or more.
            if (ok && stretch_count >= EMB_SOUND_RATE) {
               ok = _IDX_EncodeSound(indexer, arena, stretch, stretch_count, stretch_s, features, &sound_times, &sound_vectors);
            }
            VID_DecoderClose(decoder);

            // A video stopped part way is not written: it is read again next
            // time. One with no sound gets a sound file with nothing in it,
            // so it is not read again for that.
            if (ok && !InsAtomicU64Eval(&indexer->cancel)) {
               if (!pictures_done) {
                  if (INX_Write(file_path, header, path, times, vector_list)) {
                     InsAtomicU64EvalAssign(&indexer->indexed[idx], 1);
                  } else {
                     LogUserErrorF("Could not write %S, the index of %S\n", file_path, path);
                  }
               }
               if (!sound_done) {
                  if (INX_Write(sound_file_path, sound_header, path, sound_times, sound_vectors)) {
                     InsAtomicU64EvalAssign(&indexer->sound_indexed[idx], 1);
                  } else {
                     LogUserErrorF("Could not write %S, the sound index of %S\n", sound_file_path, path);
                  }
               }
               LogInfoF("Indexed %S: %llu frames, %llu stretches of sound in %.1f s (the picture model %.1f s, the sound "
                        "model %.1f s)\n",
                        indexer->names[idx], (unsigned long long)(times.total_size / sizeof(F32)),
                        (unsigned long long)(sound_times.total_size / sizeof(F32)),
                        (F64)(NowTimeUS() - begin_us) / 1000000.0, (F64)pictures_us / 1000000.0, (F64)sound_us / 1000000.0);
            }
         }
         if (!ok) {
            InsAtomicU64IncEval(&indexer->failed);
         }
      }

      _IDX_PassOnLogs(indexer, LogScopeEnd(arena));
      TempEnd(temp);
      InsAtomicU64IncEval(&indexer->done);
   }
   InsAtomicU64EvalAssign(&indexer->current, 0);
   ArenaRelease(arena);
}

internal IDX_Indexer*
IDX_Alloc(String8 index_folder, String8 model_name, String8 sound_model_name, EMB_Model* model, B32 use_gpu) {
   Arena*       arena = ArenaAlloc();
   IDX_Indexer* indexer = PushArray(arena, IDX_Indexer, 1);
   indexer->arena = ArenaAlloc();
   indexer->model = model;
   indexer->use_gpu = use_gpu;
   indexer->log_arena = ArenaAlloc();
   indexer->log_mutex = MutexAlloc();
   indexer->thread_log = LogAlloc();
   indexer->index_folder = PushStr8Copy(arena, index_folder);
   indexer->model_name = PushStr8Copy(arena, model_name);
   indexer->sound_model_name = PushStr8Copy(arena, sound_model_name);
   if (!FilePathExists(index_folder)) {
      MakeDirectory(index_folder);
   }
   return indexer;
}

internal void
_IDX_StopThread(IDX_Indexer* indexer) {
   if (indexer->running) {
      InsAtomicU64EvalAssign(&indexer->cancel, 1);
      ThreadJoin(indexer->thread, max_U64);
      indexer->running = 0;
   }
}

internal void
IDX_Stop(IDX_Indexer* indexer) {
   _IDX_StopThread(indexer);
}

internal void
IDX_Update(IDX_Indexer* indexer, LIB_Library* library) {
   // Read before the logs are: the thread hands on a video's logs before it
   // counts the video done, so all of the run's are logged ahead of its end.
   U64 done = InsAtomicU64Eval(&indexer->done);

   MutexTake(indexer->log_mutex);
   for (U64 kind = 0; kind < LogMsgKind_COUNT; kind += 1) {
      for (String8Node* n = indexer->logs[kind].first; n != 0; n = n->next) {
         LogMsg((LogMsgKind)kind, n->string);
      }
      MemoryZeroStruct(&indexer->logs[kind]);
   }
   ArenaClear(indexer->log_arena);
   MutexDrop(indexer->log_mutex);

   // The end of a run, logged once.
   if (indexer->running && !indexer->reported && done == indexer->count) {
      indexer->reported = 1;
      indexer->ended_us = NowTimeUS();
      F64 seconds = (F64)(indexer->ended_us - indexer->started_us) / 1000000.0;
      LogInfoF("%llu frames in the index of %llu videos, %.1f s (%llu were up to date, %llu could not be read)\n",
               (unsigned long long)InsAtomicU64Eval(&indexer->frames), (unsigned long long)indexer->count, seconds,
               (unsigned long long)InsAtomicU64Eval(&indexer->up_to_date),
               (unsigned long long)InsAtomicU64Eval(&indexer->failed));
   }

   if (indexer->library_gen == library->generation) {
      return;
   }

   _IDX_StopThread(indexer);

   ArenaClear(indexer->arena);
   indexer->library_gen = library->generation;
   indexer->count = library->video_count;
   indexer->paths = PushArray(indexer->arena, String8, indexer->count);
   indexer->names = PushArray(indexer->arena, String8, indexer->count);
   indexer->indexed = PushArray(indexer->arena, U64, indexer->count);
   indexer->sound_indexed = PushArray(indexer->arena, U64, indexer->count);
   for (U64 i = 0; i < indexer->count; i += 1) {
      indexer->paths[i] = PushStr8Copy(indexer->arena, library->videos[i].path);
      indexer->names[i] = Str8SkipLastSlash(indexer->paths[i]);
   }
   indexer->cancel = 0;
   indexer->done = 0;
   indexer->failed = 0;
   indexer->up_to_date = 0;
   indexer->frames = 0;
   indexer->current = 0;
   indexer->started_us = NowTimeUS();
   indexer->ended_us = 0;
   indexer->reported = 0;

   if (indexer->count != 0) {
      indexer->thread = ThreadLaunch(_IDX_ThreadEntry, indexer);
      indexer->running = 1;
   }
}

internal IDX_Status
IDX_GetStatus(IDX_Indexer* indexer) {
   IDX_Status result = {0};
   result.video_count = indexer->count;
   result.videos_done = InsAtomicU64Eval(&indexer->done);
   result.videos_failed = InsAtomicU64Eval(&indexer->failed);
   result.frames = InsAtomicU64Eval(&indexer->frames);
   result.no_model = (B32)InsAtomicU64Eval(&indexer->no_model);
   result.seconds = (F64)((indexer->ended_us != 0 ? indexer->ended_us : NowTimeUS()) - indexer->started_us) / 1000000.0;

   U64 current = InsAtomicU64Eval(&indexer->current);
   if (current != 0 && current <= indexer->count) {
      result.current = indexer->names[current - 1];
   }
   return result;
}

internal B32
IDX_IsIndexed(IDX_Indexer* indexer, U64 library_gen, U64 video_idx) {
   return (indexer->library_gen == library_gen && video_idx < indexer->count &&
           InsAtomicU64Eval(&indexer->indexed[video_idx]) != 0);
}

internal B32
IDX_SoundIsIndexed(IDX_Indexer* indexer, U64 library_gen, U64 video_idx) {
   return (indexer->library_gen == library_gen && video_idx < indexer->count &&
           InsAtomicU64Eval(&indexer->sound_indexed[video_idx]) != 0);
}

internal void
IDX_Reindex(IDX_Indexer* indexer, LIB_Library* library) {
   // Stopped first: it may be part way through writing one.
   _IDX_StopThread(indexer);
   Temp scratch = ScratchBegin(0, 0);
   for (U64 i = 0; i < library->video_count; i += 1) {
      DeleteFileAtPath(INX_FilePath(scratch.arena, indexer->index_folder, library->videos[i].path, INX_PICTURES));
      DeleteFileAtPath(INX_FilePath(scratch.arena, indexer->index_folder, library->videos[i].path, INX_SOUND));
   }
   ScratchEnd(scratch);

   // No library's generation, so IDX_Update starts a run, and until it has
   // nothing counts as indexed.
   indexer->library_gen = max_U64;
}
