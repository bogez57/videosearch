# audio

Sound out of the speakers. bgz has none, so it lives here, in the app.

## The parts

```
whatever plays (video.c today)
        |  uses only audio.h
        v
audio.h / audio.c          the mixer: voices, added up
        |  uses only audio_device.h
        v
audio_device.h             what a device is: open and close
        ^
        |  defines them
audio_wasapi.c             the device there is, done with Windows' audio
```

- **`audio.h`** is all the rest of the program sees: `AUD_Init`,
  `AUD_Shutdown`, `AUD_VoiceAdd`, `AUD_VoiceRemove`, `AUD_IsMuted`,
  `AUD_SetMuted`. Sound is always two channels of `F32` at `AUD_SAMPLE_RATE`.
- **`audio.c`** is the mixer. A voice is a function; when the device wants
  sound the mixer calls every voice and adds up what they write.
- **`audio_device.h`** is the seam: `AUD_DeviceOpen` takes a function to call
  for sound, `AUD_DeviceClose` stops calling it.
- **`audio_wasapi.c`** is those two done with WASAPI. It is the only file that
  includes Windows' audio headers.

## Changing the device

1. Write `audio_<other>.c` that defines the two functions in `audio_device.h`.
2. In `src\main.c`, include it in place of `audio/audio_wasapi.c`.

Nothing else changes.

## No sound card

A device never fails to open. With no card, or when the card goes away, it
goes on asking the mixer for sound at the pace sound would be played and
throws it away. A video keeps time by its sound, so this is what lets it play
at the right speed on a machine with nothing to hear it on.

## Not done yet

- How long sound takes to be heard, past the card's own buffer, is what
  Windows' clock for the stream says (audio_wasapi.c). On a Bluetooth
  headset that was already playing when the stream opened, that clock was
  seen to say about 60 ms where Windows' mix showed about 200 ms: a video's
  picture is then that much ahead of its sound. Opened while the headset was
  idle, the two agree.

- Volume, other than off and on.
- Following Windows when another card is made the default while the program
  runs. The card in use is kept until it stops answering.
