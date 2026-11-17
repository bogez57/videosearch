# embed

Pictures into vectors with CLIP's image half: `EMB_DIM` (512) numbers each,
made length one, so the dot product of a frame's vector and a query's (step
3) is how well they match. `embed.h` is the seam: `embed_onnx.c` is the only
file that knows ONNX Runtime (`src/third_party/onnxruntime`, 1.23.2).

The model is `data\models\clip-vit-base-patch32\vision_model.onnx`
(`data/models/README.md`): input `pixel_values`, batch by 3 by 224 by 224;
output `image_embeds`, batch by 512.

## State

- `EMB_Model` (embed_onnx.c): ONNX Runtime's environment and the image
  model's session, and the names of its input and output. One, opened by the
  indexer's thread and kept in `IDX_Indexer.model`.

## Functions

- `EMB_ImageFromFrame(pixels, size, image_out)` (embed.c): a decoded RGBA
  frame into the model's input: its middle square scaled to 224 (bilinear;
  CLIP's own is bicubic), each colour normalized with CLIP's mean and spread,
  laid out red plane, green plane, blue plane.
- `EMB_Open(arena, folder)`: loads `vision_model.onnx` from the folder. 0 if
  it can't (no file, an old `onnxruntime.dll`, no `image_embeds` output), and
  why is logged.
- `EMB_ImageVectors(model, images, count, vectors_out)`: a batch through the
  model; the vectors made length one. Checks they are count by 512.
- `EMB_Close`

## Not used yet

- `embed_tokenizer.c`: CLIP's tokenizer in C, words into the 77 numbers the
  text half reads. Checked against a port of OpenAI's `simple_tokenizer.py`;
  not compiled in until step 3 brings the text half.

## Used by

- `indexer.c`.
