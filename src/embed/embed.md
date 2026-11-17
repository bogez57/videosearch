# embed

Pictures and words into vectors with CLIP's two halves: `EMB_DIM` (512)
numbers each, made length one, so the dot product of a frame's vector and a
query's is how well they match. `embed.h` is the seam: `embed_onnx.c` is the only
file that knows ONNX Runtime (`src/third_party/onnxruntime`: Microsoft's
DirectML build, 1.23.0, with `DirectML.dll` 1.15.4 beside it).

The picture and sound halves run on the graphics card through DirectML when
`EMB_Open` is given `use_gpu` (it is unless the program is started with
`--cpu`), and on the processor if the card can't do it; the log says which.
The text halves are always on the processor. The vectors come out the same
either way.

The model is two files in `data\models\clip-vit-base-patch32\`
(`data/models/README.md`):

- `vision_model.onnx`: input `pixel_values`, batch by 3 by 224 by 224;
  output `image_embeds`, batch by 512.
- `text_model.onnx`: input `input_ids` (and `attention_mask`, if the export
  has one), 1 by up to 77; output `text_embeds`, 1 by 512.

The tokenizer reads `data\clip\bpe_merges.txt` (`data/clip/README.md`).

Sound is a second model beside it, CLAP (`clap-htsat-unfused`), which does
for a few seconds of sound what CLIP does for a picture. Its vectors are 512
numbers too, but only ever compared with its own text half's. It is optional:
without its folder, `data\models\clap-htsat-unfused\`, sound is not searched.

- `audio_model.onnx`: input `input_features`, 1 by 1 by 1001 by 64 (ten
  seconds as a log-mel spectrogram); output `audio_embeds`, 1 by 512.
- `text_model.onnx`: inputs `input_ids` and `attention_mask`; output
  `text_embeds`, 1 by 512.

Its tokenizer (RoBERTa's) reads `data\clap\vocab.json` and `merges.txt`
(`data/clap/README.md`).

## State

- `EMB_Model` (embed_onnx.c): the tokenizer, ONNX Runtime's environment,
  and a session for each half with the names of its inputs and output. One,
  opened in `_EntryPoint` and kept in `App_State.model`; the indexer and the
  search are given it. `loaded` is set, with an atomic, once both halves
  are. The sound model's tokenizer, two sessions and `sound_loaded` are in
  it too.
- embed_sound.c keeps the tables of its spectrogram (window, FFT, mel
  bands) in globals, made by the first call.

## Functions

- `EMB_ImageFromFrame(pixels, size, image_out)` (embed.c): a decoded RGBA
  frame into the model's input: its middle square scaled to 224 (bilinear;
  CLIP's own is bicubic), each colour normalized with CLIP's mean and spread,
  laid out red plane, green plane, blue plane.
- `EMB_TokenizerFromMerges`, `EMB_Tokenize` (embed_tokenizer.c): CLIP's
  tokenizer in C, words into the numbers the text half reads. Checked
  against a port of OpenAI's `simple_tokenizer.py` on ten queries.
- `EMB_Open(folder, merges_path, use_gpu)`: quick - reads the merges, starts ONNX
  Runtime. 0 if it can't (no merges file, an old `onnxruntime.dll`), and why
  is logged.
- `EMB_Load(model)`: loads both halves, on the indexer's thread. 0 if a file
  is missing or lacks its output, and why is logged.
- `EMB_IsLoaded`
- `EMB_ImageVectors(model, images, count, vectors_out)`: a batch through the
  image half; the vectors made length one. Checks they are count by 512.
- `EMB_TextVector(model, text, vector_out)`: tokenizes and runs the text
  half on the tokens up to the end one (no padding: the text half reads in
  one direction, so what follows the end changes nothing).
- `EMB_Close`, at the end of the program.
- `EMB_SoundFeatures(samples, count, features_out)` (embed_sound.c): one
  channel at 48 kHz into CLAP's input, as its `preprocessor_config.json`
  says: repeated to ten seconds, 1024-sample Hann windows 480 apart, an FFT
  of its own, 64 Slaney mel bands from 50 Hz to 14 kHz, in decibels. Checked
  against a numpy port of Hugging Face's `ClapFeatureExtractor`: the same to
  0.00001 dB.
- `EMB_SoundTokenizerFromVocab`, `EMB_SoundTokenize`
  (embed_sound_tokenizer.c): RoBERTa's tokenizer, with the byte-pair
  encoding of embed_tokenizer.c. Checked against a Python port of GPT-2's
  `encoder.py` on seventeen queries.
- `EMB_OpenSound(model, folder, vocab_path, merges_path)`,
  `EMB_LoadSound`, `EMB_SoundIsLoaded`: as `EMB_Open`, `EMB_Load` and
  `EMB_IsLoaded`. No folder is logged as something to know, not an error.
- `EMB_SoundVector(model, features, vector_out)`,
  `EMB_SoundTextVector(model, text, vector_out)`.

## Used by

- `indexer.c` (`EMB_Load`, `EMB_ImageVectors`, `EMB_LoadSound`,
  `EMB_SoundFeatures`, `EMB_SoundVector`), `search.c` (`EMB_TextVector`,
  `EMB_SoundTextVector`), `main.c` (`EMB_Open`, `EMB_OpenSound`,
  `EMB_Close`).
