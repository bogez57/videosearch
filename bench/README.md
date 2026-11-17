# bench

Test tools, not part of the program: Python scripts that measure how well ways of searching find
what is really in a set of videos, so the program's models and ranking are chosen from numbers.
The program (`src`) does not use any of this.

They work in a folder beside the repo, `..\videosearch_bench` (or `VS_BENCH`), which holds what is
downloaded and made: a Python environment (`.venv`), candidate models from Hugging Face (`models`),
a frame a second of each test video (`frames`), their vectors (`emb`), and `labels.tsv`.

- `sheets.py`: contact sheets of the frames, for labeling them by eye.
- `tags.txt`: the tags a label can use. `labels.tsv` (in the bench folder): each frame's tags and a
  caption. `labels.tsv` here in the repo is the copy the numbers in the notes were made with.
- `embed_images.py MODEL...`: every frame through a model, five ways (the whole frame squashed,
  padded, and three squares cut from it).
- `queries.txt`: searches, each with the tags a frame must have to match, or NONE.
- `bank.txt`: words that could be about anything, for taking out what a frame scores for any words.
- `eval.py CONFIG...`: scores each way of searching against the labels (the top of the file says
  how to write a config and what the numbers mean).
