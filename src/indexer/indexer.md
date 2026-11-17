# indexer

Goes through the library's videos on a thread of its own: decodes each with
the decoder (`src/video`) at `IDX_FRAME_DIM` (400 px on the longer side),
takes a frame every `IDX_SAMPLE_SECONDS` (1 s), runs those through the image
model (`src/embed`) `IDX_BATCH` (8) at a time, and writes the vectors and
the frames' times to the video's index file (`src/index`). A video whose
index file is up to date is not read again.

## State

`IDX_Indexer` (indexer.c), one, made in `_EntryPoint` and kept in
`App_State.indexer`, with the index folder (`bin\local\index`) and the model
folder (`data\models\clip-vit-base-patch32`).

- A run: one pass over the videos as they were when it started. Their paths
  are copied onto the indexer's arena, so the library can change under it.
- `library_gen`: the library's generation the run is for. A different one
  stops the run (`cancel`, then the thread is joined) and starts another.
- `model`: loaded by the first run's thread and kept for the runs after.
  None means nothing is indexed (`no_model`).
- `done`, `failed`, `up_to_date`, `frames`, `current`: written by the
  thread, read by the main thread with atomics.
- `logs`: what the thread logged (on `thread_log`), under `log_mutex`, for
  `IDX_Update` to log on the main thread.

## Functions

- `IDX_Alloc(index_folder, model_folder)`
- `IDX_Update(indexer, library)`: once a frame, from `FrameWork`. Logs what
  the thread has logged; starts a run when the library's generation has
  changed. When a run has gone through every video, logs how many frames,
  how long it took, how many were up to date and how many failed.
- `IDX_GetStatus`: how many videos, done, failed, frames indexed, whether
  there is a model, and the name of the one under way. The status line shows
  it.
- `IDX_Stop`: cancels and joins, and closes the model, at the end of the
  program.

## How a video is read

Every frame is decoded (a codec can't skip to the next second without
decoding what is between), and the first one at or after each whole second
is taken. A video that can't be opened, or that the model fails on, counts
as failed. One stopped part way (the library changed, or the program
closed) is not written, and is read again next time. A line is logged per
video indexed: how many frames and how long it took.
