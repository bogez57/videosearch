////////////////////////////////
//~ bgz: audio: the device, done with Windows' own audio (WASAPI)
//
// The two functions of audio_device.h. This is the only file in the program
// that includes Windows' audio headers or calls them: to play sound through
// something else, write another file with the same two functions and
// include that one in main.c instead.
//
// A thread opens the sound card Windows has as its default, in shared mode,
// and is woken by Windows each time the card has room for more sound. It
// then asks the mixer for exactly that much. The sound is handed over as
// two channels of F32 at AUD_SAMPLE_RATE, and Windows is asked to convert
// it to whatever the card itself is set to.
//
// With no card, or when the card stops answering - unplugged, turned off -
// the thread goes on asking the mixer for sound at the same pace, throws it
// away, and tries the default card again once a second.
//
// A card that is the default when the program starts stays the one used
// until it stops answering: making another one the default in Windows does
// not move the sound over.

#pragma push_macro("internal")
#pragma push_macro("global")
#undef internal
#undef global
#define COBJMACROS
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#pragma pop_macro("global")
#pragma pop_macro("internal")

#pragma comment(lib, "ole32")

// Windows' names for the things asked for, written out: in C its headers
// only declare them, and which library holds them has changed over the
// years.
static const GUID _aud_clsid_device_enumerator = {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
static const GUID _aud_iid_device_enumerator = {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
static const GUID _aud_iid_audio_client = {0x1CB9AD4C, 0xDBFA, 0x4C32, {0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2}};
static const GUID _aud_iid_render_client = {0xF294ACFC, 0x3146, 0x4483, {0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2}};
static const GUID _aud_iid_audio_clock = {0xCD63314F, 0x3FBA, 0x4A1B, {0x81, 0x2C, 0xEF, 0x96, 0x35, 0x87, 0x28, 0xE7}};
static const GUID _aud_format_float = {0x00000003, 0x0000, 0x0010, {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71}};

// How much sound the card is asked to hold: less is less delay between a
// voice and the ear, more is safer against gaps. In 100-nanosecond units.
#define _AUD_BUFFER_100NS 500000 // 50 ms

// With no card: how often the mixer is asked, and for how much at a time
// at most.
#define _AUD_SILENT_MS     10
#define _AUD_SILENT_FRAMES (AUD_SAMPLE_RATE / 20)

struct AUD_Device {
   AUD_DeviceFunction* function;
   void*               user;
   Thread              thread;
   HANDLE              quit_event; // set: the thread is to end
   F32*                silent_samples; // where sound that nobody hears is written
};

// Plays through the default card until it stops answering or the device is
// closed. Returns at once if there is no card to open.
internal void
_AUD_PlayThroughCard(AUD_Device* device) {
   IMMDeviceEnumerator* enumerator = 0;
   IMMDevice*           card = 0;
   IAudioClient*        client = 0;
   IAudioRenderClient*  render = 0;
   HANDLE               wake_event = CreateEventW(0, 0, 0, 0);
   UINT32               buffer_frames = 0;

   WAVEFORMATEXTENSIBLE format = {0};
   format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
   format.Format.nChannels = AUD_CHANNELS;
   format.Format.nSamplesPerSec = AUD_SAMPLE_RATE;
   format.Format.wBitsPerSample = 32;
   format.Format.nBlockAlign = (WORD)(AUD_CHANNELS * 4);
   format.Format.nAvgBytesPerSec = AUD_SAMPLE_RATE * AUD_CHANNELS * 4;
   format.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
   format.Samples.wValidBitsPerSample = 32;
   format.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
   format.SubFormat = _aud_format_float;

   // AUTOCONVERTPCM: Windows turns our form of sound into the card's own.
   DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                 AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;

   B32 is_open = (wake_event != 0 &&
                  SUCCEEDED(CoCreateInstance(&_aud_clsid_device_enumerator, 0, CLSCTX_ALL, &_aud_iid_device_enumerator,
                                             (void**)&enumerator)) &&
                  SUCCEEDED(IMMDeviceEnumerator_GetDefaultAudioEndpoint(enumerator, eRender, eConsole, &card)) &&
                  SUCCEEDED(IMMDevice_Activate(card, &_aud_iid_audio_client, CLSCTX_ALL, 0, (void**)&client)) &&
                  SUCCEEDED(IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED, flags, _AUD_BUFFER_100NS, 0,
                                                    &format.Format, 0)) &&
                  SUCCEEDED(IAudioClient_SetEventHandle(client, wake_event)) &&
                  SUCCEEDED(IAudioClient_GetBufferSize(client, &buffer_frames)) &&
                  SUCCEEDED(IAudioClient_GetService(client, &_aud_iid_render_client, (void**)&render)) &&
                  SUCCEEDED(IAudioClient_Start(client)));

   // The stream's clock, for how far behind what is written the speakers
   // are. Without it, the card's buffer alone is taken to be the delay.
   IAudioClock* clock = 0;
   UINT64       clock_frequency = 0; // the clock's units in a second
   U64          written_frames = 0;
   F64          beyond_card_s = 0; // the delay past the card's buffer (below)
   if (is_open && SUCCEEDED(IAudioClient_GetService(client, &_aud_iid_audio_clock, (void**)&clock))) {
      if (FAILED(IAudioClock_GetFrequency(clock, &clock_frequency))) {
         clock_frequency = 0;
      }
   }

   if (is_open) {
      HANDLE events[2] = {device->quit_event, wake_event};
      U32    missed = 0; // times in a row the card did not ask for sound
      for (;;) {
         DWORD waited = WaitForMultipleObjects(2, events, 0, 200);
         if (waited == WAIT_OBJECT_0) {
            break; // closed
         }
         if (waited == WAIT_TIMEOUT) {
            // A card that has gone quiet for a second is taken to be gone.
            missed += 1;
            if (missed >= 5) {
               break;
            }
            continue;
         }
         if (waited != WAIT_OBJECT_0 + 1) {
            break;
         }
         missed = 0;

         // What the card still holds is heard before anything written now.
         // The rest of its buffer is the room.
         UINT32 held_frames = 0;
         if (FAILED(IAudioClient_GetCurrentPadding(client, &held_frames))) {
            break;
         }
         UINT32 room_frames = (buffer_frames > held_frames) ? buffer_frames - held_frames : 0;
         if (room_frames == 0) {
            continue;
         }
         BYTE* data = 0;
         if (FAILED(IAudioRenderClient_GetBuffer(render, room_frames, &data))) {
            break;
         }
         // The delay: how long from now until what is written now is heard.
         // What the card holds is the least of it. Windows keeps more of its
         // own past the card's buffer - a Bluetooth headset, a fifth of a
         // second - which only the stream's clock tells: everything written
         // so far, less what it says has been played.
         F64 held_s = (F64)held_frames / (F64)AUD_SAMPLE_RATE;
         if (clock != 0 && clock_frequency != 0) {
            UINT64 played = 0;
            UINT64 played_qpc = 0; // when it said so, in 100 ns: the same clock as NowTimeUS
            if (SUCCEEDED(IAudioClock_GetPosition(clock, &played, &played_qpc))) {
               F64 written_s = (F64)written_frames / (F64)AUD_SAMPLE_RATE;
               F64 played_s = (F64)played / (F64)clock_frequency;
               F64 since_s = ((F64)NowTimeUS() - (F64)played_qpc / 10.0) / 1000000.0;
               F64 clock_delay_s = written_s - played_s - since_s;
               // The part past the card's buffer, kept at the most it has
               // been. A Bluetooth headset's clock was seen to jump ahead a
               // seventh of a second part way through, while the sound, as
               // Windows mixes it, kept its delay: a delay grows when sound
               // is held up, and does not shrink unless sound is thrown away.
               // More than a second is not believed.
               if (clock_delay_s < 1.0) {
                  beyond_card_s = Max(beyond_card_s, clock_delay_s - held_s);
               }
            }
         }
         device->function(device->user, (F32*)data, room_frames, held_s + beyond_card_s);
         if (FAILED(IAudioRenderClient_ReleaseBuffer(render, room_frames, 0))) {
            break;
         }
         written_frames += room_frames;
      }
      IAudioClient_Stop(client);
   }

   if (clock != 0) IAudioClock_Release(clock);
   if (render != 0) IAudioRenderClient_Release(render);
   if (client != 0) IAudioClient_Release(client);
   if (card != 0) IMMDevice_Release(card);
   if (enumerator != 0) IMMDeviceEnumerator_Release(enumerator);
   if (wake_event != 0) CloseHandle(wake_event);
}

internal void
_AUD_DeviceThread(void* p) {
   AUD_Device* device = (AUD_Device*)p;
   SetThreadNameF("audio");

   HRESULT com = CoInitializeEx(0, COINIT_MULTITHREADED);

   for (;;) {
      _AUD_PlayThroughCard(device);
      if (WaitForSingleObject(device->quit_event, 0) == WAIT_OBJECT_0) {
         break;
      }

      // No card, for now: a second of asking for sound that goes nowhere,
      // then the card is tried again. As much is asked for as the time that
      // has passed would have played - Windows' waits are not exact.
      B32 quit = 0;
      U64 began_us = NowTimeUS();
      U64 asked_frames = 0;
      while (NowTimeUS() - began_us < 1000000) {
         if (WaitForSingleObject(device->quit_event, _AUD_SILENT_MS) == WAIT_OBJECT_0) {
            quit = 1;
            break;
         }
         U64 due_frames = (NowTimeUS() - began_us) * AUD_SAMPLE_RATE / 1000000;
         while (asked_frames < due_frames) {
            U64 frames = Min(due_frames - asked_frames, (U64)_AUD_SILENT_FRAMES);
            device->function(device->user, device->silent_samples, frames, 0);
            asked_frames += frames;
         }
      }
      if (quit) {
         break;
      }
   }

   if (SUCCEEDED(com)) {
      CoUninitialize();
   }
}

internal AUD_Device*
AUD_DeviceOpen(Arena* arena, AUD_DeviceFunction* function, void* user) {
   AUD_Device* device = PushArray(arena, AUD_Device, 1);
   device->function = function;
   device->user = user;
   device->silent_samples = PushArray(arena, F32, _AUD_SILENT_FRAMES * AUD_CHANNELS);
   // Manual reset: once set it stays set, for every wait that asks.
   device->quit_event = CreateEventW(0, 1, 0, 0);
   device->thread = ThreadLaunch(_AUD_DeviceThread, device);
   return device;
}

internal void
AUD_DeviceClose(AUD_Device* device) {
   if (device == 0) {
      return;
   }
   SetEvent(device->quit_event);
   ThreadJoin(device->thread, max_U64);
   CloseHandle(device->quit_event);
}
