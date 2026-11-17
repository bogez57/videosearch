# Finding clips by what is in them

A plan for turning the stand-in search (file names, `src/search/search.c`)
into one that finds "a man screaming" or "a car driving down a highway" in
the videos themselves. Nothing here is built yet.

## The idea

A CLIP-style model has two halves that were trained together: an image
encoder and a text encoder. Each turns its input into a vector (512 numbers),
and a picture and a sentence that describe the same thing come out close
together. So:

- **Indexing**, once per video: decode it, take a frame every second or so,
  run each through the image encoder, keep the vectors on disk.
- **Searching**, on every query: run the words through the text encoder (a
  few milliseconds), compare that one vector with every frame vector (a dot
  product each), and the frames that score highest are the hits.
- **Clips**: a run of neighbouring frames that all score well is one hit,
  from the first of them to the last, padded by a second each side. Its score
  is its best frame's.

No training, no labels: anything a sentence can describe can be searched for.

## Sizes, to show brute force is enough

One hour of video at one frame a second is 3,600 vectors. At 512 numbers in
16-bit floats that is 3.7 MB. A library of 100 hours is 360,000 vectors,
370 MB, and comparing a query with all of them is about 184 million
multiply-adds: tens of milliseconds with AVX. So no vector database or
approximate index is needed until the library is far bigger than that, and
search stays quick enough to run as the user types.

## Which model, and how C calls it

- **Model: MobileCLIP-S2** (Apple, 2024) or **OpenCLIP ViT-B/32** (LAION).
  Both are small, fast on a CPU (tens of milliseconds a frame), good at
  everyday scenes, and use CLIP's own tokenizer. MobileCLIP-S2 is the better
  of the two for its size; ViT-B/32 is the most used and best documented.
  Both can be exported as two ONNX files, one per encoder.
- **Runtime: ONNX Runtime's C API.** It is plain C, comes as a prebuilt DLL
  with headers and an import library, and is brought in the way FFmpeg is in
  the slideshow: `src/third_party/onnxruntime/{include,lib,bin}`, the DLL
  copied next to the exe by `build.bat`, and one file of ours that includes
  its headers. It runs on the CPU, and on any GPU through DirectML later, with
  no change to our code. (The other way is a ggml port such as clip.cpp, like
  the `.gguf` models the editor has. It is C/C++ to compile in rather than a
  DLL, and has fewer models. ONNX Runtime first; the seam below lets it be
  swapped.)
- **Tokenizer**: CLIP's byte-pair encoding, written in C in our embed layer:
  lower-case, split into words, apply the merge list (a text file that comes
  with the model), look up ids, pad to 77. A few hundred lines, and the one
  part of the text side that is ours.
- The model files go in `data/models/` and are not in the repo
  (`.gitignore`).

## Layers

Each with a seam, as `video_decoder.h` is in the slideshow: the rest of the
program sees one header, and one `.c` knows the library behind it.

```
frame.c (the screen)
   |  search.h            SRCH_Run: hits for a query (exists, stand-in inside)
   v
search.c  ----------> index.h      vectors for every video, loaded from disk
   |                     ^
   |  embed.h            |  written by
   v                     |
embed.c / embed_onnx.c   indexer.c   a thread: decode, sample, encode, save
   (text and image          |
    vectors)                |  video_decoder.h
                            v
                         video_ffmpeg.c   from the slideshow, as it is
```

- **video**: the slideshow's `video_decoder.h` and `video_ffmpeg.c`, copied
  across with FFmpeg. Only the decoder is needed for indexing (open, next
  frame, seek, close). The player (`video.c`) comes later, for playing a hit.
- **embed** (`embed.h`): `EMB_Open(model folder)`, `EMB_ImageVectors(pixels,
  count)`, `EMB_TextVector(string)`. `embed_onnx.c` is the only file that
  knows ONNX Runtime; the tokenizer and the image's resize and normalize are
  in `embed.c`.
- **index** (`index.h`): one file per video in `local\index\`, named by a hash
  of the path. A header (the video's path, size and time written, which model
  made it, frames a second, how many vectors) then the vectors as F16. A
  video whose size or time has changed, or that a different model indexed, is
  indexed again. Loaded into one array at the start for searching.
- **indexer**: a worker thread (bgz's threads) that goes through the library
  for videos with no up-to-date index file, decodes each at a small size,
  takes a frame a second, encodes them in batches and writes the file. The
  status line shows how far it has got. Searching works on whatever is
  indexed so far.
- **search.c**: the stand-in replaced. `search.h` stays as it is: a hit
  already has `start_s` and `end_s`.

## Steps

Each one builds and runs, and is small enough to check by hand.

1. **Frames.** Copy the video decoder and FFmpeg from the slideshow. An
   indexer thread that decodes each video and counts the frames it would
   sample; the status line shows progress. No model yet.
2. **Image vectors.** Bring in ONNX Runtime and the image encoder. The
   indexer writes index files. A log line per video: how many frames, how
   long it took.
3. **Text vectors and real search.** The tokenizer and the text encoder.
   `search.c` compares the query's vector with the index: hits are single
   frames at first, shown with their time.
4. **Clips and seeing them.** Runs of frames become clips. A thumbnail of
   each hit's best frame in its row; clicking one plays the clip in the
   window with the slideshow's player.
5. **Later, as wanted**: the GPU through DirectML; finding new videos while
   the program runs (a `FileWatch` per folder); taking a folder out.

## Sound

CLIP sees only pictures. "A man screaming" will find a man with his mouth
open, but not a scream off screen. Two additions, after step 4 and only if
they are wanted:

- **CLAP** (a CLIP for sound and words, LAION) on one-second windows of the
  soundtrack, its scores added to the picture's for the same moment. Also an
  ONNX model, so it goes through the same embed layer.
- **Speech**: whisper.cpp to transcribe what is said, searched as words.

## Choices for J

- MobileCLIP-S2 or OpenCLIP ViT-B/32 (the plan works the same with either).
- ONNX Runtime, or a ggml port to compile in.
- Whether sound matters for the first version, or comes after step 4.
