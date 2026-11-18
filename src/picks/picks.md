# picks

The clips the user has picked out of the searches' hits, in the order they
are to be cut together, and the timeline of them an editing program reads.

## State

- `PCK_Picks` (picks.h), one, made in `_EntryPoint` (main.c) and kept in
  `App_State.picks`: up to `PCK_CAP` picks, each a video's path, what was
  searched for when it was picked, and its start, end and best moment in
  seconds (start and end both 0: the whole video). Their strings are on
  its arena, which only `PCK_Clear` empties.
- `bin\local\picks.txt`: a pick a line - start, end and best moment in
  thousandths of a second, the path and the query, with tabs between -
  written again on every change and read by `PCK_Alloc`.

## Functions

- `PCK_Alloc(file_path)`, `PCK_Find`, `PCK_Add` (0 if it is there already or
  there is no room), `PCK_Remove`, `PCK_Clear`, `PCK_Move` (one place up or
  down), `PCK_SetRange`, `PCK_Seconds`.
- `PCK_Export(picks, file_path)`: the picks as one timeline, each a cut of
  its video, one after another. The file is FCPXML 1.9, which DaVinci
  Resolve (17.4 and later) and Final Cut import:
  - a `format` for each frame rate and size the videos have, an `asset`
    for each video (its `media-rep` the file itself, as a `file:` URL), and
    one `sequence` whose `spine` is an `asset-clip` a pick;
  - what each video is comes from the file (`VID_DecoderMediaInfo`): its
    frame rate as the nearest of the ones editing programs have (23.976 is
    24000 over 1001), its length, its sound, and its first frame's
    timecode, which is where the asset starts and what a clip's `start` is
    counted from. A file with none starts at 0;
  - every time is a whole number of frames written as a fraction of
    seconds (`_PCK_Time`): a clip's `start` in its video's frames, its
    `offset` and `duration` in the timeline's;
  - the timeline has the frame rate most of the picks' videos have, and
    is as wide as the widest of them and 16 by 9 (higher if one of them
    is). A clip from a video of another frame rate is as long on it in seconds
    as it is in its video; how it is fitted is the editing program's;
  - a pick whose video can't be read is left out, and counted.

## Used by

- `frame.c`: the Pick button on a hit's row, P in the player, the Picks
  panel, and "Send to Resolve...".

## Not done yet

- The timeline has not been imported into every version of Resolve: what
  J's makes of it is what has been tried.
- No sound-only or picture-only clips, no gaps, no second track.
