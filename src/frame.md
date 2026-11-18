# frame

One frame's work: this frame's input, searching again when needed, and the
screen - the search bar, the status line, the list of hits or the clip being
played, and the foot - built with the UI layer and drawn. `frame.h` has what `main.c` calls.

## State

All in `App_State` (app_state.h): the query (`query`, `query_size`), the
results and what they were found for (`results_arena`, `results_query`,
`results_library_gen`, `results`, `results_scroll`), the clip being played
(`play_video`, `play_hit`, `play_name`, `play_path`, `play_to_end`), the notice, the log, and the UI
layer's state.

## Functions

- `FrameWork`: the frame. `_FR_Input` (typing, Backspace, Esc, a folder
  dropped, the window closed), `_FR_UpdateResults` (calls `SRCH_Run` when the
  query, the library's generation or the search's changed, and logs each
  search by the frames with its best three hits), then the UI build
  (`_FR_BuildScreen`) and its drawing (`_FR_DrawUITree`).
- The rows: each hit with a thumbnail of its best frame (`THM_Get`), its
  name, its stretch, how well it matched as 1 to 100 (the hit's score
  times 100; "sound match" when it was found by its sound, "picture and
  sound match" when by both), its folder. A click plays it.
- Playing: `_FR_PlayHit` opens the hit's video with the player (`src/video`)
  in the list's place, from the clip's start; `_FR_UpdatePlaying` stops it
  at the clip's end, once (`play_to_end`); `_FR_StopPlaying`. While a clip
  is open the keys are the player's: Space (or a click) pauses and plays,
  Left and Right go 5 seconds, Esc goes back to the hits. Under the picture
  is the whole video as a line (`###timeline`, its pieces floating boxes at
  their places): the clip played on it, the search's other clips in the
  same video, and where it is now. A press or a drag on it seeks there.
- The Folders panel, in the list's place when `folders_open` is set (the
  Folders button beside the search bar) or there are no folders: each
  folder with how many videos it has and a Remove button, and "Add a
  folder...", which asks with Windows' folder window (`PICK_Folder`), and
  "Re-index everything" (`IDX_Reindex`, `SRCH_Forget`). Esc,
  the button again, or typing closes it. `_FR_Button` is its buttons.
- `FR_AddFolder`, `FR_RemoveFolder`, `FR_LoadFolders`: the library's
  folders, kept a line each in `bin\local\folders.txt`.
- The log: `FR_OpenLogFile`, `FR_WriteLogs`, to
  `bin\local\logs\videosearch_log.txt` and the console.
- `FR_Notice`, `FR_CurrentNotice`, `FR_WholePath`, `FR_LocalFolder`,
  `FR_UIScale`, `FR_UIFontSize`.
- `_FR_UIEventsFromEvents`, `_FR_BeginUI`, `_FR_DrawUITree`: the UI layer's
  frame, as the slideshow's frame.c has them, with this program's colours.

## Not done yet

- The search bar shows a blinking caret after the text while it is where
  the user is (`search_focused`: set by a click on it or by typing, and from
  the start; cleared by playing a hit or the Folders button). Typing goes to
  it either way.
- The search bar has no cursor to move, no selection and no paste: typing
  adds at the end, Backspace takes off the last character.
- The player has no volume.
