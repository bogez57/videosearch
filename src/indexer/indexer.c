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
// or two, and kept for the runs after it.

struct IDX_Indexer {
   Arena*   arena; // the run's: emptied for each
   Thread   thread;
   B32      running; // a thread was started and not joined yet
   U64      library_gen;
   U64      started_us;
   B32      reported; // the run's end has been logged

   String8* paths;
   String8* names; // pieces of paths
   U64      count;

   String8    index_folder;
   String8    model_folder;
   Arena*     model_arena;
   EMB_Model* model;
   B32        model_tried; // only the thread reads or writes these two, and one runs at a time

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
   U64 current; // the video under way, plus one. 0: none
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

internal void
_IDX_ThreadEntry(void* p) {
   IDX_Indexer* indexer = (IDX_Indexer*)p;
   SetThreadName(Str8Lit("Indexer"));

   Arena* arena = ArenaAlloc();
   LogSelect(indexer->thread_log);

   if (!indexer->model_tried) {
      indexer->model_tried = 1;
      LogScopeBegin();
      U64 begin_us = NowTimeUS();
      indexer->model = EMB_Open(indexer->model_arena, indexer->model_folder);
      if (indexer->model != 0) {
         LogInfoF("Loaded the image model in %.1f s\n", (F64)(NowTimeUS() - begin_us) / 1000000.0);
      }
      _IDX_PassOnLogs(indexer, LogScopeEnd(arena));
      ArenaClear(arena);
   }
   if (indexer->model == 0) {
      InsAtomicU64EvalAssign(&indexer->no_model, 1);
   }

   String8 model_name = Str8SkipLastSlash(indexer->model_folder);
   for (U64 idx = 0; indexer->model != 0 && idx < indexer->count && !InsAtomicU64Eval(&indexer->cancel); idx += 1) {
      InsAtomicU64EvalAssign(&indexer->current, idx + 1);
      Temp temp = TempBegin(arena);
      LogScopeBegin();

      String8    path = indexer->paths[idx];
      INX_Header header = INX_HeaderFor(path, PropertiesFromFilePath(path), model_name, IDX_SAMPLE_SECONDS);
      String8    file_path = INX_FilePath(arena, indexer->index_folder, path);
      U64        have = 0;
      if (INX_IsUpToDate(file_path, &header, &have)) {
         InsAtomicU64EvalAssign(&indexer->frames, InsAtomicU64Eval(&indexer->frames) + have);
         InsAtomicU64IncEval(&indexer->up_to_date);
      } else {
         U64 begin_us = NowTimeUS();

         // No sound: only the picture is looked at.
         VID_DecoderInfo info = {0};
         VID_Decoder*    decoder = VID_DecoderOpen(arena, path, IDX_FRAME_DIM, 0, &info);
         B32             ok = (decoder != 0);
         if (ok) {
            U8*         pixels = PushArrayNoZero(arena, U8, (U64)info.size.x * (U64)info.size.y * 4);
            F32*        images = PushArrayNoZero(arena, F32, IDX_BATCH * EMB_IMAGE_FLOATS);
            F32*        vectors = PushArrayNoZero(arena, F32, IDX_BATCH * EMB_DIM);
            F32         batch_times[IDX_BATCH];
            U64         batch_count = 0;
            String8List times = {0};
            String8List vector_list = {0};

            // Every frame is decoded - a codec can't skip to the next second
            // without decoding what is between - and the first at or after
            // each whole step is the one taken.
            F64 next_s = 0;
            F64 time_s = 0;
            while (ok && !InsAtomicU64Eval(&indexer->cancel) && VID_DecoderNextFrame(decoder, pixels, &time_s)) {
               if (time_s >= next_s) {
                  next_s = time_s + IDX_SAMPLE_SECONDS;
                  EMB_ImageFromFrame(pixels, info.size, images + batch_count * EMB_IMAGE_FLOATS);
                  batch_times[batch_count] = (F32)time_s;
                  batch_count += 1;
                  if (batch_count == IDX_BATCH) {
                     ok = _IDX_Encode(indexer, arena, images, batch_times, batch_count, vectors, &times, &vector_list);
                     batch_count = 0;
                  }
               }
            }
            if (ok && batch_count != 0) {
               ok = _IDX_Encode(indexer, arena, images, batch_times, batch_count, vectors, &times, &vector_list);
            }
            VID_DecoderClose(decoder);

            // A video stopped part way is not written: it is read again next time.
            if (ok && !InsAtomicU64Eval(&indexer->cancel)) {
               U64 frames = times.total_size / sizeof(F32);
               if (INX_Write(file_path, header, path, times, vector_list)) {
                  LogInfoF("Indexed %S: %llu frames in %.1f s\n", indexer->names[idx], (unsigned long long)frames,
                           (F64)(NowTimeUS() - begin_us) / 1000000.0);
               } else {
                  LogUserErrorF("Could not write %S, the index of %S\n", file_path, path);
               }
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
IDX_Alloc(String8 index_folder, String8 model_folder) {
   Arena*       arena = ArenaAlloc();
   IDX_Indexer* indexer = PushArray(arena, IDX_Indexer, 1);
   indexer->arena = ArenaAlloc();
   indexer->model_arena = ArenaAlloc();
   indexer->log_arena = ArenaAlloc();
   indexer->log_mutex = MutexAlloc();
   indexer->thread_log = LogAlloc();
   indexer->index_folder = PushStr8Copy(arena, index_folder);
   indexer->model_folder = PushStr8Copy(arena, model_folder);
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
   EMB_Close(indexer->model);
   indexer->model = 0;
}

internal void
IDX_Update(IDX_Indexer* indexer, LIB_Library* library) {
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
   if (indexer->running && !indexer->reported && InsAtomicU64Eval(&indexer->done) == indexer->count) {
      indexer->reported = 1;
      F64 seconds = (F64)(NowTimeUS() - indexer->started_us) / 1000000.0;
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

   U64 current = InsAtomicU64Eval(&indexer->current);
   if (current != 0 && current <= indexer->count) {
      result.current = indexer->names[current - 1];
   }
   return result;
}
