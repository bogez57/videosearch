# index

The vectors of each video, on disk: one file per video in `bin\local\index`,
named by a hash of the video's path (`<16 hex digits>.vsi`).

## The file

`INX_Header` (index.h), then the video's path, then a F32 time in seconds
per frame, then each frame's vector as 512 F16s. The header says which video
it was made from (its path, size and time written), the model (its folder's
name), the time between frames, and how many frames.

A file is up to date while all of that matches the video and the program as
they are now. One that is not is written again whole.

## Functions

- `INX_HeaderFor(video_path, props, model, sample_seconds)`: what a file for
  the video should say.
- `INX_FilePath(arena, folder, video_path)`
- `INX_IsUpToDate(file_path, want, count_out)`
- `INX_Write(file_path, header, video_path, times, vectors)`: to
  `<file>.part`, then moved over the file, so a file is whole or not there.
- `INX_F16FromF32`: rounds to the nearest, as a cast to `float16` does
  (checked against numpy on 200,000 numbers).

## Not done yet

- Reading the vectors back, for the search (step 3).
- Index files of videos no longer in the library are not deleted.

## Used by

- `indexer.c`, which writes them.
