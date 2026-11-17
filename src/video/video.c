////////////////////////////////
//~ bgz: video: the player
//
// See video.h. Nothing here knows what decodes: that is video_decoder.h.
//
// A thread decodes ahead of what is shown and heard: frames into a queue
// of a few, sound into a ring of a couple of seconds. The main thread, once
// a frame (VID_Update), moves the clock on and takes from the queue the
// frame whose time has come. The audio thread, whenever the sound card
// wants more (_VID_Voice), takes sound from the ring:
//
//    decode thread                            main thread
//    VID_DecoderNextFrame --> [frame queue] --> VID_Update --> texture
//    VID_DecoderNextSound --> [sound ring]  --> _VID_Voice --> speakers
//                                               audio thread
//
// The queue and the ring are all the threads share, and they are under the
// mutex. The decode thread writes a frame into the queue's next free place
// without the mutex - nobody else touches a free place - and takes it only
// to say the frame is there.
//
// A seek is asked for by number (seek_serial). The decode thread, when it
// sees a number that is not the one it is on, empties the queue and the
// ring, seeks the decoder and throws away what it decodes from before the
// time asked for.
//
//- bgz: the clock
//
// A video with sound keeps time by its sound: where the video is, is what
// is being heard. Each time the audio thread takes sound it notes which
// moment of the video that sound is and when it will be heard, and the
// main thread reads the clock off that note. Picture follows sound, so the
// two can't drift apart, and sound is never stretched to fit.
//
// Without sound - a file with none, or once the sound has run out before
// the picture - the clock is the computer's own.

#define _VID_QUEUE_CAP 4

typedef struct _VID_Frame _VID_Frame;
struct _VID_Frame {
   U8* pixels; // the video's size, RGBA: on the video's arena
   F64 time_s;
};

// How much sound waits, at most, and how much the decode thread keeps
// waiting before it turns to frames. In seconds.
#define _VID_RING_SECONDS       2
#define _VID_RING_AHEAD_SECONDS 0.5
// The most sound decoded in one go, in frames.
#define _VID_SOUND_CHUNK 4096

struct VID_Video {
   Arena*          arena;
   String8         path;
   VID_Decoder*    decoder;
   VID_DecoderInfo info;
   R_Handle        texture;

   Thread  thread;
   Mutex   mutex;
   CondVar wake_decode; // room in the queue, a seek, or quit

   //- bgz: under the mutex
   _VID_Frame queue[_VID_QUEUE_CAP];
   U64        queue_first;
   U64        queue_count;
   B32        quit;
   U64        seek_serial; // goes up with every seek asked for
   F64        seek_time_s;
   B32        at_end; // the decoder has no more frames, since the last seek

   // Sound, if has_sound. The ring holds ring_count frames of it (a left
   // and a right F32 each) from ring_first on, around the end and back to
   // the start. ring_time_s: when the first of them is heard.
   F32* ring;
   U64  ring_cap;
   U64  ring_first;
   U64  ring_count;
   F64  ring_time_s;
   B32  sound_at_end; // the decoder has no more sound, since the last seek
   B32  sound_on;     // the audio thread takes from the ring: playing, and the picture is there
   B32  sound_muted;  // what it takes is not heard (VID_SetMuted)
   // The note the audio thread leaves: the sound that is heard at
   // heard_us (NowTimeUS) is the video's at heard_time_s. heard_us 0: no
   // note, since the last seek or pause.
   F64 heard_time_s;
   U64 heard_us;

   //- bgz: the main thread's
   B32 playing;
   F64 position_s;
   U64 updated_us;  // when VID_Update last ran. 0: not yet
   B32 need_frame;  // show the next frame decoded, whatever its time: the first, and after a seek
   B32 ended;       // stopped at its end: playing starts it over

   B32        has_sound;
   AUD_Voice* voice;        // how the ring gets to the speakers. 0: no sound
   F32*       sound_chunk;  // the decode thread's: sound on its way into the ring
};

// The audio thread, wanting more sound: from the ring, as much as there is.
// What it does not write stays silence.
internal void
_VID_Voice(void* user, F32* samples, U64 frame_count, F64 delay_s) {
   VID_Video* video = (VID_Video*)user;

   MutexTake(video->mutex);
   U64 taken = 0;
   if (video->sound_on && video->ring_count != 0) {
      taken = Min(frame_count, video->ring_count);
      U64 until_wrap = Min(taken, video->ring_cap - video->ring_first);
      MemoryCopy(samples, video->ring + video->ring_first * AUD_CHANNELS, until_wrap * AUD_CHANNELS * sizeof(F32));
      MemoryCopy(samples + until_wrap * AUD_CHANNELS, video->ring, (taken - until_wrap) * AUD_CHANNELS * sizeof(F32));
      if (video->sound_muted) {
         // Taken all the same: the clock goes by what is taken.
         MemoryZero(samples, taken * AUD_CHANNELS * sizeof(F32));
      }

      // The first of these is heard delay_s from now.
      video->heard_time_s = video->ring_time_s;
      video->heard_us = NowTimeUS() + (U64)(delay_s * 1000000.0);

      video->ring_first = (video->ring_first + taken) % video->ring_cap;
      video->ring_count -= taken;
      video->ring_time_s += (F64)taken / (F64)AUD_SAMPLE_RATE;
   }
   MutexDrop(video->mutex);

   if (taken != 0) {
      CondVarBroadcast(video->wake_decode); // room in the ring
   }
}

internal B32
VID_NameIsVideo(String8 name) {
   B32 result = 0;

   String8 extensions[] = {Str8Lit("mp4"), Str8Lit("m4v"), Str8Lit("mov"), Str8Lit("mkv"), Str8Lit("webm"),
                           Str8Lit("avi"), Str8Lit("wmv"), Str8Lit("flv"), Str8Lit("mpg"), Str8Lit("mpeg"),
                           Str8Lit("ts"), Str8Lit("m2ts"), Str8Lit("mts"), Str8Lit("3gp"), Str8Lit("ogv"),
                           Str8Lit("vob")};

   String8 extension = Str8SkipLastDot(name);
   for (U64 i = 0; i < ArrayCount(extensions); i += 1) {
      if (Str8Match(extension, extensions[i], StringMatchFlag_CaseInsensitive)) {
         result = 1;
         break;
      }
   }

   return result;
}

internal void
_VID_DecodeThread(void* p) {
   VID_Video* video = (VID_Video*)p;
   SetThreadNameF("video decode");

   U64 serial = 0;      // the seek this thread is on
   F64 skip_before = 0; // frames and sound from before this are thrown away

   for (;;) {
      //- bgz: wait until there is something to do, and choose what
      B32 do_seek = 0;
      B32 do_sound = 0;
      F64 seek_time_s = 0;
      U8* pixels = 0;
      MutexTake(video->mutex);
      for (;;) {
         if (video->quit || video->seek_serial != serial) {
            break;
         }
         B32 frames_want = (video->queue_count < _VID_QUEUE_CAP && !video->at_end);
         B32 sound_wants = (video->has_sound && !video->sound_at_end &&
                            video->ring_cap - video->ring_count >= _VID_SOUND_CHUNK);
         // Sound first while little of it waits: a gap in sound is heard,
         // a late frame is hardly seen.
         B32 sound_is_low = ((F64)video->ring_count < _VID_RING_AHEAD_SECONDS * (F64)AUD_SAMPLE_RATE);
         if (sound_wants && (sound_is_low || !frames_want)) {
            do_sound = 1;
            break;
         }
         if (frames_want) {
            break;
         }
         CondVarWait(video->wake_decode, video->mutex, max_U64);
      }
      B32 quit = video->quit;
      if (!quit) {
         if (video->seek_serial != serial) {
            serial = video->seek_serial;
            seek_time_s = video->seek_time_s;
            do_seek = 1;
            do_sound = 0;
            video->queue_count = 0;
            video->at_end = 0;
            video->ring_count = 0;
            video->sound_at_end = 0;
         }
         // The next free place. It stays free, and this thread's, until the
         // count goes up below: the main thread only takes from the front.
         pixels = video->queue[(video->queue_first + video->queue_count) % _VID_QUEUE_CAP].pixels;
      }
      MutexDrop(video->mutex);
      if (quit) {
         break;
      }

      if (do_seek) {
         VID_DecoderSeek(video->decoder, seek_time_s);
         skip_before = seek_time_s;
      }

      if (do_sound) {
         //- bgz: decode some sound, and put it in the ring
         F64  time_s = 0;
         F32* chunk = video->sound_chunk;
         U64  count = VID_DecoderNextSound(video->decoder, chunk, _VID_SOUND_CHUNK, &time_s);

         // The part of it from before the time asked for is cut off.
         if (count != 0 && time_s < skip_before) {
            U64 cut = (U64)((skip_before - time_s) * (F64)AUD_SAMPLE_RATE);
            if (cut >= count) {
               continue;
            }
            chunk += cut * AUD_CHANNELS;
            count -= cut;
            time_s += (F64)cut / (F64)AUD_SAMPLE_RATE;
         }

         MutexTake(video->mutex);
         if (video->seek_serial == serial) {
            if (count == 0) {
               video->sound_at_end = 1;
            } else {
               // Only this thread adds to the ring, so there is still the
               // room there was. An empty ring starts at this sound's time.
               if (video->ring_count == 0) {
                  video->ring_time_s = time_s;
               }
               U64 at = (video->ring_first + video->ring_count) % video->ring_cap;
               U64 until_wrap = Min(count, video->ring_cap - at);
               MemoryCopy(video->ring + at * AUD_CHANNELS, chunk, until_wrap * AUD_CHANNELS * sizeof(F32));
               MemoryCopy(video->ring, chunk + until_wrap * AUD_CHANNELS, (count - until_wrap) * AUD_CHANNELS * sizeof(F32));
               video->ring_count += count;
            }
         }
         MutexDrop(video->mutex);
      } else {
         //- bgz: decode one frame
         F64 time_s = 0;
         B32 got_frame = VID_DecoderNextFrame(video->decoder, pixels, &time_s);

         // Half a frame's leeway: the frame that is showing at the time
         // asked for began a little before it.
         F64 frame_s = (video->info.frames_per_second > 0) ? 1.0 / video->info.frames_per_second : 0.04;
         if (got_frame && time_s + frame_s * 0.5 < skip_before) {
            continue;
         }

         // Say it is there - unless a seek was asked for meanwhile.
         MutexTake(video->mutex);
         if (video->seek_serial == serial) {
            if (got_frame) {
               // A seek the main thread asked for and this thread has not
               // seen would have changed seek_serial, so the place is still
               // the one written to.
               video->queue[(video->queue_first + video->queue_count) % _VID_QUEUE_CAP].time_s = time_s;
               video->queue_count += 1;
            } else {
               video->at_end = 1;
            }
         }
         MutexDrop(video->mutex);
      }
   }
}

internal VID_Video*
VID_Open(String8 path) {
   VID_Video* result = 0;

   Arena*          arena = ArenaAlloc();
   VID_DecoderInfo info = {0};
   VID_Decoder*    decoder = VID_DecoderOpen(arena, path, VID_MAX_DIM, AUD_SAMPLE_RATE, &info);
   if (decoder != 0 && (info.size.x <= 0 || info.size.y <= 0)) {
      LogUserErrorF("%S has no picture to show\n", path);
      VID_DecoderClose(decoder);
      decoder = 0;
   }

   if (decoder == 0) {
      ArenaRelease(arena);
   } else {
      VID_Video* video = PushArray(arena, VID_Video, 1);
      video->arena = arena;
      video->path = PushStr8Copy(arena, path);
      video->decoder = decoder;
      video->info = info;

      U64 frame_size = (U64)info.size.x * (U64)info.size.y * 4;
      for (U64 i = 0; i < _VID_QUEUE_CAP; i += 1) {
         video->queue[i].pixels = PushArrayNoZero(arena, U8, frame_size);
      }

      // Black, not see-through, until the first frame is in. The first
      // place in the queue is borrowed for it: the thread is not started yet.
      U8* black = video->queue[0].pixels;
      MemoryZero(black, frame_size);
      for (U64 i = 3; i < frame_size; i += 4) {
         black[i] = 255;
      }
      video->texture = Core_R_Tex2DAlloc(R_ResourceKind_Dynamic, info.size, R_Tex2DFormat_RGBA8, black);

      video->need_frame = 1;
      video->mutex = MutexAlloc();
      video->wake_decode = CondVarAlloc();

      // Sound, if the file has some and the mixer has a voice to spare. If
      // not, the decoder's sound is never asked for and the clock is the
      // computer's.
      if (info.has_sound) {
         video->ring_cap = (U64)AUD_SAMPLE_RATE * _VID_RING_SECONDS;
         video->ring = PushArrayNoZero(arena, F32, video->ring_cap * AUD_CHANNELS);
         video->sound_chunk = PushArrayNoZero(arena, F32, _VID_SOUND_CHUNK * AUD_CHANNELS);
         video->voice = AUD_VoiceAdd(_VID_Voice, video);
         video->has_sound = (video->voice != 0);
      }

      video->thread = ThreadLaunch(_VID_DecodeThread, video);

      LogInfoF("Video %S: %d x %d, %.1f s, %.2f frames a second, %s\n", path, info.size.x, info.size.y, info.duration_s,
               info.frames_per_second, video->has_sound ? "with sound" : "no sound");
      result = video;
   }

   return result;
}

internal void
VID_Close(VID_Video* video) {
   if (video == 0) {
      return;
   }

   // The voice first: after this the audio thread is not in _VID_Voice and
   // never will be again.
   AUD_VoiceRemove(video->voice);

   MutexTake(video->mutex);
   video->quit = 1;
   MutexDrop(video->mutex);
   CondVarBroadcast(video->wake_decode);
   ThreadJoin(video->thread, max_U64);

   VID_DecoderClose(video->decoder);
   Core_R_Tex2DRelease(video->texture);
   CondVarRelease(video->wake_decode);
   MutexRelease(video->mutex);
   ArenaRelease(video->arena);
}

internal R_Handle
VID_Texture(VID_Video* video) {
   return video->texture;
}

internal Vec2S32
VID_Size(VID_Video* video) {
   return video->info.size;
}

internal F64
VID_Duration(VID_Video* video) {
   return video->info.duration_s;
}

internal F64
VID_Position(VID_Video* video) {
   return video->position_s;
}

internal B32
VID_IsPlaying(VID_Video* video) {
   return video->playing;
}

internal B32
VID_HasEnded(VID_Video* video) {
   return video->ended;
}

internal void
VID_SetMuted(VID_Video* video, B32 muted) {
   MutexTake(video->mutex);
   video->sound_muted = muted;
   MutexDrop(video->mutex);
}

internal void
VID_Seek(VID_Video* video, F64 time_s) {
   if (video->info.duration_s > 0) {
      time_s = Min(time_s, video->info.duration_s);
   }
   time_s = Max(time_s, 0.0);

   MutexTake(video->mutex);
   video->seek_serial += 1;
   video->seek_time_s = time_s;
   // What waits is from before the seek: never shown.
   video->queue_count = 0;
   video->at_end = 0;
   video->ring_count = 0;
   video->sound_at_end = 0;
   video->sound_on = 0; // until the picture from there is showing
   video->heard_us = 0;
   MutexDrop(video->mutex);
   CondVarBroadcast(video->wake_decode);

   video->position_s = time_s;
   video->need_frame = 1;
   video->ended = 0;
}

internal void
VID_SetPlaying(VID_Video* video, B32 playing) {
   if (playing && video->ended) {
      VID_Seek(video, 0);
   }
   video->playing = playing;
   if (!playing) {
      // At once, not at the next VID_Update: sound that goes on after
      // Pause is heard.
      MutexTake(video->mutex);
      video->sound_on = 0;
      video->heard_us = 0;
      MutexDrop(video->mutex);
   }
}

internal void
VID_Update(VID_Video* video) {
   U64 now_us = NowTimeUS();
   F64 dt_s = (video->updated_us != 0) ? (F64)(now_us - video->updated_us) / 1000000.0 : 0;
   video->updated_us = now_us;
   // A long gap - the window dragged, the program held up - is not played
   // through at once.
   dt_s = Min(dt_s, 0.1);

   MutexTake(video->mutex);

   // The clock stands still while there is nothing to show: waiting for the
   // first frame after a seek, or for a decoder that can't keep up. A video
   // too heavy for the machine plays slowly rather than in jumps.
   B32 is_waiting = (video->need_frame || (video->queue_count == 0 && !video->at_end));

   // By the sound while there is sound to go by: playing, and not run out.
   B32 sound_is_left = (video->has_sound && !(video->sound_at_end && video->ring_count == 0));
   B32 by_sound = (video->playing && sound_is_left);

   // What is heard now: the note's moment, and on from when it was - or
   // back, if that sound is still on its way to the speakers. No note yet:
   // the sound has not started, and the clock waits for it.
   B32 is_heard = (by_sound && video->sound_on && video->heard_us != 0);
   F64 heard_s = 0;
   if (is_heard) {
      F64 since_s = ((F64)now_us - (F64)video->heard_us) / 1000000.0;
      heard_s = video->heard_time_s + Clamp(-0.5, since_s, 0.5);
   }

   if (by_sound) {
      if (is_heard && !video->need_frame) {
         // Never backwards: two notes a moment apart can disagree by a hair.
         video->position_s = Max(video->position_s, heard_s);
      }
   } else if (video->playing && !is_waiting) {
      video->position_s += dt_s;
   }

   // The latest frame whose time has come. Ones before it were due and are
   // passed over.
   U8* due = 0;
   while (video->queue_count != 0) {
      _VID_Frame* frame = &video->queue[video->queue_first];
      if (video->need_frame) {
         // The first frame after a start or a seek. Stopped, or with no
         // sound to go by, it is shown at once. Playing by sound, its sound
         // has started (below) and it is shown when that is heard: the
         // sound takes a while to come out of the speakers - a fifth of a
         // second, on a Bluetooth headset - and the picture waits for it.
         if (by_sound && !(is_heard && frame->time_s <= heard_s)) {
            break;
         }
      } else if (frame->time_s > video->position_s) {
         break;
      }
      due = frame->pixels;
      if (video->need_frame) {
         video->need_frame = 0;
         // From the frame there is: a seek lands on a frame, not between
         // two. Or from what is heard, which has reached it.
         video->position_s = by_sound ? Max(heard_s, frame->time_s) : Max(video->position_s, frame->time_s);
      }
      video->queue_first = (video->queue_first + 1) % _VID_QUEUE_CAP;
      video->queue_count -= 1;
   }
   if (due != 0) {
      // Before the mutex is let go: the place is free for the decode thread
      // to write over as soon as it is.
      Core_R_FillTex2DRegion(video->texture, R2S32(V2S32(0, 0), video->info.size), due);
   }

   // Sound plays while the video does, once the picture it goes with is
   // there to be shown: not ahead of the first frame after a seek.
   video->sound_on = (video->playing && (!video->need_frame || video->queue_count != 0));
   if (!video->sound_on) {
      video->heard_us = 0;
   }

   if (video->queue_count == 0 && video->at_end) {
      // No frame waiting and none to come.
      video->need_frame = 0;
      video->sound_on = video->playing;
      // The end, once the sound is through as well: some files' sound
      // goes on a little after the last frame.
      if (video->playing && !sound_is_left) {
         video->playing = 0;
         video->ended = 1;
         // The last frame begins a frame before the end: the clock says
         // the end, as the bar and the time should.
         video->position_s = Max(video->position_s, video->info.duration_s);
      }
   }

   MutexDrop(video->mutex);
   if (due != 0) {
      CondVarBroadcast(video->wake_decode);
   }
}
