////////////////////////////////
//~ bgz: embed: pictures (and, in step 3, words) into vectors
//
// CLIP's two halves, behind one header. Each turns what it is given into
// EMB_DIM numbers, and a picture and words that describe it come out close:
// the dot product of their vectors is high. The vectors are made length one,
// so a dot product is their cosine.
//
// embed.c gets a frame ready for the model (EMB_ImageFromFrame).
// embed_onnx.c runs the model, and is the only file that knows ONNX Runtime:
// to run it with something else, write another file with its functions and
// include that one in main.c instead.
//
// The model is a folder holding vision_model.onnx: the ONNX export of
// openai/clip-vit-base-patch32 (Xenova's on Hugging Face). It is not in the
// repo: see data\models\README.md.

#ifndef EMBED_H
#define EMBED_H

#define EMB_DIM       512 // numbers in a vector
#define EMB_IMAGE_DIM 224 // the side of the square the image model looks at

// The numbers of one picture as the image model reads them: red, then green,
// then blue, each EMB_IMAGE_DIM rows from the top.
#define EMB_IMAGE_FLOATS (3 * EMB_IMAGE_DIM * EMB_IMAGE_DIM)

typedef struct EMB_Model EMB_Model;

// embed.c. A frame as the decoder gives it (rows from the top, each pixel
// red, green, blue, alpha) into image_out, EMB_IMAGE_FLOATS of them: its
// middle square, scaled to EMB_IMAGE_DIM, the way CLIP was trained on.
internal void EMB_ImageFromFrame(U8* pixels, Vec2S32 size, F32* image_out);

// embed_onnx.c. Loads the model in the folder; takes a second or two. 0 if it
// could not, and why is logged. What it needs is on arena, and in ONNX
// Runtime's own memory until EMB_Close.
internal EMB_Model* EMB_Open(Arena* arena, String8 folder);
internal void       EMB_Close(EMB_Model* model);

// count images, made by EMB_ImageFromFrame and one after another, into count
// vectors in vectors_out (EMB_DIM each). 0 if the model failed, and why is
// logged. One thread at a time.
internal B32 EMB_ImageVectors(EMB_Model* model, F32* images, U64 count, F32* vectors_out);

#endif // EMBED_H
