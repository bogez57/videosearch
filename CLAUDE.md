# Video Search - notes for Claude

A native C unity-build Windows program, built like the slideshow
(`..\slideshow`): `bgz.dll` (raddebugger's layers in `src/bgz/rad/`, bgz's on
top in `src/bgz/bgz/`) and the app, `bin\videosearch.exe`, whose one unit is
`src/main.c`. It has a search bar; what is typed finds the clips that match
in the folders of videos the user gave it.

How the code works now: each layer's own `<folder>/<folder>.md` (e.g.
`src/search/search.md`) - what it's for, State, Functions, Used by. Current
facts only. A change to a layer updates its `.md` in the same change.

Where it is going: `Claude outputs/clip_search_plan.md`, the plan for finding
clips by what is in them (frames sampled with FFmpeg, a CLIP-style model run
through ONNX Runtime, vectors on disk) in five steps.

## How J works

- J and Claude both work on `main` of `bogez57/videosearch` on GitHub, no
  branches or pull requests. Claude pushes each step there when it is done;
  J pulls, builds and runs. Pull before starting, so neither overwrites the
  other.
- J builds and runs: `build` from the repo root (an MSVC prompt, clang-cl),
  then `bin\videosearch.exe [folder ...]`. Plan larger features first, then
  small, clearly explained steps.
- Only touch the files that are necessary. Don't abstract or pull things into
  functions unnecessarily.
- Comments only where the code can't explain itself - short, no narrating the
  change.
- Line endings: hand-written sources and `.bat` files are CRLF; generated meta
  files are LF. Preserve each file's endings.
- The program writes its log to `bin\local\logs\videosearch_log.txt`
  (`videosearch_log_previous.txt` for the run before). Read that rather than
  asking J to paste output.

## Rules of the code

- `src/bgz/` is a copy of the slideshow's `src/bgz`, and `src/ui`,
  `src/text`, `src/video` and `src/audio` of its layers of those names.
  Change them there first, then copy across, so the projects don't drift.
- `src/bgz/rad/` is an unedited copy of raddebugger - never edit it.
- The app calls bgz's PascalCase functions (`Core_WM_...`, `Core_D_...`,
  `FileOpen`, `PushStr8F`, ...), never raddebugger's snake_case ones, and
  includes no `rad/` header itself: `bgz_inc.h` does.
- A layer is a folder under `src/` with a `.h`, a `.c` and a `.md`; `main.c`
  includes every header, then every `.c`, in dependency order. Its functions
  share a prefix (`LIB_`, `SRCH_`, `FR_`); its own ones start with `_`.
- A library from outside (FFmpeg, ONNX Runtime) is known to one `.c` file
  only, behind a seam header, as `video_decoder.h` / `video_ffmpeg.c` are in
  the slideshow.
- Style: `.clang-format` (3 spaces, `Type* name`).

## Where we left off

- The skeleton: the window, the search bar, the library of folders (from the
  command line, dropped on the window, remembered in
  `bin\local\folders.txt`), and the stand-in search by file name.
- Step 1 of the plan, frames: the indexer (`src/indexer`) reads a frame a
  second from every video with the slideshow's decoder (`src/video`).
- Step 2, image vectors: ONNX Runtime (`src/third_party/onnxruntime`) runs
  CLIP ViT-B/32's image half (`src/embed`, the model in `data/models`, see
  its README) on each frame, and the vectors go to one file per video in
  `bin\local\index` (`src/index`).
- Step 3, real search: CLIP's tokenizer (`src/embed/embed_tokenizer.c`,
  merges in `data/clip`) and text half put the query into a vector;
  `src/search` loads the index files and ranks every frame by its dot
  product with it.
- Step 4, clips: runs of frames that score nearly as well as their best are
  one clip (`src/search`); each row has a thumbnail of its best frame
  (`src/thumbs`); a click plays the clip in the window with the slideshow's
  player and audio (`src/video`, `src/audio`, copied as they are).
- Folders in the app: the Folders panel (`frame.c`) lists the folders with
  Remove, and "Add a folder..." opens Windows' folder window (`src/picker`).
- `main.c` only lists includes: warnings are turned off in `build.bat`, not
  with pragmas around includes.
- Next: step 5, as wanted (the GPU through DirectML, new videos found while
  it runs, taking a folder out), or sound search (CLAP, whisper.cpp).
