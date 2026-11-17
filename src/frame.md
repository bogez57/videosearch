# frame

One frame's work: this frame's input, searching again when needed, and the
screen - the search bar, the status line, the list of hits or the clip being
played, and the foot - built with the UI layer and drawn. `frame.h` has what `main.c` calls.

## State

All in `App_State` (app_state.h): the query (`query`, `query_size`), the
results and what they were found for (`results_arena`, `results_query`,
`results_library_gen`, `results`, `results_scroll`), the clip being played
(`play_video`, `play_hit`, `play_name`), the notice, the log, and the UI
layer's state.

## Functions

- `FrameWork`: the frame. `_FR_Input` (typing, Backspace, Esc, a folder
  dropped, the window closed), `_FR_UpdateResults` (calls `SRCH_Run` when the
  query, the library's generation or the search's changed, and logs each
  search by the frames with its best three hits), then the UI build
  (`_FR_BuildScreen`) and its drawing (`_FR_DrawUITree`).
- The rows: each hit with a thumbnail of its best frame (`THM_Get`), its
  name, its stretch and score, its folder. A click plays it.
- Playing: `_FR_PlayHit` opens the hit's video with the player (`src/video`)
  in the list's place, from the clip's start; `_FR_UpdatePlaying` stops it
  at the clip's end, once; `_FR_StopPlaying`. While a clip is open the keys
  are the player's: Space (or a click) pauses and plays, Left and Right go 5
  seconds, Esc goes back to the hits.
- `FR_AddFolder`, `FR_LoadFolders`: the library's folders, kept a line each
  in `bin\local\folders.txt`.
- The log: `FR_OpenLogFile`, `FR_WriteLogs`, to
  `bin\local\logs\videosearch_log.txt` and the console.
- `FR_Notice`, `FR_CurrentNotice`, `FR_WholePath`, `FR_LocalFolder`,
  `FR_UIScale`, `FR_UIFontSize`.
- `_FR_UIEventsFromEvents`, `_FR_BeginUI`, `_FR_DrawUITree`: the UI layer's
  frame, as the slideshow's frame.c has them, with this program's colours.

## Not done yet

- The search bar has no cursor to move, no selection and no paste: typing
  adds at the end, Backspace takes off the last character.
- The player has no bar to seek with and no volume.
