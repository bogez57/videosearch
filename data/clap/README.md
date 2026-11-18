# CLAP's tokenizer

`vocab.json` and `merges.txt`: the vocabulary and merges of RoBERTa's
tokenizer, which CLAP's text half uses, as they are in
https://huggingface.co/Xenova/clap-htsat-unfused (from `roberta-base`, MIT
licence). `src/embed/embed_sound_tokenizer.c` reads them.

`sound_words.txt`: 95 words and phrases about sounds of every kind ("a dog
barking", "rain falling", "a man shouting", ...), one a line. What a stretch
of sound scores for these on average it scores for any words about sound,
and the search takes that out of its score; and a query is only searched for
in the sound if the sound model puts it nearer to these than to
`data/clip/any_words.txt` (`EMB_SoundQueryVector`). Written for this
program; `bench/bank_sound.txt` is the same list, where it was tried.
