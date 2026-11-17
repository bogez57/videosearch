////////////////////////////////
//~ bgz: audio
//
// Sound out of the speakers. bgz has none, so it is here, in the app.
//
// The layer is in two parts, as video is, so that what talks to the sound
// card can be changed without anything else knowing (audio.md):
//
//    audio.h, audio.c     the mixer. Voices are added to it, and it adds up
//                         what they play. Knows nothing of Windows.
//    audio_device.h       what the mixer asks of a device: open and close.
//    audio_wasapi.c       those two, done with Windows' own audio (WASAPI).
//                         The only file that knows it is there.
//
// The rest of the program uses only what is declared in this file.
//
// Sound is always two channels of F32, left then right, AUD_SAMPLE_RATE of
// such pairs - "frames" - to the second. The device turns that into
// whatever the sound card wants.
//
// A voice is something that plays: a function the mixer calls, on the
// audio thread, whenever the device wants more sound.

#ifndef AUDIO_H
#define AUDIO_H

#define AUD_SAMPLE_RATE 48000
#define AUD_CHANNELS    2

// Called on the audio thread. To write up to frame_count frames into samples,
// which is silence already, and return - quickly: no files, no waiting on
// anything slow. delay_s is how long from now the first of these frames
// will be heard, for a voice that keeps time by what is heard.
typedef void AUD_VoiceFunction(void* user, F32* samples, U64 frame_count, F64 delay_s);

typedef struct AUD_Voice AUD_Voice;

// Starts the device. Before any voice is added. With no sound card, or one
// that can't be opened, everything works as it does with one - voices are
// called at the same pace - and nothing is heard.
internal void AUD_Init(void);
internal void AUD_Shutdown(void);

// 0 if there are too many voices already (AUD_MAX_VOICES).
internal AUD_Voice* AUD_VoiceAdd(AUD_VoiceFunction* function, void* user);
// When this returns the function is not running and will not be called again.
internal void AUD_VoiceRemove(AUD_Voice* voice);

// Everything, at once. Muted, voices are still called: they keep their time.
internal B32  AUD_IsMuted(void);
internal void AUD_SetMuted(B32 muted);

#endif // AUDIO_H
