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
- `queries.txt`: searches, each with the tags a frame must have to match (or words its caption
  must have), or NEAR / NONE for something that is in none of the videos.
- `bank.txt`: words that could be about anything, for taking out what a frame scores for any words.
- `eval.py CONFIG...`: scores each way of searching against the labels (the top of the file says
  how to write a config and what the numbers mean).
- `clips.py CONFIG...`: the list the program would show for each search (clips made of the
  frames' scores as `search.c` makes them, the weak ones left out), judged row by row.
- `flickr.py MODEL...`: 1000 captioned photographs (Flickr30k's test split): whether each model
  finds a caption's photograph as often as its makers say, and pictures of other things for
  `eval.py` to set a search's scores against.
- `sound.py`: the sound model against sound whose content is known: ESC-50 (2000 labeled clips),
  ESC-50 sounds mixed into the test videos' own sound, and the videos' own sound where the labels
  say what should be heard. `bank_sound.txt`: words about sounds of every kind, as `bank.txt` is
  for pictures.
- `check_index.py`: the program's own index files against the bench's vectors for the same frames.
- `app_check.py`, `app_check.patch`: the built program against the bench, in a scratch copy of the
  repo: the patch makes it run every search of `queries.txt` and log its vectors and hits, and the
  script sets those against the bench's.

## What it found

Twelve videos (five film trailers and seven clips), a frame a second: 1764 frames, each labelled by
eye. 171 searches with something to find and 53 with nothing (`queries.txt`). The first five videos
were used to choose between ways; the seven clips were labelled after, and say the same.

How well the frames are ranked (`eval.py`). mAP: are the right frames over the wrong ones. First:
how often the best hit is right. Five: how many of the best five moments are. Kept: with one line
drawn so that 95 in 100 searches with nothing to find show nothing, how many searches with
something to find still show a right frame.

| Way                                                             | mAP  | First | Five | Kept |
| --------------------------------------------------------------- | ---- | ----- | ---- | ---- |
| The program before: CLIP ViT-B/32, the frame's middle square    | 0.40 | 0.59  | 0.44 | 0.30 |
| MobileCLIP-B, middle square, less what any words score          | 0.63 | 0.77  | 0.58 | 0.74 |
| and the query read five ways                                    | 0.65 | 0.81  | 0.58 | 0.77 |
| and the whole frame beside the middle square                    | 0.66 | 0.79  | 0.59 | 0.83 |
| and three squares beside the whole frame: the program now       | 0.67 | 0.81  | 0.60 | 0.85 |
| SigLIP 2 B/16, the same way                                     | 0.66 | 0.84  | 0.60 | 0.89 |
| Both models, their scores added                                 | 0.68 | 0.83  | 0.61 | 0.91 |

On the seven clips alone, which had no part in the choosing: before 0.42 / 0.59 / 0.23 (mAP,
first, kept), now 0.68 / 0.80 / 0.84, SigLIP 2 0.69 / 0.83 / 0.91.

- **The model.** On the first five videos, with the middle square, plain words and the first
  change below: CLIP B/32 0.52 mAP, MobileCLIP-B 0.65. With the other two as well, CLIP B/32
  reaches 0.59 and MobileCLIP-B 0.69, so the model is most of it. MobileCLIP-B finds a caption's photograph
  among 1000 at the first try 77 times in 100 (`flickr.py`; CLIP B/32 59, B/16 62, SigLIP 2 74),
  which is what its makers say, so it is run as it should be. A picture takes it 6.4 ms on J's
  card (CLIP B/32 2.0, SigLIP 2 7.0, CLIP L/14 23). SigLIP 2 is as good, a little better at
  telling a search with nothing to find, but reads words with another tokenizer (SentencePiece)
  and its text half is 1.1 GB; MobileCLIP-B reads them with CLIP's, which the program has.
- **Less what any words score** (`fb`). Some frames score well for any words and some for none.
  What a frame scores for 208 phrases that could be about anything (`bank.txt`) is that part, and
  taking their average vector from the query's takes it out. This is what makes one score mean
  nearly the same for every search: before it, half of what the library's frames averaged for the
  query was taken out, which moved with the library.
- **The query read five ways** (`ens`): as it is, and after "a photo of", "a video frame of", "a
  scene from a movie showing" and "a picture of", the five vectors averaged. "A scene from a
  movie showing" alone does worse than the plain words.
- **Four views of a frame.** The model looks at a square. The whole frame pressed into it keeps
  what is at the sides, three squares cut from it keep the shapes; the four vectors averaged do
  better than any one. It is four times the model's work for each frame.
- **What did not help:** scaling a score by how far the query is from the bank's middle (`fbs`,
  `cent`): the same ranking, and fewer searches kept.

The list a search shows (`clips.py`), with the low end of the picture's band at 0.122 and the
rest as `search.h` has it: 10 rows on average, 66 in 100 of them right, the first row right for 80
in 100 searches, 3 in 100 searches with something to find list nothing, and 60 in 100 with nothing
to find list nothing (the rest a row, with a low match). Before: 7 rows, 57 right, the first right
for 41, and 39 in 100 searches with something to find listed nothing. `--scan` has the other
places the line could be.

What it is still poor at, of the searches tried: "a cat" (small, in the dark), "a monster", "an oil
lamp", "yellow flowers", "a person climbing", "a palace", "a flag".

Sound (`sound.py`). CLAP `clap-htsat-unfused` stays: the two larger CLAPs name ESC-50's sounds a
little more often (0.94 and 0.93 mAP against 0.91) but are no better at the videos' own sound.
"This is a sound of X." for a query, less the average of 95 phrases about sounds of every kind
(`bank_sound.txt`), does better than the plain words less ten words that say nothing, as the
program had it: a sound mixed into a video's own at the same loudness is ranked 0.69 mAP (from
0.63), at three times as loud 0.84 (from 0.78). No score
of CLAP's tells a search for a picture from one for a sound ("a hockey game" scores 0.60 at best
on these videos, singing where there is singing 0.42), so the words themselves are asked: how
much nearer its text half puts them to the phrases about sound than to `bank.txt`'s (`sound.py
words`). The line at -0.07 lets through 31 of 32 searches for a sound and a quarter of the searches
for pictures.
