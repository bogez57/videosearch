# indexer

Goes through the library's videos on threads of its own: decodes each with
the decoder (`src/video`) at `IDX_FRAME_DIM` (512 px on the longer side),
takes a frame every `IDX_SAMPLE_SECONDS` (1 s), runs those through the image
model (`src/embed`) `IDX_BATCH` (4) at a time, and writes the vectors and
the frames' times to the video's index file (`src/index`). The model is
shown each frame four ways (`EMB_View`: the whole frame, and its left,
middle and right squares) and the frame's vector is the average of the four
vectors, made length one, so a batch is sixteen pictures. A video whose
index file is up to date is not read again.

A run has two threads, so that reading a video and running the models go on
at the same time: the reader decodes and makes the frames and the sound
into what the models read, and the run's own thread runs the models and
writes the files. The models are most of the work, and run one after the
other without waiting for the decoder.

With the sound model loaded, the video's sound goes the same way in the same
pass: `IDX_SOUND_SECONDS` (5 s) at a time, one stretch after another, through
the sound model and into the video's sound index file.

## State

`IDX_Indexer` (indexer.c), one, made in `_EntryPoint` and kept in
`App_State.indexer`, with the index folder (`bin\local\index`), the model
(`App_State.model`) and its name, which goes in the index files.

- A run: one pass over the videos as they were when it started. Their paths
  are copied onto the indexer's arena, so the library can change under it.
- `pieces`: the ring between the run's two threads, `_IDX_PIECES` (4) of
  them, under `pieces_mutex`, with `pieces_changed` to wait on. A piece is
  a video's Begin (its file's properties and which of its index files are
  up to date), a batch of its frames made ready, a stretch of its sound
  made ready, or its End (whether it could be read, and where the reader's
  time went), in the order the reader made them. Each piece has pictures
  and sound of its own; the reader fills `spare_images` and
  `spare_features` and hands them on by exchanging them for the piece's.
- `give_up`: the video the model failed on, set by the run's thread, so the
  reader stops reading it.
- `library_gen`: the library's generation the run is for. A different one
  stops the run (`cancel`, then the thread is joined) and starts another.
- `model`: its halves loaded (`EMB_Load`) by the first run's thread. None,
  or one that failed to load, means nothing is indexed (`no_model`).
- `done`, `failed`, `up_to_date`, `frames`, `frames_had`, `current`, and
  `indexed` and `sound_indexed` (one a video each, set when its file is up
  to date): written by the threads, read by the main thread with atomics.
  A video that was up to date is the reader's to count, one that is indexed
  the run's thread's.
- `sound`, `sound_had`: stretches of sound, as `frames` and `frames_had`
  are of frames. `video_bytes`: the sizes of all the videos, added up by the
  reader before it starts. `had_video_bytes` and `had_vectors` (the
  reader's), `made_video_bytes` and `made_vectors` (the run's thread's): the
  sizes of the videos gone through and how many vectors their index files
  have. From these `IDX_GetStatus` gives `bytes`, what the index files so
  far come to (1028 bytes a vector), and `bytes_in_all`, about what they
  will when every video is done: the videos to go taken to need as much
  for each of their bytes as those gone through did, which is a guess (a
  video's size says little of its length) that gets better as it goes.
- `logs`: what the threads logged (on `thread_log` and `reader_log`), under
  `log_mutex`, for `IDX_Update` to log on the main thread.

## Functions

- `IDX_Alloc(index_folder, model_name, sound_model_name, model, use_gpu)`:
  with `use_gpu` the videos are decoded on the graphics card.
- `IDX_Update(indexer, library)`: once a frame, from `FrameWork`. Logs what
  the thread has logged; starts a run when the library's generation has
  changed. When a run has gone through every video, logs how many frames,
  how long it took, how many were up to date and how many failed.
- `IDX_GetStatus`: how many videos, done, failed, frames indexed, whether
  there is a model, how long the run has taken (or took), and the name of
  the one under way. The status line shows
  it.
- `IDX_IsIndexed(indexer, library_gen, video_idx)`,
  `IDX_SoundIsIndexed(...)`: the search asks them which files to load.
- `IDX_Reindex(indexer, library)`: stops the run, deletes the index files
  of the library's videos (both kinds) and has the next `IDX_Update` start
  again. The Folders panel's "Re-index everything".
- `IDX_Stop`: cancels and joins, at the end of the program. The run's thread
  joins its reader.

## Where the time goes

On J's PC (RTX 5060, built with `build`), the twelve test videos, 1760
seconds of them: 38.7 s in all, from 107.1 s with one thread, the whole
picture model and the models' sizes left open. Of that, loading the models
is 5.7 s. The reader then takes 33 s (the decoder 22 s, making the frames
ready 7.5 s, the sound 3.1 s) and the models, at the same time, 27 s (the
picture model 20 s, the sound model 7 s), so the two are about even. Built
with `build o2` it is 35.3 s: making ready is a quarter of that (2.8 s), and
the decoder is what is waited for. One of the twelve, a Blu-ray's H.264 at
a high bit rate, takes three times as long to decode a second of as the
rest.

The index files come out as they did: the sound's vectors the same, the
pictures' within 0.9998 of what the whole model made (`bench/check_index.py
--same`), and the bench's 224 searches as before (`bench/app_check.py`).

## How a video is read

Every frame is decoded (a codec can't skip to the next second without
decoding what is between), on the graphics card if it can, but only the
first one at or after each whole second is made into pixels and taken
(`VID_DecoderNextFrameFrom`). A video that can't be opened, or that the model fails on, counts
as failed. One stopped part way (the library changed, or the program
closed) is not written, and is read again next time. A line is logged per
video indexed: how many frames, how many stretches of sound, how long it
took from its first piece to its last, how much of that was each model, and
what the reader spent meanwhile in the decoder and on making the frames and
the sound into what the models read. The reader is ahead of the models, so
its times are not part of the video's own.

The sound is asked of the decoder at 48 kHz, after each frame up to that
frame's time and then to its end, so the decoder never holds much of the one
while giving the other. Its two channels are averaged into one. A full
stretch, or a last one of a second or more, goes through
`EMB_SoundFeatures` (the reader) and `EMB_SoundVector` (the run's thread);
one never louder than `IDX_SOUND_SILENCE` is left out. A video with no sound gets a sound file with
nothing in it. A video whose frames are up to date but whose sound is not
(the sound model was added later) is decoded again for its sound alone.
