# library

The videos to search: the folders the user gave, and every video file in them
and in the folders under them. Only names: nothing here opens or decodes a
video.

## State

- `LIB_Library` (library.h), one, made in `_EntryPoint` (main.c) and kept in
  `App_State.library`.
  - `folders`, on `folders_arena`: the folders given, in order. It only grows.
  - `videos`, `video_count`, on `videos_arena`: what the last `LIB_Rescan`
    found. Each `LIB_Video` has its whole path, its name and its folder (the
    last two are pieces of the path).
  - `generation`: goes up by one with each rescan, so what was worked out from
    the videos knows it is out of date.

## Functions

- `LIB_Alloc`
- `LIB_AddFolder(library, folder)`: adds a whole path that is a folder and is
  not there already, then rescans. 0 if it was not added.
- `LIB_Rescan`: walks every folder again, up to 16 deep, skipping hidden
  files. A video is a file whose extension `LIB_NameIsVideo` knows (the same
  list as `.gitignore`).

## Used by

- `frame.c`: `FR_AddFolder` (the command line, a folder dropped on the
  window) and `FR_LoadFolders` (the folders from last time).
- `search.c`: searches `videos`.

## Not done yet

- Taking a folder out.
- Finding new videos while the program runs (a `FileWatch` on each folder).
