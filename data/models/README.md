# Models

Downloaded, not kept in the repo (they are hundreds of MB). The program looks
for them here, beside `bin`.

## mobileclip_blt

Apple's MobileCLIP-B (the one trained longer, "LT"), as exported to ONNX by
Xenova: https://huggingface.co/Xenova/mobileclip_blt. Three files from it,
into `data\models\mobileclip_blt\`:

| In the repo on Hugging Face   | Size        | What it is                        |
| ----------------------------- | ----------- | --------------------------------- |
| `onnx/vision_model.onnx`      | 345,594,507 | the image half                    |
| `onnx/vision_model_fp16.onnx` | 172,963,522 | the image half, in half precision |
| `onnx/text_model.onnx`        | 253,902,061 | the words half                    |

The half-precision file is the one used on a graphics card, where it runs
more than twice as fast and finds the same things; the whole one is used on
the processor (`--cpu`), or if the other is not there. The whole one alone
will do everywhere, the half-precision one alone only on a graphics card.

The `.onnx` files go straight in the folder, not in an `onnx` folder under
it. Each file is at
`https://huggingface.co/Xenova/mobileclip_blt/resolve/main/<path>`.

The weights are under Apple's own licence for them (`LICENSE_weights_data`
in https://github.com/apple/ml-mobileclip), not MIT as CLIP's were: to be
read before the program is given to anyone else.

Without an image half the program still runs and searches by file name, but
indexes nothing, and its status line says so.

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
