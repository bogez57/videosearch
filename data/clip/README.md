# CLIP's tokenizer

`bpe_merges.txt`: the merges CLIP's tokenizer is made of - the first 48,895
lines (a version line, then the merges) of `bpe_simple_vocab_16e6.txt.gz`
from https://github.com/openai/CLIP (`clip/`), under OpenAI's MIT licence
(`LICENSE`). `src/embed/embed_tokenizer.c` reads it.
