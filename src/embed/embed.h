////////////////////////////////
//~ bgz: embed: pictures and words into vectors
//
// CLIP's two halves, behind one header. Each turns what it is given into
// EMB_DIM numbers, and a picture and words that describe it come out close:
// the dot product of their vectors is high. The vectors are made length one,
// so a dot product is their cosine.
//
// embed.c gets a frame ready for the model (EMB_ImageFromFrame), and
// embed_tokenizer.c words (EMB_Tokenize). embed_onnx.c runs the model, and
// is the only file that knows ONNX Runtime: to run it with something else,
// write another file with its functions and include that one in main.c
// instead.
//
// The model is a folder holding vision_model.onnx and text_model.onnx: the
// ONNX export of openai/clip-vit-base-patch32 (Xenova's on Hugging Face). It
// is not in the repo: see data\models\README.md.
//
// Sound is a second model beside it, CLAP, which does for a few seconds of
// sound what CLIP does for a picture: sound and words that describe it come
// out close. Its vectors are EMB_DIM numbers too, but its own: a sound's
// vector is only compared with the vector CLAP's text half makes of the
// words, never with CLIP's. It is a folder holding audio_model.onnx and
// text_model.onnx (Xenova's export of laion/clap-htsat-unfused), and the
// program runs without it: then sound is not searched.

#ifndef EMBED_H
#define EMBED_H

#define EMB_DIM       512 // numbers in a vector
#define EMB_IMAGE_DIM 224 // the side of the square the image model looks at

// The numbers of one picture as the image model reads them: red, then green,
// then blue, each EMB_IMAGE_DIM rows from the top.
#define EMB_IMAGE_FLOATS (3 * EMB_IMAGE_DIM * EMB_IMAGE_DIM)

// The words as the text model reads them: at most this many numbers, the
// first EMB_TOKEN_START and the last EMB_TOKEN_END.
#define EMB_TEXT_TOKENS  77
#define EMB_TOKEN_START  49406
#define EMB_TOKEN_END    49407

// A sound as CLAP reads it: EMB_SOUND_SAMPLES of it (ten seconds, one
// channel, EMB_SOUND_RATE samples a second) as EMB_SOUND_FRAMES rows of
// EMB_SOUND_MELS numbers - how loud each band of pitch is, a hundredth of a
// second a row.
#define EMB_SOUND_RATE    48000
#define EMB_SOUND_SAMPLES 480000
#define EMB_SOUND_MELS    64
#define EMB_SOUND_FRAMES  1001
#define EMB_SOUND_FLOATS  (EMB_SOUND_FRAMES * EMB_SOUND_MELS)

// What CLAP's text half reads starts and ends with these.
#define EMB_SOUND_TOKEN_START 0
#define EMB_SOUND_TOKEN_END   2

typedef struct EMB_Model     EMB_Model;
typedef struct EMB_Tokenizer EMB_Tokenizer;

// embed.c. A frame as the decoder gives it (rows from the top, each pixel
// red, green, blue, alpha) into image_out, EMB_IMAGE_FLOATS of them: its
// middle square, scaled to EMB_IMAGE_DIM, the way CLIP was trained on.
internal void EMB_ImageFromFrame(U8* pixels, Vec2S32 size, F32* image_out);

// embed_tokenizer.c. The tokenizer from the merges file's text
// (data\clip\bpe_merges.txt), and text into ids_out: EMB_TOKEN_START, the
// words' numbers, EMB_TOKEN_END, then zeros. Too long a text is cut.
internal EMB_Tokenizer* EMB_TokenizerFromMerges(Arena* arena, String8 merges_text);
internal void           EMB_Tokenize(EMB_Tokenizer* tok, String8 text, S32 ids_out[EMB_TEXT_TOKENS]);

// embed_sound.c. count samples of sound, one channel at EMB_SOUND_RATE, into
// features_out, EMB_SOUND_FLOATS of them. Less than ten seconds is repeated
// to fill them, as CLAP was trained; more is cut. One thread at a time.
internal void EMB_SoundFeatures(F32* samples, U64 count, F32* features_out);

// embed_sound_tokenizer.c. CLAP's tokenizer, from its vocabulary
// (data\clap\vocab.json) and merges (data\clap\merges.txt), and text into
// ids_out: EMB_SOUND_TOKEN_START, the words' numbers, EMB_SOUND_TOKEN_END.
// Returns how many. Too long a text is cut.
internal EMB_Tokenizer* EMB_SoundTokenizerFromVocab(Arena* arena, String8 vocab_json, String8 merges_text);
internal U64            EMB_SoundTokenize(EMB_Tokenizer* tok, String8 text, S32 ids_out[EMB_TEXT_TOKENS]);

// embed_onnx.c. EMB_Open is quick: it reads the tokenizer's merges and
// starts ONNX Runtime. 0 if it could not, and why is logged. EMB_Load then
// loads the two halves, which takes a second or so - the indexer's thread
// does it. Until it has, EMB_IsLoaded is 0 and nothing can be encoded; if
// it failed, why is logged and it stays 0. The model's memory is its own
// and ONNX Runtime's, let go of in EMB_Close.
internal EMB_Model* EMB_Open(String8 folder, String8 merges_path);
internal B32        EMB_Load(EMB_Model* model);
internal B32        EMB_IsLoaded(EMB_Model* model);
internal void       EMB_Close(EMB_Model* model);

// count images, made by EMB_ImageFromFrame and one after another, into count
// vectors in vectors_out (EMB_DIM each). 0 if the model failed, and why is
// logged. One thread at a time.
internal B32 EMB_ImageVectors(EMB_Model* model, F32* images, U64 count, F32* vectors_out);

// The vector of text, into vector_out (EMB_DIM). 0 if the model is not
// loaded or failed. One thread at a time - another than the one encoding
// images may.
internal B32 EMB_TextVector(EMB_Model* model, String8 text, F32* vector_out);

// The sound model, as the four above are for CLIP: EMB_OpenSound is quick
// (the tokenizer's files) and EMB_LoadSound loads the two halves, on the
// indexer's thread. model may be 0. If the model's folder is not there, that
// is logged once as something to know, not as an error.
internal B32 EMB_OpenSound(EMB_Model* model, String8 folder, String8 vocab_path, String8 merges_path);
internal B32 EMB_LoadSound(EMB_Model* model);
internal B32 EMB_SoundIsLoaded(EMB_Model* model);

// The vector of one sound, made by EMB_SoundFeatures, and of text as words
// about a sound. As EMB_ImageVectors and EMB_TextVector.
internal B32 EMB_SoundVector(EMB_Model* model, F32* features, F32* vector_out);
internal B32 EMB_SoundTextVector(EMB_Model* model, String8 text, F32* vector_out);

#endif // EMBED_H
