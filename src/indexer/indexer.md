# indexer

Goes through the library's videos on a thread of its own: decodes each with
the decoder (`src/video`) at `IDX_FRAME_DIM` (400 px on the longer side),
takes a frame every `IDX_SAMPLE_SECONDS` (1 s), runs those through the image
model (`src/embed`) `IDX_BATCH` (8) at a time, and writes the vectors and
the frames' times to the video's index file (`src/index`). A video whose
index file is up to date is not read again.

With the sound model loaded, the video's sound goes the same way in the same
pass: `IDX_SOUND_SECONDS` (5 s) at a time, one stretch after another, through
the sound model and into the video's sound index file.

## State

`IDX_Indexer` (indexer.c), one, made in `_EntryPoint` and kept in
`App_State.indexer`, with the index folder (`bin\local\index`), the model
(`App_State.model`) and its name, which goes in the index files.

- A run: one pass over the videos as they were when it started. Their paths
  are copied onto the indexer's arena, so the library can change under it.
- `library_gen`: the library's generation the run is for. A different one
  stops the run (`cancel`, then the thread is joined) and starts another.
- `model`: its halves loaded (`EMB_Load`) by the first run's thread. None,
  or one that failed to load, means nothing is indexed (`no_model`).
- `done`, `failed`, `up_to_date`, `frames`, `current`, and `indexed` and
  `sound_indexed` (one a video each, set when its file is up to date): written by the thread, read by
  the main thread with atomics.
- `logs`: what the thread logged (on `thread_log`), under `log_mutex`, for
  `IDX_Update` to log on the main thread.

## Functions

- `IDX_Alloc(index_folder, model_name, sound_model_name, model)`
- `IDX_Update(indexer, library)`: once a frame, from `FrameWork`. Logs what
  the thread has logged; starts a run when the library's generation has
  changed. When a run has gone through every video, logs how many frames,
  how long it took, how many were up to date and how many failed.
- `IDX_GetStatus`: how many videos, done, failed, frames indexed, whether
  there is a model, and the name of the one under way. The status line shows
  it.
- `IDX_IsIndexed(indexer, library_gen, video_idx)`,
  `IDX_SoundIsIndexed(...)`: the search asks them which files to load.
- `IDX_Stop`: cancels and joins, at the end of the program.

## How a video is read

Every frame is decoded (a codec can't skip to the next second without
decoding what is between), and the first one at or after each whole second
is taken. A video that can't be opened, or that the model fails on, counts
as failed. One stopped part way (the library changed, or the program
closed) is not written, and is read again next time. A line is logged per
video indexed: how many frames, how many stretches of sound and how long it
took.

The sound is asked of the decoder at 48 kHz, after each frame up to that
frame's time and then to its end, so the decoder never holds much of the one
while giving the other. Its two channels are averaged into one. A full
stretch, or a last one of a second or more, goes through
`EMB_SoundFeatures` and `EMB_SoundVector`; one never louder than
`IDX_SOUND_SILENCE` is left out. A video with no sound gets a sound file with
nothing in it. A video whose frames are up to date but whose sound is not
(the sound model was added later) is decoded again for its sound alone.
