////////////////////////////////
//~ bgz: audio: the mixer
//
// See audio.h. Nothing here knows what plays the sound: that is
// audio_device.h.
//
// The device asks for sound (_AUD_Mix, on its thread). Each voice in turn
// writes what it has into a buffer of silence, and the buffers are added
// up. The list of voices is under the mutex, which the device's thread
// holds while it mixes: a voice being removed waits for the mix to finish,
// and is then never called again.

#define AUD_MAX_VOICES 16
// The most frames mixed in one go. The device asks for less: this is for
// the voices' buffer, which is not on the stack.
#define _AUD_MIX_FRAMES 8192

struct AUD_Voice {
   AUD_VoiceFunction* function; // 0: free
   void*              user;
};

typedef struct _AUD_State _AUD_State;
struct _AUD_State {
   Arena*      arena;
   AUD_Device* device;
   Mutex       mutex;

   //- bgz: under the mutex
   AUD_Voice voices[AUD_MAX_VOICES];
   B32       muted;
   F32*      voice_samples; // one voice's, before it is added in
};

global _AUD_State* _aud_state = 0;

internal void
_AUD_Mix(void* user, F32* samples, U64 frame_count, F64 delay_s) {
   _AUD_State* state = (_AUD_State*)user;
   U64         sample_count = frame_count * AUD_CHANNELS;
   MemoryZero(samples, sample_count * sizeof(F32));

   MutexTake(state->mutex);
   // More than the buffer holds is mixed a buffer at a time.
   for (U64 frame = 0; frame < frame_count;) {
      U64  frames = Min(frame_count - frame, (U64)_AUD_MIX_FRAMES);
      F64  delay = delay_s + (F64)frame / (F64)AUD_SAMPLE_RATE;
      F32* out = samples + frame * AUD_CHANNELS;

      for (U64 i = 0; i < AUD_MAX_VOICES; i += 1) {
         AUD_Voice* voice = &state->voices[i];
         if (voice->function == 0) {
            continue;
         }
         MemoryZero(state->voice_samples, frames * AUD_CHANNELS * sizeof(F32));
         voice->function(voice->user, state->voice_samples, frames, delay);
         if (!state->muted) {
            for (U64 s = 0; s < frames * AUD_CHANNELS; s += 1) {
               out[s] += state->voice_samples[s];
            }
         }
      }
      frame += frames;
   }
   MutexDrop(state->mutex);

   // Several loud voices together can go past what a speaker can play.
   for (U64 s = 0; s < sample_count; s += 1) {
      samples[s] = Clamp(-1.0f, samples[s], 1.0f);
   }
}

internal void
AUD_Init(void) {
   if (_aud_state != 0) {
      return;
   }
   Arena* arena = ArenaAlloc();
   _aud_state = PushArray(arena, _AUD_State, 1);
   _aud_state->arena = arena;
   _aud_state->mutex = MutexAlloc();
   _aud_state->voice_samples = PushArray(arena, F32, _AUD_MIX_FRAMES * AUD_CHANNELS);
   _aud_state->device = AUD_DeviceOpen(arena, _AUD_Mix, _aud_state);
}

internal void
AUD_Shutdown(void) {
   if (_aud_state == 0) {
      return;
   }
   AUD_DeviceClose(_aud_state->device);
   MutexRelease(_aud_state->mutex);
   ArenaRelease(_aud_state->arena);
   _aud_state = 0;
}

internal AUD_Voice*
AUD_VoiceAdd(AUD_VoiceFunction* function, void* user) {
   AUD_Voice* result = 0;
   if (_aud_state != 0 && function != 0) {
      MutexTake(_aud_state->mutex);
      for (U64 i = 0; i < AUD_MAX_VOICES; i += 1) {
         if (_aud_state->voices[i].function == 0) {
            result = &_aud_state->voices[i];
            result->user = user;
            result->function = function;
            break;
         }
      }
      MutexDrop(_aud_state->mutex);
   }
   return result;
}

internal void
AUD_VoiceRemove(AUD_Voice* voice) {
   if (_aud_state != 0 && voice != 0) {
      MutexTake(_aud_state->mutex);
      voice->function = 0;
      voice->user = 0;
      MutexDrop(_aud_state->mutex);
   }
}

internal B32
AUD_IsMuted(void) {
   B32 result = 0;
   if (_aud_state != 0) {
      MutexTake(_aud_state->mutex);
      result = _aud_state->muted;
      MutexDrop(_aud_state->mutex);
   }
   return result;
}

internal void
AUD_SetMuted(B32 muted) {
   if (_aud_state != 0) {
      MutexTake(_aud_state->mutex);
      _aud_state->muted = muted;
      MutexDrop(_aud_state->mutex);
   }
}
