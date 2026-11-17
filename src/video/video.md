# video

The slideshow's video layer, copied as it is: the player (`video.h`,
`video.c`: a decode thread, sound through `src/audio`, a clock and a
texture), `video_decoder.h`, the seam, and `video_ffmpeg.c`, the only file
that includes FFmpeg's headers. The slideshow's `video.md` has the whole of
it.

Change these in the slideshow first, then copy them across. One change was
made here first, with J's say-so, and is to be copied back: the decoder
gained `VID_DecoderOpenOnCard` (the video decoded by the graphics card,
through FFmpeg's Direct3D 11 decoder, and by the processor if the card
can't) and `VID_DecoderNextFrameFrom` (the first frame at or after a time;
the ones before are decoded and let go without being made into pixels).
What was there behaves as it did.

## Used by

- `src/indexer`: `VID_DecoderOpenOnCard` (or `VID_DecoderOpen` with
  `--cpu`), `VID_DecoderNextFrameFrom`, `VID_DecoderNextSound`,
  `VID_DecoderClose`, on the indexer's thread.
- `src/thumbs`: the same and `VID_DecoderSeek`, on its thread.
- `frame.c`: the player, for the clip being played (`VID_Open`, `VID_Seek`,
  `VID_SetPlaying`, `VID_Update`, `VID_Texture`, `VID_Close`).

## FFmpeg

`src/third_party/ffmpeg` is the slideshow's: the BtbN LGPL shared build,
headers, import libraries and DLLs. `build.bat` copies the DLLs next to the
exe. A program that ships with them has to say that it uses FFmpeg and under
which licence, and let the DLLs be replaced.
