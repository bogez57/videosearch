# picker

Asks the user for a folder, or for where to save a file, with Windows' own
windows for it. `picker.h` is
the seam; `picker_win32.c` is the only file that knows Windows is there
(the COM file dialogs: `IFileOpenDialog` told to pick folders, and
`IFileSaveDialog`).

bgz has `ShPickFile` for a file and nothing for a folder. If bgz gets one
(in the slideshow first), this layer goes.

## Functions

- `PICK_Folder(arena, title)`: the folder picked, whole, or nothing when
  cancelled. Does not return until the user has answered. A repaint Windows
  asks for meanwhile does nothing (`g_frame_running`, main.c), so the window
  behind stays as it was.

- `PICK_SaveFile(arena, title, file_name, type_name, extension)`: where
  the user wants a file written, whole, or nothing when cancelled. The
  same waiting.

## Used by

- `frame.c`: the Folders panel's "Add a folder", and the picks' "Send to
  Resolve".
