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
- `picture_sum`: the sum of every loaded frame's vector, so the mean frame
  is at hand for each search.
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

Where in that range the scores are depends on the words more than on the
frames ("a man screaming" averaged 0.229 over the test library, "a city
skyline" 0.182), so the mean frame's vector is taken from the query's first:
a frame's score is then how far above the library's average for those words
it is. In a library whose frames nearly all match the words, that makes
them all score low.

The sound is searched the same way (`_SRCH_Clips` does both): the query
through the sound model's text half (`EMB_SoundTextVector`), a dot product
with every stretch of sound. The sound model gives some stretches a good
score for any words at all, so `sound_base` is taken from the query's vector
first: a stretch's score is then what it scores for the query less what it
scores for words that say nothing.

So that clips found by picture and by sound can be in one list, a score is
the model's own stretched over its band: `SRCH_PICTURE_LOW` to
`SRCH_PICTURE_HIGH` (0.03 to 0.10 above the average) becomes 0 to 1, and
`SRCH_SOUND_LOW` to `SRCH_SOUND_HIGH` (0.20 to 0.50) likewise. Both bands
are from 58 test searches on five film trailers. The best frame for words
in none of them was about 0.04 above the average, for words that were in
them 0.07 to 0.11. For the sound: sound that matched the words was left with
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

A clip that scores under `SRCH_KEEP` (half) of what the best found the same
way does is left out - by frames, by sound and by both each against their
own best, so a good clip of sound does not put out the best of the frames.

Until there are frames or sound loaded, or if no model could be loaded, it
matches file names instead (`by_name` in the results): a video is a hit
when a word of the query is in its name, and the hit is the whole video.

## Not done yet

- A score below which nothing is a hit at all: the best clip is always
  shown, however poor.

## Used by

- `frame.c`: `SRCH_Update` in `FrameWork`, `SRCH_Run` in `_FR_UpdateResults`.
