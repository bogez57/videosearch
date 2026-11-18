# embed

Pictures and words into vectors with a picture model's two halves:
`EMB_DIM` (512) numbers each, so the dot product of a frame's vector and a
query's is how well they match. `embed.h` is the seam: `embed_onnx.c` is the only
file that knows ONNX Runtime (`src/third_party/onnxruntime`: Microsoft's
DirectML build, 1.23.0, with `DirectML.dll` 1.15.4 beside it).

The model is Apple's MobileCLIP-B: of CLIP's kind, and it reads words with
CLIP's tokenizer, but it finds what is asked for more often (`bench/README.md`
has the numbers that chose it, and the views and the query's vector below).

The picture and sound halves run on the graphics card through DirectML when
`EMB_Open` is given `use_gpu` (it is unless the program is started with
`--cpu`), and on the processor if the card can't do it; the log says which.
The text halves are always on the processor. The vectors come out the same
either way.

Two things make the card quicker at it (`bench/speed.py` has the numbers,
on J's RTX 5060). The image half is run in half precision if that file is
there (`vision_model_fp16.onnx`): 1.8 ms a picture against 4.5, and its
vectors are within 0.9998 of the whole one's, so an index made with either
is searched the same. And ONNX Runtime is told the sizes of what is coming
before a model is loaded (`_EMB_Size`), which lets DirectML plan for them:
the image half always gets `EMB_IMAGE_BATCH` (16) pictures, and is nearly
twice as fast for it in half precision (1.8 ms a picture against 3.4); the
sound half always gets one stretch, 19 ms against 31.

The model is two files in `data\models\mobileclip_blt\`
(`data/models/README.md`):

- `vision_model.onnx`: input `pixel_values`, batch by 3 by 224 by 224, each
  colour 0 to 1; output `image_embeds`, batch by 512.
- `text_model.onnx`: input `input_ids` (and `attention_mask`, if an export
  has one), batch by 77; output `text_embeds`, batch by 512.

The tokenizer reads `data\clip\bpe_merges.txt`, and the words that could be
about anything are `data\clip\any_words.txt` (`data/clip/README.md`).

Sound is a second model beside it, CLAP (`clap-htsat-unfused`), which does
for a few seconds of sound what the picture model does for a picture. Its
vectors are 512 numbers too, but only ever compared with its own text
half's. It is optional: without its folder,
`data\models\clap-htsat-unfused\`, sound is not searched.

- `audio_model.onnx`: input `input_features`, 1 by 1 by 1001 by 64 (ten
  seconds as a log-mel spectrogram); output `audio_embeds`, 1 by 512.
- `text_model.onnx`: inputs `input_ids` and `attention_mask`; output
  `text_embeds`, 1 by 512.

Its tokenizer (RoBERTa's) reads `data\clap\vocab.json` and `merges.txt`,
and the words about sound are `data\clap\sound_words.txt`
(`data/clap/README.md`).

## State

- `EMB_Model` (embed_onnx.c): the tokenizer, ONNX Runtime's environment,
  and a session for each half with the names of its inputs and output. One,
  opened in `_EntryPoint` and kept in `App_State.model`; the indexer and the
  search are given it. `any_words` are the lines of `any_words.txt` and
  `any_words_mean` the average of the text half's vectors of them. `loaded`
  is set, with an atomic, once both halves are loaded and that average is
  made. The sound model's tokenizer, two sessions and `sound_loaded` are in
  it too, and what `EMB_LoadSound` makes of `sound_words` (the lines of
  `sound_words.txt`) and `any_words` with CLAP's text half:
  `sound_words_mean`, the average vector of "This is a sound of ..." for
  each of the words about sound; `sound_middle`, the average vector of
  every one of both lists as it is; and `sound_words_from_middle` and
  `any_words_from_middle`, each word's vector less `sound_middle` and made
  length one.
- embed_sound.c keeps the tables of its spectrogram (window, FFT, mel
  bands) in globals, made by the first call.

## Functions

- `EMB_ImageFromFrame(pixels, size, view, image_out)` (embed.c): a decoded
  RGBA frame into the model's input: the view of it (`EMB_View`: the whole
  frame pressed into the square, or a square as high as the frame from its
  left end, its middle or its right end) scaled to 224, each colour 0 to 1,
  laid out red plane, green plane, blue plane. The scaling is bilinear with
  the filter as wide as the scale, as Pillow's is: checked against Pillow on
  test pictures of nine sizes, the same to a 255th.
- `EMB_TokenizerFromMerges`, `EMB_Tokenize` (embed_tokenizer.c): CLIP's
  tokenizer in C, words into the numbers the text half reads. Checked
  against a port of OpenAI's `simple_tokenizer.py` on ten queries.
- `EMB_Open(folder, merges_path, any_words_path, use_gpu)`: quick - reads
  the merges and the words that could be about anything, starts ONNX
  Runtime. 0 if it can't (a file missing, an old `onnxruntime.dll`), and why
  is logged.
- `EMB_Load(model)`: loads both halves - the image half from
  `vision_model_fp16.onnx` if it is there and runs on the card, else from
  `vision_model.onnx` - then puts the words that could be about anything
  through the text half, eight at a time, for their average.
  On the indexer's thread. 0 if a file is missing or lacks its output, and
  why is logged.
- `EMB_IsLoaded`
- `EMB_ImageVectors(model, images, vectors_out)`: `EMB_IMAGE_BATCH` pictures
  through the image half, always that many (with fewer to run, the caller
  makes the rest zeros); the vectors made length one. Checks they are 16 by
  512.
- `EMB_QueryVector(model, text, vector_out)`: the vector a search for
  pictures is made with. The text as it is and after "a photo of ", "a video
  frame of ", "a scene from a movie showing " and "a picture of " goes
  through the text half in one run (`_EMB_TextVectors`: each tokenized and
  given whole, all 77 numbers, since this model reads the zeros after the
  end too); the five vectors are averaged and the average made length one;
  `any_words_mean` is taken from it. So a frame's dot product with it is
  what the frame scores for the words less what it scores for any words.
- `EMB_Close`, at the end of the program.
- `EMB_SoundFeatures(samples, count, features_out)` (embed_sound.c): one
  channel at 48 kHz into CLAP's input, as its `preprocessor_config.json`
  says: repeated to ten seconds, 1024-sample Hann windows 480 apart, an FFT
  of its own, 64 Slaney mel bands from 50 Hz to 14 kHz, in decibels. Checked
  against a numpy port of Hugging Face's `ClapFeatureExtractor`: the same to
  0.00001 dB. Where the sound is a repeat, a window that lies wholly in a
  later repeat is copied from the one a repeat before it (the same numbers).
- `EMB_SoundTokenizerFromVocab`, `EMB_SoundTokenize`
  (embed_sound_tokenizer.c): RoBERTa's tokenizer, with the byte-pair
  encoding of embed_tokenizer.c. Checked against a Python port of GPT-2's
  `encoder.py` on seventeen queries.
- `EMB_OpenSound(model, folder, vocab_path, merges_path, sound_words_path)`,
  `EMB_LoadSound`, `EMB_SoundIsLoaded`: as `EMB_Open`, `EMB_Load` and
  `EMB_IsLoaded`. No folder is logged as something to know, not an error.
  `EMB_LoadSound` puts some 400 short texts through CLAP's text half, one
  at a time (`_EMB_SoundTextVector`).
- `EMB_SoundVector(model, features, vector_out)`: one sound's vector, of
  length one.
- `EMB_SoundQueryVector(model, text, vector_out, about_sound_out)`: the
  vector a search for sound is made with, and how much the words are about
  sound at all. The vector is CLAP's text half's of "This is a sound of
  <text>." (the sentence CLAP's makers tested it with) less
  `sound_words_mean`: what a stretch of sound scores for the words less
  what it scores for any sound. `about_sound` is from the text as it is,
  less `sound_middle` and made length one: the average of its dot products
  with the five words about sound nearest to it after the very nearest
  (which may be the same words), less the same with the words that could be
  about anything (`_EMB_NearnessToWords`). Over 0 for most words about a
  sound, well under for most about a picture.

## Used by

- `indexer.c` (`EMB_Load`, `EMB_ImageVectors`, `EMB_LoadSound`,
  `EMB_SoundFeatures`, `EMB_SoundVector`), `search.c` (`EMB_QueryVector`,
  `EMB_SoundQueryVector`), `main.c` (`EMB_Open`, `EMB_OpenSound`,
  `EMB_Close`).
