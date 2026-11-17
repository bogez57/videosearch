////////////////////////////////
//~ bgz: indexer
//
// See indexer.h.
//
// A run is one pass over the library's videos as they were when it started:
// their paths are copied onto the run's arena, so the library can change
// while the thread reads them. A change stops the run (cancel, then join)
// and starts a new one. What the thread writes and the main thread reads are
// the U64s below, through atomics; the paths do not change while it runs.

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

   // Written by the thread, read by the main thread.
   U64 cancel;  // set by the main thread: the thread stops at the next frame
   U64 done;
   U64 failed;
   U64 frames;
   U64 current; // the video under way, plus one. 0: none
};

internal void
_IDX_ThreadEntry(void* p) {
   IDX_Indexer* indexer = (IDX_Indexer*)p;
   SetThreadName(Str8Lit("Indexer"));

   Arena* arena = ArenaAlloc();
   for (U64 idx = 0; idx < indexer->count && !InsAtomicU64Eval(&indexer->cancel); idx += 1) {
      InsAtomicU64EvalAssign(&indexer->current, idx + 1);
      Temp temp = TempBegin(arena);

      // No sound: only the picture is looked at.
      VID_DecoderInfo info = {0};
      VID_Decoder*    decoder = VID_DecoderOpen(arena, indexer->paths[idx], IDX_FRAME_DIM, 0, &info);
      if (decoder == 0) {
         InsAtomicU64IncEval(&indexer->failed);
      } else {
         U8* pixels = PushArrayNoZero(arena, U8, (U64)info.size.x * (U64)info.size.y * 4);

         // Every frame is decoded - a codec can't skip to the next second
         // without decoding what is between - and the first at or after each
         // whole step is the one taken.
         F64 next_s = 0;
         F64 time_s = 0;
         while (!InsAtomicU64Eval(&indexer->cancel) && VID_DecoderNextFrame(decoder, pixels, &time_s)) {
            if (time_s >= next_s) {
               InsAtomicU64IncEval(&indexer->frames);
               next_s = time_s + IDX_SAMPLE_SECONDS;
            }
         }
         VID_DecoderClose(decoder);
      }

      TempEnd(temp);
      InsAtomicU64IncEval(&indexer->done);
   }
   InsAtomicU64EvalAssign(&indexer->current, 0);
   ArenaRelease(arena);
}

internal IDX_Indexer*
IDX_Alloc(void) {
   Arena*       arena = ArenaAlloc();
   IDX_Indexer* indexer = PushArray(arena, IDX_Indexer, 1);
   indexer->arena = ArenaAlloc();
   return indexer;
}

internal void
IDX_Stop(IDX_Indexer* indexer) {
   if (indexer->running) {
      InsAtomicU64EvalAssign(&indexer->cancel, 1);
      ThreadJoin(indexer->thread, max_U64);
      indexer->running = 0;
   }
}

internal void
IDX_Update(IDX_Indexer* indexer, LIB_Library* library) {
   // The end of a run, logged once from here: the thread has no log.
   if (indexer->running && !indexer->reported && InsAtomicU64Eval(&indexer->done) == indexer->count) {
      indexer->reported = 1;
      F64 seconds = (F64)(NowTimeUS() - indexer->started_us) / 1000000.0;
      LogInfoF("Read %llu frames from %llu videos in %.1f s (%llu could not be decoded)\n",
               (unsigned long long)InsAtomicU64Eval(&indexer->frames), (unsigned long long)indexer->count, seconds,
               (unsigned long long)InsAtomicU64Eval(&indexer->failed));
   }

   if (indexer->library_gen == library->generation) {
      return;
   }

   IDX_Stop(indexer);

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

   U64 current = InsAtomicU64Eval(&indexer->current);
   if (current != 0 && current <= indexer->count) {
      result.current = indexer->names[current - 1];
   }
   return result;
}
