# library

The videos to search: the folders the user gave, and every video file in them
and in the folders under them, each once. Only names: nothing here decodes a
video.

## State

- `LIB_Library` (library.h), one, made in `_EntryPoint` (main.c) and kept in
  `App_State.library`.
  - `folders`, on `folders_arena`: the folders given, in order.
  - `videos`, `video_count`, on `videos_arena`: what the last `LIB_Rescan`
    found. Each `LIB_Video` has its whole path, its name and its folder (the
    last two are pieces of the path).
  - `left_out`: how many more were found and left out, as the same as one
    of `videos`.
  - `generation`: goes up by one with each rescan, so what was worked out from
    the videos knows it is out of date.

## Functions

- `LIB_Alloc`
- `LIB_AddFolder(library, folder)`: adds a whole path that is a folder and is
  not there already, then rescans. 0 if it was not added.
- `LIB_RemoveFolder(library, folder)`: takes it out and rescans. 0 if it was
  not there. Its videos' index files stay on disk, so adding it again reads
  nothing anew.
- `LIB_Rescan`: walks every folder again, up to 16 deep, skipping hidden
  files. A video is a file whose extension `LIB_NameIsVideo` knows (the same
  list as `.gitignore`). One found a second time is left out: the same path
  (a folder given that is under another given), or a copy somewhere else,
  under any name - a file of the same size whose start, middle and end, 64 KB
  of each, are the same (`_LIB_Sample`, read only for files whose sizes are
  the same). The first found stays, in the order of the folders; if its
  folder is taken out, the copy is a video again, and is indexed as a new
  one, since an index file goes by the path.

## Used by

- `frame.c`: `FR_AddFolder` (the command line, a folder dropped on the
  window, the Folders panel), `FR_LoadFolders` (the folders from last time)
  and the Folders panel's Remove.
- `search.c`: searches `videos`.

## Not done yet

- Finding new videos while the program runs (a `FileWatch` on each folder).
