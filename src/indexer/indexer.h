////////////////////////////////
//~ bgz: indexer
//
// Goes through the library's videos on a thread of its own, decoding each
// and taking a frame every IDX_SAMPLE_SECONDS: the frames the search will
// look at. For now the frames are only counted - the model that turns them
// into what is searched comes in step 2 of the plan (indexer.md).
//
// Called from the main thread. The thread it starts calls nothing of the
// program's but the decoder (video_decoder.h).

#ifndef INDEXER_H
#define INDEXER_H

// How far apart in a video the frames taken are, in seconds.
#define IDX_SAMPLE_SECONDS 1.0

// The longest side frames are decoded at. A model looks at a square a little
// smaller than this (224 pixels for CLIP's), and a small frame is quick to
// convert.
#define IDX_FRAME_DIM 256

typedef struct IDX_Indexer IDX_Indexer;

// How far it has got, as last seen.
typedef struct IDX_Status IDX_Status;
struct IDX_Status {
   U64     video_count; // in the library when it started
   U64     videos_done; // gone through, whether or not they could be decoded
   U64     videos_failed;
   U64     frames;      // taken, from all the videos done and the one under way
   String8 current;     // the name of the video under way. Empty when none is. Good until the next IDX_Update
};

internal IDX_Indexer* IDX_Alloc(void);

// Once a frame: starts again from the first video whenever the library's
// videos have changed (its generation).
internal void IDX_Update(IDX_Indexer* indexer, LIB_Library* library);

internal IDX_Status IDX_GetStatus(IDX_Indexer* indexer);

// Stops the thread, at the end of the program.
internal void IDX_Stop(IDX_Indexer* indexer);

#endif // INDEXER_H
