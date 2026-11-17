# bench

Test tools, not part of the program: Python scripts that measure how well ways of searching find
what is really in a set of videos, so the program's models and ranking are chosen from numbers.
The program (`src`) does not use any of this.

They work in a folder beside the repo, `..\videosearch_bench` (or `VS_BENCH`), which holds what is
downloaded and made: a Python environment (`.venv`), candidate models from Hugging Face (`models`),
a frame a second of each test video (`frames`), their sound (`audio`), their vectors (`emb`), and
the public test sets (`esc50`, `flickr1k`).

- `sheets.py`: contact sheets of the frames, for labeling them by eye.
- `tags.txt`: the tags a label can use. `labels.tsv`: each frame's tags and a caption, given by
  eye as `LABELING.md` says.
- `embed_images.py MODEL...`: every frame through a model, five ways (the whole frame squashed,
  padded, and three squares cut from it).
- `queries.txt`: searches, each with the tags a frame must have to match, or NEAR / NONE for
  something that is in none of the videos.
- `bank.txt`: words that could be about anything, for taking out what a frame scores for any words.
- `eval.py CONFIG...`: scores each way of searching against the labels (the top of the file says
  how to write a config and what the numbers mean).
- `flickr.py MODEL...`: 1000 captioned photographs (Flickr30k's test split): whether each model
  finds a caption's photograph as often as its makers say, and pictures of other things for
  `eval.py` to set a search's scores against.
- `sound.py`: the sound model against sound whose content is known: ESC-50 (2000 labeled clips),
  ESC-50 sounds mixed into the test videos' own sound, and the videos' own sound where the labels
  say what should be heard. `bank_sound.txt`: words about sounds of every kind, as `bank.txt` is
  for pictures.
- `check_index.py`: the program's own index files against the bench's vectors for the same frames.
