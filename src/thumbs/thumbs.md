# thumbs

Small pictures of moments in videos, as textures: what a hit's row shows of
its best frame.

## State

`THM_Thumbs` (thumbs.c), one, made in `_EntryPoint` and kept in
`App_State.thumbs`.

- `slots`: `THM_CAP` (64: bgz's renderer holds 128 textures in all) thumbnails, each free, wanted, being decoded,
  decoded, ready (has its texture) or failed. A slot is found by a key made
  of the video's path and the second.
- A thread of its own, which takes the wanted slot asked for last, opens the
  video with the decoder (`src/video`) at `THM_DIM` (192 px), seeks, decodes
  on to the second and copies the pixels into the slot.
- `mutex` over the slots; `wanted`, a condition the thread sleeps on.

## Functions

- `THM_Alloc`: starts the thread.
- `THM_Update`: once a frame, from `FrameWork`, before the UI is built.
  Makes a texture of each slot decoded since.
- `THM_Get(thumbs, path, time_s)`: the texture and its size, or zero while
  it is not there. Asking is what starts one being made, and what keeps it:
  when all the slots are taken, the one asked for longest ago is given up
  (never one asked for this frame, or one being decoded).
- `THM_Stop`: stops and joins the thread, at the end of the program.

## Not done yet

- Nothing is kept on disk: thumbnails are decoded again each run.
- A video is opened anew for each thumbnail, even several of the same video.

## Used by

- `frame.c`: `_FR_BuildScreen`, for each row on screen.
