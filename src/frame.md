# frame

One frame's work: this frame's input, searching again when needed, and the
screen - the search bar, the status line, the list of hits and the foot -
built with the UI layer and drawn. `frame.h` has what `main.c` calls.

## State

All in `App_State` (app_state.h): the query (`query`, `query_size`), the
results and what they were found for (`results_arena`, `results_query`,
`results_library_gen`, `results`, `results_scroll`), the notice, the log, and
the UI layer's state.

## Functions

- `FrameWork`: the frame. `_FR_Input` (typing, Backspace, Esc, a folder
  dropped, the window closed), `_FR_UpdateResults` (calls `SRCH_Run` when the
  query or the library's generation changed), then the UI build
  (`_FR_BuildScreen`) and its drawing (`_FR_DrawUITree`).
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
- Clicking a hit does nothing (step 4 of the plan plays it).
