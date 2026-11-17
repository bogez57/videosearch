////////////////////////////////
//~ bgz: indexer
//
// Goes through the library's videos on a thread of its own, decoding each,
// taking a frame every IDX_SAMPLE_SECONDS, running those through the image
// model (embed.h) and writing what it makes of them to the video's index
// file (index.h). A video whose file is up to date is not read again.
//
// Called from the main thread. The thread it starts calls the decoder
// (video_decoder.h), the model and the index files; what they log is handed
// to the main thread and logged by IDX_Update.

#ifndef INDEXER_H
#define INDEXER_H

// How far apart in a video the frames taken are, in seconds.
#define IDX_SAMPLE_SECONDS 1.0

// The longest side frames are decoded at. The model looks at the middle
// square of a frame scaled to 224 (EMB_IMAGE_DIM): at 400 a 16:9 frame is
// 225 high, so it is only scaled down, and by little.
#define IDX_FRAME_DIM 400

// Frames run through the model at once.
#define IDX_BATCH 8

typedef struct IDX_Indexer IDX_Indexer;

// How far it has got, as last seen.
typedef struct IDX_Status IDX_Status;
struct IDX_Status {
   U64     video_count; // in the library when it started
   U64     videos_done; // gone through, whether or not they could be decoded
   U64     videos_failed;
   U64     frames;      // indexed, in all the videos done and the one under way
   B32     no_model;    // the model could not be loaded, so nothing is indexed
   String8 current;     // the name of the video under way. Empty when none is. Good until the next IDX_Update
};

// index_folder is where the index files go, model_folder the model's.
internal IDX_Indexer* IDX_Alloc(String8 index_folder, String8 model_folder);

// Once a frame: logs what the thread has logged, and starts again from the
// first video whenever the library's videos have changed (its generation).
internal void IDX_Update(IDX_Indexer* indexer, LIB_Library* library);

internal IDX_Status IDX_GetStatus(IDX_Indexer* indexer);

// Stops the thread and lets go of the model, at the end of the program.
internal void IDX_Stop(IDX_Indexer* indexer);

#endif // INDEXER_H
