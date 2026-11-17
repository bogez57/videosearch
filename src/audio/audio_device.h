////////////////////////////////
//~ bgz: audio: what the mixer asks of a device
//
// The mixer (audio.c) is written against these two functions and nothing
// else. A file that defines them is a device: audio_wasapi.c is the one
// there is. To play sound through something else, write another such file
// and include it in main.c in place of that one.

#ifndef AUDIO_DEVICE_H
#define AUDIO_DEVICE_H

// Called on the device's own thread whenever it wants more sound: to write
// frame_count frames (AUD_CHANNELS of F32 each, AUD_SAMPLE_RATE to the
// second) into samples. delay_s: how long from now the first of them will
// be heard.
typedef void AUD_DeviceFunction(void* user, F32* samples, U64 frame_count, F64 delay_s);

typedef struct AUD_Device AUD_Device; // the device's own

// Starts a thread that calls function for as long as the device is open.
// Never 0: with no sound card, or when the card goes away, the function is
// still called, at the pace sound would be played at, and what it writes is
// thrown away - so that whatever keeps time by the sound still does. A card
// that turns up later is used.
internal AUD_Device* AUD_DeviceOpen(Arena* arena, AUD_DeviceFunction* function, void* user);
// When this returns the function is not running and will not be called again.
internal void        AUD_DeviceClose(AUD_Device* device);

#endif // AUDIO_DEVICE_H
