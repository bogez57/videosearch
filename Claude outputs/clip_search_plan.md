# Finding clips by what is in them

A plan for turning the stand-in search (file names, `src/search/search.c`)
into one that finds "a man screaming" or "a car driving down a highway" in
the videos themselves. Steps 1 to 4 are built; the model chosen is OpenAI's
CLIP ViT-B/32, exported to ONNX (`data/models/README.md`).

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

- **CLAP** (a CLIP for sound and words, LAION). Done: see below.
- **Speech**: whisper.cpp to transcribe what is said, searched as words.

## Choices for J

- MobileCLIP-S2 or OpenCLIP ViT-B/32 (the plan works the same with either).
- ONNX Runtime, or a ggml port to compile in.
- Whether sound matters for the first version, or comes after step 4.

## Sound search, as built

- The model: `clap-htsat-unfused` (Xenova's ONNX export), in
  `data\models\clap-htsat-unfused\`, optional. It goes through the embed
  layer beside CLIP (`embed_sound.c` for its spectrogram,
  `embed_sound_tokenizer.c` for its tokenizer, the sessions in
  `embed_onnx.c`).
- Indexing: in the same pass as the frames, the soundtrack five seconds at a
  time, one stretch after another, each into a vector in a second index file
  per video (`.vsa`). Silence is left out.
- Searching: the query through CLAP's text half as well as CLIP's; clips of
  sound are found as clips of frames are, and both go in one list. Each
  model's score is stretched over its own band so they compare; a row found
  by sound says "sound match".
- Still to do: set the sound's band (`SRCH_SOUND_LOW`, `SRCH_SOUND_HIGH`)
  from real videos; maybe one hit where a clip matches by both.

## The GPU, as built

The picture and sound models run on the graphics card through DirectML
(ONNX Runtime's DirectML build and `DirectML.dll`, in
`src/third_party/onnxruntime`), falling back to the processor; `--cpu`
forces that. Measured on J's PC (RTX 5060) over 13 minutes of video, 809
frames and 153 stretches of sound: 36.0 s on the card, 42.6 s on the
processor, the same vectors. The card was at about 12%: the models are not
what takes the time, decoding every frame on the processor is. What would
help next is in the decoder (`video_ffmpeg.c`, the slideshow's): decoding on
the card too (D3D11VA), or decoding only keyframes.

Then the decoder (with J's say-so, as it is the slideshow's): the indexer
decodes on the card through FFmpeg's Direct3D 11 decoder, and only the
frame taken each second is brought back and made into pixels. The same 13
minutes: 27.4 s on the card, 34.5 s with `--cpu` (from 36.0 and 42.6). Of
the 25 s in the videos themselves on the card: the picture model 2.8 s, the
sound model with its spectrogram 9.0 s, the rest (decoding picture and
sound) about 13 s.
