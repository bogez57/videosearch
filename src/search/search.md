# search

What the program asks of a search: the clips in the library that match what
was typed, best first. `search.h` is the seam: the screen calls `SRCH_Run`
and shows the `SRCH_Hit`s, and knows nothing of how they were found.

## State

None of its own. `SRCH_Run` reads the library and pushes its results on the
arena it is given; `App_State` keeps the last results and the query they were
for (`results_arena`, `results_query`, `results`).

## Functions

- `SRCH_Run(arena, library, query)`: up to `SRCH_MAX_HITS` hits, best first.
  A hit is a video (`video_idx` in `library->videos`), a stretch of it in
  seconds (`start_s`, `end_s`; both 0 is the whole video) and a `score` that
  only means something next to the other hits of the same search.

## What it does now

A stand-in, so that the rest of the program can be built against the seam:
a video is a hit when any word of the query is in its file name, scored by
how many of the words are. Every hit is the whole video.

## What it is to become

The search by what is in the frames: `Claude outputs/clip_search_plan.md`.
The query is turned into a vector by a text encoder, compared with the
vectors of frames sampled from each video, and runs of close frames become
the hits' stretches. `search.h` stays as it is.

## Used by

- `frame.c`: `_FR_UpdateResults`, whenever the query or the library changes.
