////////////////////////////////
//~ bgz: indexer
//
// See indexer.h.
//
// A run is one pass over the library's videos as they were when it started:
// their paths are copied onto the run's arena, so the library can change
// while the threads read them. A change stops the run (cancel, then join)
// and starts a new one. What the threads write and the main thread reads are
// the U64s below, through atomics, and the logs, under log_mutex; the paths
// do not change while it runs.
//
// A run has two threads, so that reading a video and running the models go
// on at the same time. The reader decodes the videos and makes their frames
// and sound into what the models read, a piece at a time. The run's own
// thread, which starts the reader, runs each piece through its model and
// writes the index files. The pieces wait in a short ring between the two.
//
// The model is loaded by the first run's thread, since that takes a second
// or so, and kept for the runs after it.

// Pieces that can wait between the reader and the models.
#define _IDX_PIECES 4

// What the reader hands the run's thread, in the order it is to be taken.
typedef enum _IDX_PieceKind {
   _IDX_Piece_Begin,  // a video that is to be indexed, before anything of it
   _IDX_Piece_Frames, // some of its frames, as the picture model reads them
   _IDX_Piece_Sound,  // a stretch of its sound, as the sound model reads it
   _IDX_Piece_End,    // nothing more of it: it was read to its end, or could not be
} _IDX_PieceKind;

typedef struct _IDX_Piece _IDX_Piece;
struct _IDX_Piece {
   _IDX_PieceKind kind;
   U64            video_idx;

   // Begin: the video's file as the reader found it, and which of its two
   // index files are up to date already.
   FileProperties props;
   B32            pictures_done;
   B32            sound_done;

   // Frames: EMB_View_COUNT pictures of each, one frame's after another's.
   F32* images;
   F32  times[IDX_BATCH];
   U64  count;

   // Sound
   F32* features;
   F32  time_s;

   // End: whether it could be read, and where the reader's time went.
   B32 ok;
   U64 decoder_us;
   U64 ready_us;       // making the frames into what the model reads
   U64 sound_ready_us; // and the sound
};

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

   // The reader and its pieces, a ring under pieces_mutex: the reader fills
   // pieces[written % _IDX_PIECES] and then counts it written, the run's
   // thread works on pieces[taken % _IDX_PIECES] and then counts it taken.
   // pieces_changed is signalled at each count, when the reader ends and at
   // a cancel. spare_images and spare_features are what the reader fills
   // next: it hands them on by exchanging them for the piece's own.
   Thread      reader;
   Mutex       pieces_mutex;
   CondVar     pieces_changed;
   _IDX_Piece* pieces;
   U64         written;
   U64         taken;
   B32         reader_ended;
   F32*        spare_images;
   F32*        spare_features;
   U64         reader_waited_us; // the reader's own: how long it has waited for room, for the video under way

   // What the threads have logged and the main thread has not yet.
   // thread_log and reader_log are the two threads' logs, kept for every
   // run's: there is no letting go of one.
   Log*        thread_log;
   Log*        reader_log;
   Mutex       log_mutex;
   Arena*      log_arena;
   String8List logs[LogMsgKind_COUNT];

   // Written by the threads, read by the main thread. Each has one thread
   // that writes it, so an add is a read and a write, but cancel (the main
   // thread's), done (both count it up) and give_up.
   U64 cancel;  // set by the main thread: the threads stop at the next frame or piece
   U64 give_up; // set by the run's thread: the video, plus one, that the reader is to stop reading
   U64 done;
   U64 failed;
   U64 up_to_date; // the reader's
   U64 frames;
   U64 frames_had; // the reader's: in the files that were up to date
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

// The piece the reader fills next, once there is room for it. 0 if the run
// was cancelled first.
internal _IDX_Piece*
_IDX_PieceToFill(IDX_Indexer* indexer, _IDX_PieceKind kind, U64 video_idx) {
   _IDX_Piece* result = 0;
   U64         begin_us = NowTimeUS();
   MutexTake(indexer->pieces_mutex);
   while (indexer->written - indexer->taken == _IDX_PIECES && !InsAtomicU64Eval(&indexer->cancel)) {
      CondVarWait(indexer->pieces_changed, indexer->pieces_mutex, max_U64);
   }
   if (!InsAtomicU64Eval(&indexer->cancel)) {
      result = &indexer->pieces[indexer->written % _IDX_PIECES];
   }
   MutexDrop(indexer->pieces_mutex);
   indexer->reader_waited_us += NowTimeUS() - begin_us;
   if (result != 0) {
      result->kind = kind;
      result->video_idx = video_idx;
   }
   return result;
}

internal void
_IDX_PieceFilled(IDX_Indexer* indexer) {
   MutexTake(indexer->pieces_mutex);
   indexer->written += 1;
   MutexDrop(indexer->pieces_mutex);
   CondVarBroadcast(indexer->pieces_changed);
}

// The piece the run's thread takes next, once there is one. 0 when the
// reader has ended and nothing is left, or the run was cancelled.
internal _IDX_Piece*
_IDX_PieceToTake(IDX_Indexer* indexer) {
   _IDX_Piece* result = 0;
   MutexTake(indexer->pieces_mutex);
   while (indexer->written == indexer->taken && !indexer->reader_ended && !InsAtomicU64Eval(&indexer->cancel)) {
      CondVarWait(indexer->pieces_changed, indexer->pieces_mutex, max_U64);
   }
   if (indexer->written != indexer->taken && !InsAtomicU64Eval(&indexer->cancel)) {
      result = &indexer->pieces[indexer->taken % _IDX_PIECES];
   }
   MutexDrop(indexer->pieces_mutex);
   return result;
}

internal void
_IDX_PieceTaken(IDX_Indexer* indexer) {
   MutexTake(indexer->pieces_mutex);
   indexer->taken += 1;
   MutexDrop(indexer->pieces_mutex);
   CondVarBroadcast(indexer->pieces_changed);
}

// The frames the reader has made ready, handed on: the piece gets the
// pictures filled, and the reader the piece's own to fill next. Fewer than
// a batch (a video's last) are handed on with zeros for the rest.
internal void
_IDX_HandOnFrames(IDX_Indexer* indexer, U64 video_idx, F32* times, U64 count) {
   MemoryZero(indexer->spare_images + count * EMB_View_COUNT * EMB_IMAGE_FLOATS,
              (IDX_BATCH - count) * EMB_View_COUNT * EMB_IMAGE_FLOATS * sizeof(F32));
   _IDX_Piece* piece = _IDX_PieceToFill(indexer, _IDX_Piece_Frames, video_idx);
   if (piece != 0) {
      F32* images = piece->images;
      piece->images = indexer->spare_images;
      indexer->spare_images = images;
      MemoryCopy(piece->times, times, count * sizeof(F32));
      piece->count = count;
      _IDX_PieceFilled(indexer);
   }
}

// A stretch of sound made into what the sound model reads, and handed on as
// the frames are. One too quiet to be anything is left out. ready_us has how
// long the making took added to it.
internal void
_IDX_HandOnSound(IDX_Indexer* indexer, U64 video_idx, F32* samples, U64 count, F32 time_s, U64* ready_us) {
   F32 peak = 0;
   for (U64 i = 0; i < count; i += 1) {
      peak = Max(peak, fabsf(samples[i]));
   }
   if (peak >= IDX_SOUND_SILENCE) {
      U64 t = NowTimeUS();
      EMB_SoundFeatures(samples, count, indexer->spare_features);
      *ready_us += NowTimeUS() - t;

      _IDX_Piece* piece = _IDX_PieceToFill(indexer, _IDX_Piece_Sound, video_idx);
      if (piece != 0) {
         F32* features = piece->features;
         piece->features = indexer->spare_features;
         indexer->spare_features = features;
         piece->time_s = time_s;
         _IDX_PieceFilled(indexer);
      }
   }
}

// The reader's thread: every video whose index files are not up to date,
// decoded and handed to the run's thread a piece at a time.
internal void
_IDX_ReaderEntry(void* p) {
   IDX_Indexer* indexer = (IDX_Indexer*)p;
   SetThreadName(Str8Lit("Indexer's reader"));

   Arena* arena = ArenaAlloc();
   LogSelect(indexer->reader_log);
   B32 sound_on = EMB_SoundIsLoaded(indexer->model);

   for (U64 idx = 0; idx < indexer->count && !InsAtomicU64Eval(&indexer->cancel); idx += 1) {
      Temp temp = TempBegin(arena);
      LogScopeBegin();

      String8        path = indexer->paths[idx];
      FileProperties props = PropertiesFromFilePath(path);
      INX_Header     header = INX_HeaderFor(path, props, indexer->model_name, IDX_SAMPLE_SECONDS);
      INX_Header     sound_header = INX_HeaderFor(path, props, indexer->sound_model_name, IDX_SOUND_SECONDS);
      U64            have = 0;
      U64            sound_have = 0;
      B32            pictures_done = INX_IsUpToDate(INX_FilePath(arena, indexer->index_folder, path, INX_PICTURES), &header, &have);
      B32            sound_done = !sound_on || INX_IsUpToDate(INX_FilePath(arena, indexer->index_folder, path, INX_SOUND),
                                                              &sound_header, &sound_have);
      if (pictures_done) {
         InsAtomicU64EvalAssign(&indexer->frames_had, InsAtomicU64Eval(&indexer->frames_had) + have);
         InsAtomicU64EvalAssign(&indexer->indexed[idx], 1);
      }
      if (sound_on && sound_done) {
         InsAtomicU64EvalAssign(&indexer->sound_indexed[idx], 1);
      }

      if (pictures_done && sound_done) {
         InsAtomicU64IncEval(&indexer->up_to_date);
         _IDX_PassOnLogs(indexer, LogScopeEnd(arena));
         InsAtomicU64IncEval(&indexer->done);
      } else {
         _IDX_Piece* piece = _IDX_PieceToFill(indexer, _IDX_Piece_Begin, idx);
         if (piece != 0) {
            piece->props = props;
            piece->pictures_done = pictures_done;
            piece->sound_done = sound_done;
            _IDX_PieceFilled(indexer);
         }

         // Where the time goes, for the log: making the frames and the
         // sound into what the models read, and waiting for room to hand
         // them on. The rest is the decoder's.
         U64 begin_us = NowTimeUS();
         U64 ready_us = 0;
         U64 sound_ready_us = 0;
         indexer->reader_waited_us = 0;

         // The sound is only asked for if it is to be indexed.
         VID_DecoderInfo info = {0};
         S32             sound_rate = sound_done ? 0 : EMB_SOUND_RATE;
         VID_Decoder*    decoder = indexer->use_gpu ? VID_DecoderOpenOnCard(arena, path, IDX_FRAME_DIM, sound_rate, &info)
                                                    : VID_DecoderOpen(arena, path, IDX_FRAME_DIM, sound_rate, &info);
         if (decoder != 0) {
            U8* pixels = PushArrayNoZero(arena, U8, (U64)info.size.x * (U64)info.size.y * 4);
            F32 batch_times[IDX_BATCH];
            U64 batch_count = 0;

            // The sound, gathered into one channel a stretch at a time.
            F32* stretch = PushArrayNoZero(arena, F32, IDX_SOUND_SAMPLES);
            F32  chunk[2 * 4096];
            U64  stretch_count = 0;
            F32  stretch_s = 0;
            F64  sound_s = 0; // how far the sound has been read
            B32  sound_more = (!sound_done && info.has_sound);

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
            while (!InsAtomicU64Eval(&indexer->cancel) && InsAtomicU64Eval(&indexer->give_up) != idx + 1 &&
                   (frames_more || sound_more)) {
               if (frames_more) {
                  frames_more = VID_DecoderNextFrameFrom(decoder, next_s, pixels, &time_s);
                  next_s = time_s + IDX_SAMPLE_SECONDS;
               }
               if (frames_more && !pictures_done) {
                  U64 t = NowTimeUS();
                  for (U64 view = 0; view < EMB_View_COUNT; view += 1) {
                     EMB_ImageFromFrame(pixels, info.size, (EMB_View)view,
                                        indexer->spare_images + (batch_count * EMB_View_COUNT + view) * EMB_IMAGE_FLOATS);
                  }
                  ready_us += NowTimeUS() - t;
                  batch_times[batch_count] = (F32)time_s;
                  batch_count += 1;
                  if (batch_count == IDX_BATCH) {
                     _IDX_HandOnFrames(indexer, idx, batch_times, batch_count);
                     batch_count = 0;
                  }
               }

               // The sound up to this frame, or all the rest of it once the
               // frames have ended.
               while (sound_more && (!frames_more || sound_s < time_s) && !InsAtomicU64Eval(&indexer->cancel)) {
                  F64 chunk_s = 0;
                  U64 got = VID_DecoderNextSound(decoder, chunk, ArrayCount(chunk) / 2, &chunk_s);
                  sound_more = (got != 0);
                  sound_s = chunk_s + (F64)got / (F64)EMB_SOUND_RATE;
                  for (U64 i = 0; i < got; i += 1) {
                     if (stretch_count == 0) {
                        stretch_s = (F32)(chunk_s + (F64)i / (F64)EMB_SOUND_RATE);
                     }
                     stretch[stretch_count++] = 0.5f * (chunk[2 * i] + chunk[2 * i + 1]);
                     if (stretch_count == IDX_SOUND_SAMPLES) {
                        _IDX_HandOnSound(indexer, idx, stretch, stretch_count, stretch_s, &sound_ready_us);
                        stretch_count = 0;
                     }
                  }
               }
            }
            if (batch_count != 0) {
               _IDX_HandOnFrames(indexer, idx, batch_times, batch_count);
            }
            // What is left at the end, if it is a second or more.
            if (stretch_count >= EMB_SOUND_RATE) {
               _IDX_HandOnSound(indexer, idx, stretch, stretch_count, stretch_s, &sound_ready_us);
            }
            VID_DecoderClose(decoder);
         }
         U64 decoder_us = NowTimeUS() - begin_us - ready_us - sound_ready_us - indexer->reader_waited_us;

         // Its logs before its end, so they are logged ahead of the video's
         // line and of the run's.
         _IDX_PassOnLogs(indexer, LogScopeEnd(arena));
         piece = _IDX_PieceToFill(indexer, _IDX_Piece_End, idx);
         if (piece != 0) {
            piece->ok = (decoder != 0);
            piece->decoder_us = decoder_us;
            piece->ready_us = ready_us;
            piece->sound_ready_us = sound_ready_us;
            _IDX_PieceFilled(indexer);
         }
      }
      TempEnd(temp);
   }

   MutexTake(indexer->pieces_mutex);
   indexer->reader_ended = 1;
   MutexDrop(indexer->pieces_mutex);
   CondVarBroadcast(indexer->pieces_changed);
   ArenaRelease(arena);
}

// A piece's frames through the model - images holds EMB_View_COUNT pictures
// of each, one frame's after another's, a whole batch of them of which
// count frames are to be used - and their times and vectors (as F16) onto
// the lists. A frame's vector is the average of its views' vectors, made
// length one.
internal B32
_IDX_Encode(IDX_Indexer* indexer, Arena* arena, F32* images, F32* times, U64 count, F32* vectors, String8List* times_out,
            String8List* vectors_out) {
   B32 ok = EMB_ImageVectors(indexer->model, images, vectors);
   if (ok) {
      F32* t = PushArrayNoZero(arena, F32, count);
      MemoryCopy(t, times, count * sizeof(F32));
      Str8ListPush(arena, times_out, Str8((U8*)t, count * sizeof(F32)));

      U16* v = PushArrayNoZero(arena, U16, count * EMB_DIM);
      for (U64 i = 0; i < count; i += 1) {
         F32  mean[EMB_DIM] = {0};
         F32* views = vectors + i * EMB_View_COUNT * EMB_DIM;
         F32  sum = 0;
         for (U64 j = 0; j < EMB_DIM; j += 1) {
            for (U64 view = 0; view < EMB_View_COUNT; view += 1) mean[j] += views[view * EMB_DIM + j];
            sum += mean[j] * mean[j];
         }
         F32 inv = (sum > 0) ? 1.0f / sqrtf(sum) : 0;
         for (U64 j = 0; j < EMB_DIM; j += 1) {
            v[i * EMB_DIM + j] = INX_F16FromF32(mean[j] * inv);
         }
      }
      Str8ListPush(arena, vectors_out, Str8((U8*)v, count * EMB_DIM * sizeof(U16)));
      InsAtomicU64EvalAssign(&indexer->frames, InsAtomicU64Eval(&indexer->frames) + count);
   }
   return ok;
}

// The run's thread: loads the model if it is not, starts the reader, and
// runs what the reader hands it through the models and into the index files.
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

   if (loaded) {
      // The ring's pieces and the reader's spares, each with room for a
      // batch of frames and a stretch of sound. They are this thread's to
      // let go of, after the reader has ended.
      U64 images_size = (U64)IDX_BATCH * EMB_View_COUNT * EMB_IMAGE_FLOATS;
      indexer->pieces = PushArray(arena, _IDX_Piece, _IDX_PIECES);
      for (U64 i = 0; i < _IDX_PIECES; i += 1) {
         indexer->pieces[i].images = PushArrayNoZero(arena, F32, images_size);
         indexer->pieces[i].features = PushArrayNoZero(arena, F32, EMB_SOUND_FLOATS);
      }
      indexer->spare_images = PushArrayNoZero(arena, F32, images_size);
      indexer->spare_features = PushArrayNoZero(arena, F32, EMB_SOUND_FLOATS);
      indexer->written = 0;
      indexer->taken = 0;
      indexer->reader_ended = 0;
      InsAtomicU64EvalAssign(&indexer->give_up, 0);
      indexer->reader = ThreadLaunch(_IDX_ReaderEntry, indexer);

      F32* vectors = PushArrayNoZero(arena, F32, IDX_BATCH * EMB_View_COUNT * EMB_DIM);

      // The video whose pieces are being taken, from its Begin to its End.
      B32         in_video = 0;
      Temp        temp = {0};
      String8     path = {0};
      INX_Header  header = {0};
      INX_Header  sound_header = {0};
      B32         pictures_done = 0;
      B32         sound_done = 0;
      B32         ok = 0;
      String8List times = {0};
      String8List vector_list = {0};
      String8List sound_times = {0};
      String8List sound_vectors = {0};
      U64         begin_us = 0;
      U64         pictures_us = 0;
      U64         sound_us = 0;

      for (_IDX_Piece* piece = _IDX_PieceToTake(indexer); piece != 0; piece = _IDX_PieceToTake(indexer)) {
         switch (piece->kind) {
            case _IDX_Piece_Begin: {
               temp = TempBegin(arena);
               LogScopeBegin();
               in_video = 1;
               InsAtomicU64EvalAssign(&indexer->current, piece->video_idx + 1);

               path = indexer->paths[piece->video_idx];
               header = INX_HeaderFor(path, piece->props, indexer->model_name, IDX_SAMPLE_SECONDS);
               sound_header = INX_HeaderFor(path, piece->props, indexer->sound_model_name, IDX_SOUND_SECONDS);
               pictures_done = piece->pictures_done;
               sound_done = piece->sound_done;
               ok = 1;
               MemoryZeroStruct(&times);
               MemoryZeroStruct(&vector_list);
               MemoryZeroStruct(&sound_times);
               MemoryZeroStruct(&sound_vectors);
               begin_us = NowTimeUS();
               pictures_us = 0;
               sound_us = 0;
            } break;

            case _IDX_Piece_Frames: {
               if (ok) {
                  U64 t = NowTimeUS();
                  ok = _IDX_Encode(indexer, arena, piece->images, piece->times, piece->count, vectors, &times, &vector_list);
                  pictures_us += NowTimeUS() - t;
               }
            } break;

            case _IDX_Piece_Sound: {
               if (ok) {
                  U64 t = NowTimeUS();
                  F32 vector[EMB_DIM];
                  ok = EMB_SoundVector(indexer->model, piece->features, vector);
                  if (ok) {
                     U16 v[EMB_DIM];
                     for (U64 i = 0; i < EMB_DIM; i += 1) {
                        v[i] = INX_F16FromF32(vector[i]);
                     }
                     Str8ListPush(arena, &sound_times, PushStr8Copy(arena, Str8((U8*)&piece->time_s, sizeof(piece->time_s))));
                     Str8ListPush(arena, &sound_vectors, PushStr8Copy(arena, Str8((U8*)v, sizeof(v))));
                  }
                  sound_us += NowTimeUS() - t;
               }
            } break;

            case _IDX_Piece_End: {
               // A video stopped part way is not written: it is read again
               // next time. One with no sound gets a sound file with nothing
               // in it, so it is not read again for that.
               ok = ok && piece->ok;
               if (ok && !InsAtomicU64Eval(&indexer->cancel)) {
                  if (!pictures_done) {
                     String8 file_path = INX_FilePath(arena, indexer->index_folder, path, INX_PICTURES);
                     if (INX_Write(file_path, header, path, times, vector_list)) {
                        InsAtomicU64EvalAssign(&indexer->indexed[piece->video_idx], 1);
                     } else {
                        LogUserErrorF("Could not write %S, the index of %S\n", file_path, path);
                     }
                  }
                  if (!sound_done) {
                     String8 file_path = INX_FilePath(arena, indexer->index_folder, path, INX_SOUND);
                     if (INX_Write(file_path, sound_header, path, sound_times, sound_vectors)) {
                        InsAtomicU64EvalAssign(&indexer->sound_indexed[piece->video_idx], 1);
                     } else {
                        LogUserErrorF("Could not write %S, the sound index of %S\n", file_path, path);
                     }
                  }
                  LogInfoF("Indexed %S: %llu frames, %llu stretches of sound in %.1f s (the picture model %.1f s, the sound "
                           "model %.1f s; meanwhile the decoder %.1f s, the frames made ready %.1f s, the sound made ready "
                           "%.1f s)\n",
                           indexer->names[piece->video_idx], (unsigned long long)(times.total_size / sizeof(F32)),
                           (unsigned long long)(sound_times.total_size / sizeof(F32)),
                           (F64)(NowTimeUS() - begin_us) / 1000000.0, (F64)pictures_us / 1000000.0, (F64)sound_us / 1000000.0,
                           (F64)piece->decoder_us / 1000000.0, (F64)piece->ready_us / 1000000.0,
                           (F64)piece->sound_ready_us / 1000000.0);
               }
               if (!ok) {
                  InsAtomicU64IncEval(&indexer->failed);
               }
               _IDX_PassOnLogs(indexer, LogScopeEnd(arena));
               TempEnd(temp);
               in_video = 0;
               InsAtomicU64IncEval(&indexer->done);
            } break;
         }

         // A video the model failed on is not read any further.
         if (in_video && !ok) {
            InsAtomicU64EvalAssign(&indexer->give_up, piece->video_idx + 1);
         }
         _IDX_PieceTaken(indexer);
      }

      // Cancelled part way through a video.
      if (in_video) {
         _IDX_PassOnLogs(indexer, LogScopeEnd(arena));
         TempEnd(temp);
      }
      ThreadJoin(indexer->reader, max_U64);
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
   indexer->reader_log = LogAlloc();
   indexer->pieces_mutex = MutexAlloc();
   indexer->pieces_changed = CondVarAlloc();
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
      // Under the pieces' mutex, so a thread about to wait for a piece does
      // not miss it.
      MutexTake(indexer->pieces_mutex);
      InsAtomicU64EvalAssign(&indexer->cancel, 1);
      MutexDrop(indexer->pieces_mutex);
      CondVarBroadcast(indexer->pieces_changed);
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
   // Read before the logs are: the threads hand on a video's logs before it
   // is counted done, so all of the run's are logged ahead of its end.
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
               (unsigned long long)(InsAtomicU64Eval(&indexer->frames) + InsAtomicU64Eval(&indexer->frames_had)),
               (unsigned long long)indexer->count, seconds,
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
   indexer->frames_had = 0;
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
   result.frames = InsAtomicU64Eval(&indexer->frames) + InsAtomicU64Eval(&indexer->frames_had);
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
