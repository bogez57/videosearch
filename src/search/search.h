////////////////////////////////
//~ bgz: search
//
// What the program asks of a search: the clips in the library that match a
// query, best first. The rest of the program sees only this file.
//
// The search holds the index files of the library's videos, loaded as the
// indexer finishes them. A query is put through the text model (embed.h),
// and each frame scores the dot product of its vector and the query's. A run
// of neighbouring frames that score nearly as well as the best of them is a
// clip, and the clips that score highest are the hits. The videos' sound is
// searched the same way with the sound model, where there is one and the
// query is about sound, and its clips are hits in the same list. A clip of
// frames and a clip of sound that both match well, at the same time in the
// same video, are one hit.
// Until there are frames to search, or if there is no model, it matches
// file names instead (search.md).
//
// All of it is called from the main thread.

#ifndef SEARCH_H
#define SEARCH_H

// The most hits a search gives.
#define SRCH_MAX_HITS 500

// A frame's own score for a query is its vector's dot product with the
// query's (EMB_QueryVector): around 0 for words that have nothing to do with
// it, and some 0.1 to 0.25 for words that say what is in it. A stretch of
// sound's is the same with the sound model's (EMB_SoundQueryVector). A hit's
// score is that stretched so that LOW is 0 and HIGH is 1 - the picture's and
// the sound's each by its own, which is what makes the two comparable - and
// a clip that scores 0 or under is not a hit.
//
// The numbers are from the test bench (bench\README.md), where twelve videos
// whose every frame is labelled were searched 224 ways. PICTURE_HIGH is what
// the best right frame scores for a search with something to find, the
// middle one of them. PICTURE_LOW is a choice between two wrongs. At 0.122,
// 3 in 100 searches with something to find list nothing, and 4 in 10 of
// those with nothing to find ("a telescope") list a row of what is not
// there, with a low match. Higher, and fewer of those do, but plain words
// whose right frames score only 0.14 ("a woman", "a guitar") list nothing:
// at 0.158, which leaves 19 in 20 of the searches with nothing to find
// empty, so are 12 in 100 of those with something.
//
// SOUND_LOW is what three in four searches for a sound that is in no video
// stay under with their best stretch. A sound mixed into a video's own as
// loud as it is scores 0.32, the middle one, and at three times as loud
// 0.45; singing where the labels say someone sings, 0.25 to 0.45.
#define SRCH_PICTURE_LOW  0.122f
#define SRCH_PICTURE_HIGH 0.192f
#define SRCH_SOUND_LOW    0.28f
#define SRCH_SOUND_HIGH   0.50f

// The sound is searched only for a query that is at least this much about
// sound (EMB_SoundQueryVector's about_sound). On the bench that is 29 of 32
// searches for a sound ("a crowd cheering", "a man shouting" and "a person
// sneezing" are kept out), 2 of 30 for something only seen ("a man with a
// beard"), and a quarter of the searches for pictures, most of them of what
// is heard as well ("a man screaming", "a helicopter", "an explosion").
#define SRCH_SOUND_WORDS -0.07f

// How far below a clip's best frame its other frames may score, in a hit's
// score.
#define SRCH_CLIP_MARGIN  0.30f
#define SRCH_SOUND_MARGIN 0.17f

// A clip of frames and a clip of sound become one hit when both score at
// least this. The hit scores as the better of the two, and this much of the
// other on top.
#define SRCH_BOTH_FLOOR 0.35f
#define SRCH_BOTH_SHARE 0.25f

// A clip is a hit only if it scores at least this much of what the best
// clip found the same way does: by its frames, by its sound, or by both.
#define SRCH_KEEP 0.3f

// A clip that matches: a stretch of one of the library's videos. start_s and
// end_s are seconds from the video's start; both 0 is the whole video.
// best_s is when the frame that matched best is.
typedef struct SRCH_Hit SRCH_Hit;
struct SRCH_Hit {
   U64 video_idx; // in the library's videos
   F64 start_s;
   F64 end_s;
   F64 best_s;
   F32 score;    // about 0 for no match to 1 for a good one; it can be under or over
   B32 by_sound; // found by what is heard in it, not what is seen
   B32 by_both;  // found by what is seen and by what is heard
};

typedef struct SRCH_Results SRCH_Results;
struct SRCH_Results {
   SRCH_Hit* v;
   U64       count;
   B32       by_name;     // matched by file name, not by what is in the frames
   F32       about_sound; // how much the query is about sound (SRCH_SOUND_WORDS), for the log
};

typedef struct SRCH_Search SRCH_Search;

// index_folder is the indexer's. model may be 0.
internal SRCH_Search* SRCH_Alloc(String8 index_folder, EMB_Model* model);

// Once a frame, after IDX_Update: loads the index files the indexer has
// finished since. Its generation goes up whenever what a search would find
// has changed - more frames, or the model loaded - so results can be
// worked out again.
internal void SRCH_Update(SRCH_Search* search, LIB_Library* library, IDX_Indexer* indexer);
internal U64  SRCH_Generation(SRCH_Search* search);

// Lets go of every index file loaded: the next SRCH_Update loads them again
// as the indexer finishes them. For when they are made again (IDX_Reindex).
internal void SRCH_Forget(SRCH_Search* search);

// The hits for query, best first, pushed on arena. No hits for an empty
// query.
internal SRCH_Results SRCH_Run(Arena* arena, SRCH_Search* search, LIB_Library* library, String8 query);

#endif // SEARCH_H
