////////////////////////////////
//~ bgz: thumbs
//
// See thumbs.h.
//
// A slot is one thumbnail, on its way or there. The main thread and the
// layer's thread both read and write the slots, under mutex; the decoding
// itself is done outside it, into the thread's own pixels, and a slot being
// decoded is left alone by the main thread until it is done. Textures are
// made and let go of on the main thread only.

typedef enum _THM_State {
   _THM_State_Free,
   _THM_State_Wanted,   // asked for; the thread has not got to it
   _THM_State_Decoding, // the thread is on it: not to be given to another
   _THM_State_Decoded,  // pixels are in; no texture yet
   _THM_State_Ready,    // has its texture
   _THM_State_Failed,
} _THM_State;

typedef struct _THM_Slot _THM_Slot;
struct _THM_Slot {
   _THM_State state;
   U64        key;       // of the path and the second
   U8         path[1024];
   U64        path_size;
   F64        time_s;
   U64        asked;     // the frame it was last asked for in
   U8*        pixels;    // THM_DIM * THM_DIM * 4, used up to size
   Vec2S32    size;
   R_Handle   texture;
};

struct THM_Thumbs {
   Arena*    arena;
   Thread    thread;
   Mutex     mutex;
   CondVar   wanted; // signalled when a slot becomes wanted, or at the end
   B32       stop;   // under mutex
   U64       frame;
   _THM_Slot slots[THM_CAP];
};

internal void
_THM_ThreadEntry(void* p) {
   THM_Thumbs* thumbs = (THM_Thumbs*)p;
   SetThreadName(Str8Lit("Thumbs"));
   Arena* arena = ArenaAlloc();
   U8*    pixels = PushArrayNoZero(arena, U8, THM_DIM * THM_DIM * 4);

   MutexTake(thumbs->mutex);
   while (!thumbs->stop) {
      // The one asked for last: what is on screen now, before what was
      // scrolled past.
      _THM_Slot* slot = 0;
      for (U64 i = 0; i < THM_CAP; i += 1) {
         _THM_Slot* s = &thumbs->slots[i];
         if (s->state == _THM_State_Wanted && (slot == 0 || s->asked > slot->asked)) {
            slot = s;
         }
      }
      if (slot == 0) {
         CondVarWait(thumbs->wanted, thumbs->mutex, max_U64);
         continue;
      }
      slot->state = _THM_State_Decoding;
      MutexDrop(thumbs->mutex);

      // The slot's path and time don't change while it is being decoded.
      Temp            temp = TempBegin(arena);
      VID_DecoderInfo info = {0};
      VID_Decoder*    decoder = VID_DecoderOpen(arena, Str8(slot->path, slot->path_size), THM_DIM, 0, &info);
      B32             ok = 0;
      if (decoder != 0) {
         if (info.size.x > 0 && info.size.y > 0 && info.size.x <= THM_DIM && info.size.y <= THM_DIM) {
            // The seek lands on a frame that can be decoded on its own, at
            // or before the time: on from there to the time. Not for ever,
            // in case the times make no sense.
            VID_DecoderSeek(decoder, slot->time_s);
            F64 time_s = 0;
            for (U64 n = 0; n < 2000 && VID_DecoderNextFrame(decoder, pixels, &time_s); n += 1) {
               ok = 1;
               if (time_s >= slot->time_s) break;
            }
         }
         VID_DecoderClose(decoder);
      }
      TempEnd(temp);

      MutexTake(thumbs->mutex);
      if (ok) {
         MemoryCopy(slot->pixels, pixels, (U64)info.size.x * (U64)info.size.y * 4);
         slot->size = info.size;
         slot->state = _THM_State_Decoded;
      } else {
         slot->state = _THM_State_Failed;
      }
   }
   MutexDrop(thumbs->mutex);
   ArenaRelease(arena);
}

internal THM_Thumbs*
THM_Alloc(void) {
   Arena*      arena = ArenaAlloc();
   THM_Thumbs* thumbs = PushArray(arena, THM_Thumbs, 1);
   thumbs->arena = arena;
   thumbs->mutex = MutexAlloc();
   thumbs->wanted = CondVarAlloc();
   thumbs->thread = ThreadLaunch(_THM_ThreadEntry, thumbs);
   return thumbs;
}

internal void
THM_Stop(THM_Thumbs* thumbs) {
   MutexTake(thumbs->mutex);
   thumbs->stop = 1;
   MutexDrop(thumbs->mutex);
   CondVarBroadcast(thumbs->wanted);
   ThreadJoin(thumbs->thread, max_U64);
}

internal void
THM_Update(THM_Thumbs* thumbs) {
   thumbs->frame += 1;
   MutexTake(thumbs->mutex);
   for (U64 i = 0; i < THM_CAP; i += 1) {
      _THM_Slot* slot = &thumbs->slots[i];
      if (slot->state == _THM_State_Decoded) {
         slot->texture = Core_R_Tex2DAlloc(R_ResourceKind_Static, slot->size, R_Tex2DFormat_RGBA8, slot->pixels);
         slot->state = _THM_State_Ready;
      }
   }
   MutexDrop(thumbs->mutex);
}

internal THM_Thumb
THM_Get(THM_Thumbs* thumbs, String8 path, F64 time_s) {
   THM_Thumb result = {0};
   U64       second = (U64)Max(time_s, 0.0);
   U64       key = U64HashFromSeedStr8(second + 1, path);

   MutexTake(thumbs->mutex);
   _THM_Slot* slot = 0;
   _THM_Slot* oldest = 0;
   for (U64 i = 0; i < THM_CAP && slot == 0; i += 1) {
      _THM_Slot* s = &thumbs->slots[i];
      if (s->state != _THM_State_Free && s->key == key) {
         slot = s;
      } else if (s->state != _THM_State_Decoding && s->asked != thumbs->frame && (oldest == 0 || s->asked < oldest->asked)) {
         // Not one asked for this frame: its texture may be on screen.
         oldest = s;
      }
   }

   // Not there: in the place of the one asked for longest ago. A path too
   // long for a slot has no thumbnail.
   if (slot == 0 && oldest != 0 && path.size <= sizeof(oldest->path)) {
      slot = oldest;
      if (slot->state == _THM_State_Ready) {
         Core_R_Tex2DRelease(slot->texture);
      }
      if (slot->pixels == 0) {
         slot->pixels = PushArrayNoZero(thumbs->arena, U8, THM_DIM * THM_DIM * 4);
      }
      slot->texture = Core_R_HandleZero();
      slot->state = _THM_State_Wanted;
      slot->key = key;
      slot->time_s = (F64)second;
      slot->path_size = path.size;
      MemoryCopy(slot->path, path.str, path.size);
      CondVarSignal(thumbs->wanted);
   }

   if (slot != 0) {
      slot->asked = thumbs->frame;
      if (slot->state == _THM_State_Ready) {
         result.texture = slot->texture;
         result.size = slot->size;
      }
   }
   MutexDrop(thumbs->mutex);
   return result;
}
