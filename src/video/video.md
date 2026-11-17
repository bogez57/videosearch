# video

The slideshow's decoder, copied as it is: `video_decoder.h`, the seam, and
`video_ffmpeg.c`, the only file that includes FFmpeg's headers. The
slideshow's `video.md` has the whole of it. The player (`video.c`, the decode
thread, sound and the texture) is not here yet: indexing needs only the
decoder. It comes with step 4, to play a hit.

Change these in the slideshow first, then copy them across.

## Used by

- `src/indexer`: `VID_DecoderOpen` (with no sound), `VID_DecoderNextFrame`,
  `VID_DecoderClose`, on the indexer's thread.

## FFmpeg

`src/third_party/ffmpeg` is the slideshow's: the BtbN LGPL shared build,
headers, import libraries and DLLs. `build.bat` copies the DLLs next to the
exe. A program that ships with them has to say that it uses FFmpeg and under
which licence, and let the DLLs be replaced.
