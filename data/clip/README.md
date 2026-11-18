# CLIP's tokenizer

`bpe_merges.txt`: the merges CLIP's tokenizer is made of - the first 48,895
lines (a version line, then the merges) of `bpe_simple_vocab_16e6.txt.gz`
from https://github.com/openai/CLIP (`clip/`), under OpenAI's MIT licence
(`LICENSE`). `src/embed/embed_tokenizer.c` reads it.

`any_words.txt`: 208 words and phrases that could be about anything ("a
photo", "something", "a person", ...), one a line. What a frame scores for
these on average it scores for any words, whatever is in it, and the search
takes that out of a frame's score (`EMB_QueryVector`). Written for this
program; `bench/bank.txt` is the same list, where it was tried.
