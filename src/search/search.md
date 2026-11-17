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
- `sound_base`: the mean of the sound model's vectors for ten texts that
  say nothing about a sound (`_srch_sound_base_texts`), worked out when the
  sound model has loaded.
- A table from every F16 to its F32, for the dot products.

`App_State` keeps the last results and what they were for (`results_query`,
`results_library_gen`, `results_search_gen`).

## Functions

- `SRCH_Alloc(index_folder, model)`
- `SRCH_Update(search, library, indexer)`: once a frame, after `IDX_Update`.
- `SRCH_Generation`
- `SRCH_Forget(search)`: lets go of what is loaded, to be loaded again as
  the indexer finishes each video. Called with `IDX_Reindex`.
- `SRCH_Run(arena, search, library, query)`: up to `SRCH_MAX_HITS` hits,
  best first. A hit is a video (`video_idx`), a stretch of it in seconds
  (`start_s`, `end_s`; both 0 is the whole video), when its best frame is
  (`best_s`), a `score` (about 0 for no match to 1 for a good one) and
  whether it was found by its sound (`by_sound`) or by its frames and its
  sound both (`by_both`).

## How it searches

The query goes through CLIP's text model (`EMB_TextVector`, on the main
thread: a few milliseconds), and every loaded frame scores the dot product
of its vector and the query's - their cosine, about 0.2 to 0.35 for CLIP.
Brute force over every frame: see the plan for why that is enough.

The sound is searched the same way (`_SRCH_Clips` does both): the query
through the sound model's text half (`EMB_SoundTextVector`), a dot product
with every stretch of sound. The sound model gives some stretches a good
score for any words at all, so `sound_base` is taken from the query's vector
first: a stretch's score is then what it scores for the query less what it
scores for words that say nothing.

So that clips found by picture and by sound can be in one list, a score is
the model's own stretched over its band: `SRCH_PICTURE_LOW` to
`SRCH_PICTURE_HIGH` (0.18 to 0.33) becomes 0 to 1, and `SRCH_SOUND_LOW` to
`SRCH_SOUND_HIGH` (0.20 to 0.50) likewise. The sound's band is from 58 test
searches on five film trailers: sound that matched the words was left with
0.3 to 0.5 ("music", "gunfire and explosions", "people cheering"), sound
under words about something seen mostly with less than 0.3, but up to 0.4
("a soldier in a trench", "fire", "a monkey"). The sound model's scores
don't tell the two kinds of words apart, so the band can only put such rows
below good picture rows, not leave them out.

Then, per video, the frames become clips: the best frame not yet in a clip
starts one, which takes in its neighbours each side for as long as they
score within `SRCH_CLIP_MARGIN` (0.02) of it. Every frame ends up in one
clip, and a clip scores as its best frame. Neighbours must also follow
straight on in time (silence is left out of the sound index). A clip runs
from its first frame to the end of its last, padded by a second each side;
a clip of sound is not padded (`SRCH_SOUND_MARGIN` is its margin).

Last, in `SRCH_Run`, a clip of frames and a clip of sound of the same video
that overlap in time and both score `SRCH_BOTH_FLOOR` (0.35) or more become
one hit (`by_both`): the best clips choose first, each the best of the other
kind it overlaps. It runs from the start of the first to the end of the
last, its best moment is the frames', and it scores as the better of the
two plus `SRCH_BOTH_SHARE` (a quarter) of the other.

Until there are frames or sound loaded, or if no model could be loaded, it
matches file names instead (`by_name` in the results): a video is a hit
when a word of the query is in its name, and the hit is the whole video.

## Not done yet

- A score below which a frame is not a hit at all: for now the best 500
  are shown, however poor.

## Used by

- `frame.c`: `SRCH_Update` in `FrameWork`, `SRCH_Run` in `_FR_UpdateResults`.
