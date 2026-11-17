# picker

Asks the user for a folder with Windows' own window for it. `picker.h` is
the seam; `picker_win32.c` is the only file that knows Windows is there
(the COM file dialog, `IFileOpenDialog`, told to pick folders).

bgz has `ShPickFile` for a file and nothing for a folder. If bgz gets one
(in the slideshow first), this layer goes.

## Functions

- `PICK_Folder(arena, title)`: the folder picked, whole, or nothing when
  cancelled. Does not return until the user has answered. A repaint Windows
  asks for meanwhile does nothing (`g_frame_running`, main.c), so the window
  behind stays as it was.

## Used by

- `frame.c`: the Folders panel's "Add a folder".
