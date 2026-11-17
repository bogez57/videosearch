# Models

Downloaded, not kept in the repo (they are hundreds of MB). The program looks
for them here, beside `bin`.

## clip-vit-base-patch32

OpenAI's CLIP ViT-B/32 (MIT licence), as exported to ONNX by Xenova:
https://huggingface.co/Xenova/clip-vit-base-patch32. Four files from it, into
`data\models\clip-vit-base-patch32\`:

| In the repo on Hugging Face | Size        | What it is                          |
| --------------------------- | ----------- | ----------------------------------- |
| `onnx/vision_model.onnx`    | 351,685,709 | the image half                      |
| `onnx/text_model.onnx`      | 254,058,553 | the words half (step 3)             |
| `preprocessor_config.json`  | 520         | how a picture is got ready for it   |
| `config.json`               | 4,524       | the model's sizes                   |

The `.onnx` files go straight in the folder, not in an `onnx` folder under
it. Each file is at
`https://huggingface.co/Xenova/clip-vit-base-patch32/resolve/main/<path>`.

Without `vision_model.onnx` the program still runs and searches by file
name, but indexes nothing, and its status line says so.

## clap-htsat-unfused (optional: search by sound)

LAION's CLAP (Apache 2.0 licence), as exported to ONNX by Xenova:
https://huggingface.co/Xenova/clap-htsat-unfused. Into
`data\models\clap-htsat-unfused\`:

| In the repo on Hugging Face | Size        | What it is                        |
| --------------------------- | ----------- | --------------------------------- |
| `onnx/audio_model.onnx`     | 117,528,416 | the sound half                    |
| `onnx/text_model.onnx`      | 501,513,769 | the words half                    |
| `preprocessor_config.json`  |             | how sound is got ready for it     |
| `config.json`               |             | the model's sizes                 |

As above, the `.onnx` files go straight in the folder. Without the folder
the program searches by picture only, and its log says so.
