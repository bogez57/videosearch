# video

The slideshow's video layer, copied as it is: the player (`video.h`,
`video.c`: a decode thread, sound through `src/audio`, a clock and a
texture), `video_decoder.h`, the seam, and `video_ffmpeg.c`, the only file
that includes FFmpeg's headers. The slideshow's `video.md` has the whole of
it.

Change these in the slideshow first, then copy them across.

## Used by

- `src/indexer`: `VID_DecoderOpen` (with no sound), `VID_DecoderNextFrame`,
  `VID_DecoderClose`, on the indexer's thread.
- `src/thumbs`: the same and `VID_DecoderSeek`, on its thread.
- `frame.c`: the player, for the clip being played (`VID_Open`, `VID_Seek`,
  `VID_SetPlaying`, `VID_Update`, `VID_Texture`, `VID_Close`).

## FFmpeg

`src/third_party/ffmpeg` is the slideshow's: the BtbN LGPL shared build,
headers, import libraries and DLLs. `build.bat` copies the DLLs next to the
exe. A program that ships with them has to say that it uses FFmpeg and under
which licence, and let the DLLs be replaced.
