# search

What the program asks of a search: the clips in the library that match what
was typed, best first. `search.h` is the seam: the screen calls `SRCH_Run`
and shows the `SRCH_Hit`s, and knows nothing of how they were found.

## State

`SRCH_Search` (search.c), one, made in `_EntryPoint` and kept in
`App_State.search`.

- Per library video, for the library's generation: its index file's frames
  (times and F16 vectors), loaded once the indexer says the file is up to
  date (`IDX_IsIndexed`), and its sound index file's stretches of sound
  the same way (`IDX_SoundIsIndexed`). A new library generation empties
  them all.
- `generation`: goes up when a file is loaded or a model finishes
  loading, so `_FR_UpdateResults` searches again.
- A table from every F16 to its F32, for the dot products.

`App_State` keeps the last results and what they were for (`results_query`,
`results_library_gen`, `results_search_gen`).

## Functions

- `SRCH_Alloc(index_folder, model)`
- `SRCH_Update(search, library, indexer)`: once a frame, after `IDX_Update`.
- `SRCH_Generation`
- `SRCH_Run(arena, search, library, query)`: up to `SRCH_MAX_HITS` hits,
  best first. A hit is a video (`video_idx`), a stretch of it in seconds
  (`start_s`, `end_s`; both 0 is the whole video), when its best frame is
  (`best_s`), a `score` (about 0 for no match to 1 for a good one) and
  whether it was found by its sound (`by_sound`).

## How it searches

The query goes through CLIP's text model (`EMB_TextVector`, on the main
thread: a few milliseconds), and every loaded frame scores the dot product
of its vector and the query's - their cosine, about 0.2 to 0.35 for CLIP.
Brute force over every frame: see the plan for why that is enough.

The sound is searched the same way (`_SRCH_Clips` does both): the query
through the sound model's text half (`EMB_SoundTextVector`), a dot product
with every stretch of sound. So that clips found by picture and by sound can
be in one list, a score is the model's own stretched over its band:
`SRCH_PICTURE_LOW` to `SRCH_PICTURE_HIGH` (0.18 to 0.33) becomes 0 to 1, and
`SRCH_SOUND_LOW` to `SRCH_SOUND_HIGH` likewise. The sound's band is a first
guess, to be set from real videos.

Then, per video, the frames become clips: the best frame not yet in a clip
starts one, which takes in its neighbours each side for as long as they
score within `SRCH_CLIP_MARGIN` (0.02) of it. Every frame ends up in one
clip, and a clip scores as its best frame. Neighbours must also follow
straight on in time (silence is left out of the sound index). A clip runs
from its first frame to the end of its last, padded by a second each side;
a clip of sound is not padded (`SRCH_SOUND_MARGIN` is its margin).

Until there are frames or sound loaded, or if no model could be loaded, it
matches file names instead (`by_name` in the results): a video is a hit
when a word of the query is in its name, and the hit is the whole video.

## Not done yet

- A score below which a frame is not a hit at all: for now the best 500
  are shown, however poor.

## Used by

- `frame.c`: `SRCH_Update` in `FrameWork`, `SRCH_Run` in `_FR_UpdateResults`.
