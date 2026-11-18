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
- `SRCH_Forget(search)`: lets go of what is loaded, to be loaded again as
  the indexer finishes each video. Called with `IDX_Reindex`.
- `SRCH_Run(arena, search, library, query)`: up to `SRCH_MAX_HITS` hits,
  best first. A hit is a video (`video_idx`), a stretch of it in seconds
  (`start_s`, `end_s`; both 0 is the whole video), when its best frame is
  (`best_s`), a `score` (over 0, and about 1 for a good match) and
  whether it was found by its sound (`by_sound`) or by its frames and its
  sound both (`by_both`). The results also say how much the query was
  about sound (`about_sound`), which `frame.c` puts in the log's line for
  each search.

## How it searches

The query is made into a vector by `EMB_QueryVector` (on the main thread:
the text half reads it five ways at once, some tens of milliseconds), and
every loaded frame scores the dot product of its vector and the query's.
Brute force over every frame: see the plan for why that is enough.

That vector already has taken from it what a frame would score for any
words (embed.md), so a frame's score is around 0 for words that have
nothing to do with it, whatever the words and whatever the frame. Before,
where the scores fell depended on the words more than on the frames, and
on the library's other frames.

The sound is searched the same way (`_SRCH_Clips` does both), with
`EMB_SoundQueryVector`'s vector and every stretch of sound - but only if
the query is about sound: `about_sound` at least `SRCH_SOUND_WORDS`
(-0.07). The sound model was only ever shown words about sound, and gives
some stretches as good a score for "a hockey game" or "a dark room" as for
a sound that is there; a picture is the frames' to find. On the bench the
line lets through 29 of 32 searches for a sound, 2 of 30 for something only
seen, and a quarter of the 224 searches for pictures, most of them of what
is heard as well ("a man screaming", "a helicopter", "an explosion").

So that clips found by picture and by sound can be in one list, a score is
the model's own stretched over its band: `SRCH_PICTURE_LOW` to
`SRCH_PICTURE_HIGH` (0.122 to 0.192) becomes 0 to 1, and `SRCH_SOUND_LOW`
to `SRCH_SOUND_HIGH` (0.28 to 0.50) likewise. The numbers are from the test
bench (`bench/README.md`): twelve videos, every frame labelled, searched 224
ways. The picture's high end is what the best right frame scores, the
middle one of the searches. Its low end is a choice: at 0.122, 3 in 100
searches with something to find list nothing, and 4 in 10 of those with
nothing to find list a row of what is not there, with a low match; at
0.158 those are 12 in 100 and 1 in 20. The sound's low end is what three in
four searches for a sound in no video stay under; a sound mixed into a
video's own as loud as it is scores 0.32, the middle one, and at three
times as loud 0.45.

Then, per video, the frames become clips: the best frame not yet in a clip
starts one, which takes in its neighbours each side for as long as they
score within `SRCH_CLIP_MARGIN` (0.3, of a hit's score) of it. Every frame
that scores over 0 ends up in one clip, and a clip scores as its best
frame; a clip whose best frame scores 0 or under is not a hit. Neighbours
must also follow straight on in time (silence is left out of the sound
index). A clip runs from its first frame to the end of its last, padded by
a second each side; a clip of sound is not padded (`SRCH_SOUND_MARGIN` is
its margin).

Last, in `SRCH_Run`, a clip of frames and a clip of sound of the same video
that overlap in time and both score `SRCH_BOTH_FLOOR` (0.35) or more become
one hit (`by_both`): the best clips choose first, each the best of the other
kind it overlaps. It runs from the start of the first to the end of the
last, its best moment is the frames', and it scores as the better of the
two plus `SRCH_BOTH_SHARE` (a quarter) of the other.

A clip that scores under `SRCH_KEEP` (three tenths) of what the best found
the same way does is left out - by frames, by sound and by both each against
their own best, so a good clip of sound does not put out the best of the
frames. On the bench that is 12 rows a search, 64 in 100 of them right, and
63 in 100 of the stretches that match have a row; at half of the best it is
7 rows, 71 right, and 53 of the stretches.

Until there are frames or sound loaded, or if no model could be loaded, it
matches file names instead (`by_name` in the results): a video is a hit
when a word of the query is in its name, and the hit is the whole video.

## Not done yet

- One low end for every query. Plain words score lower for their right
  frames than longer ones do ("a woman" 0.14 at best, "soldiers in a trench"
  0.25), and some words in no video score as high ("a telescope" 0.16), so
  no one line parts them.
- Words the picture model is weak at on the test videos: "a cat" (small
  and in the dark), "a monster", "an oil lamp", "a person climbing".
- "a crowd cheering" is not searched for in the sound: `about_sound` puts
  it with the words about pictures, next to "a crowd of people". "A man
  shouting" and "a person sneezing" are just under the line too.
- The query's vectors are made on the main thread on every key: the text
  halves read it seven times (five for the pictures, two for the sound),
  and a search of twelve videos is 48 ms on J's PC.

## Used by

- `frame.c`: `SRCH_Update` in `FrameWork`, `SRCH_Run` in `_FR_UpdateResults`.
