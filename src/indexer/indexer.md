# indexer

Goes through the library's videos on a thread of its own, decoding each with
the decoder (`src/video`) and taking a frame every `IDX_SAMPLE_SECONDS` (1 s),
at `IDX_FRAME_DIM` (256 px on the longer side). These are the frames the
search will look at.

For now (step 1 of `Claude outputs/clip_search_plan.md`) the frames are only
counted. Step 2 runs each through the image model and writes the vectors to
disk; step 3 searches them.

## State

`IDX_Indexer` (indexer.c), one, made in `_EntryPoint` and kept in
`App_State.indexer`.

- A run: one pass over the videos as they were when it started. Their paths
  are copied onto the indexer's arena, so the library can change under it.
- `library_gen`: the library's generation the run is for. A different one
  stops the run (`cancel`, then the thread is joined) and starts another.
- `done`, `failed`, `frames`, `current`: written by the thread, read by the
  main thread with atomics.

## Functions

- `IDX_Alloc`
- `IDX_Update(indexer, library)`: once a frame, from `FrameWork`. Starts a
  run when the library's generation has changed.
- `IDX_GetStatus`: how many videos, done, failed, frames taken, and the name
  of the one under way. The status line shows it.
- `IDX_Stop`: cancels and joins, at the end of the program.

## How a video is read

Every frame is decoded (a codec can't skip to the next second without
decoding what is between), scaled to 256 px, and the first one at or after
each whole second is taken. A video that can't be opened counts as failed.
The decoder's errors are logged on the indexer's thread, where no log is
selected, so they are not seen yet.

## Not done yet

- The frames are not kept or encoded (step 2).
- Nothing is saved: every start of the program reads every video again.
- Errors from the thread reaching the log.
