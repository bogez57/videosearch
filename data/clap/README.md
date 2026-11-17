# CLAP's tokenizer

`vocab.json` and `merges.txt`: the vocabulary and merges of RoBERTa's
tokenizer, which CLAP's text half uses, as they are in
https://huggingface.co/Xenova/clap-htsat-unfused (from `roberta-base`, MIT
licence). `src/embed/embed_sound_tokenizer.c` reads them.
