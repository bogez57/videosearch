////////////////////////////////
//~ bgz: indexer
//
// Goes through the library's videos on a thread of its own, decoding each,
// taking a frame every IDX_SAMPLE_SECONDS, running those through the image
// model (embed.h) and writing what it makes of them to the video's index
// file (index.h). A video whose file is up to date is not read again.
//
// With the sound model there (embed.h), the video's sound goes the same way
// in the same pass: IDX_SOUND_SECONDS of it at a time, one stretch after
// another, through the sound model and into the video's sound index file.
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

// How long a stretch of sound is, in seconds and in samples. The sound model
// takes ten seconds and a shorter sound repeated; at five, a hit says where
// in the video to within five seconds.
#define IDX_SOUND_SECONDS 5.0
#define IDX_SOUND_SAMPLES (5 * EMB_SOUND_RATE)

// A stretch that is never louder than this (of 1) is silence, and left out.
#define IDX_SOUND_SILENCE 0.003f

typedef struct IDX_Indexer IDX_Indexer;

// How far it has got, as last seen.
typedef struct IDX_Status IDX_Status;
struct IDX_Status {
   U64     video_count; // in the library when it started
   U64     videos_done; // gone through, whether or not they could be decoded
   U64     videos_failed;
   U64     frames;      // indexed, in all the videos done and the one under way
   B32     no_model;    // the model could not be opened or loaded, so nothing is indexed
   String8 current;     // the name of the video under way. Empty when none is. Good until the next IDX_Update
};

// index_folder is where the index files go. model may be 0 (it could not be
// opened); model_name and sound_model_name go in the files, to say which
// model made them.
internal IDX_Indexer* IDX_Alloc(String8 index_folder, String8 model_name, String8 sound_model_name, EMB_Model* model);

// Once a frame: logs what the thread has logged, and starts again from the
// first video whenever the library's videos have changed (its generation).
internal void IDX_Update(IDX_Indexer* indexer, LIB_Library* library);

internal IDX_Status IDX_GetStatus(IDX_Indexer* indexer);

// Whether the video's index file is up to date, as of this run: the
// library's video_idx'th video at library_gen.
internal B32 IDX_IsIndexed(IDX_Indexer* indexer, U64 library_gen, U64 video_idx);
internal B32 IDX_SoundIsIndexed(IDX_Indexer* indexer, U64 library_gen, U64 video_idx);

// Stops the thread, at the end of the program.
internal void IDX_Stop(IDX_Indexer* indexer);

#endif // INDEXER_H
