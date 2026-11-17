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
// searched the same way with the sound model, where there is one, and its
// clips are hits in the same list.
// Until there are frames to search, or if there is no model, it matches
// file names instead (search.md).
//
// All of it is called from the main thread.

#ifndef SEARCH_H
#define SEARCH_H

// The most hits a search gives.
#define SRCH_MAX_HITS 500

// A model's own score for a picture (or a sound) and words is in a narrow
// band: CLIP's is about 0.18 when they have nothing to do with each other
// and about 0.33 when they match well. A hit's score is that band stretched
// so its low end is 0 and its high end 1 - CLIP's and the sound model's each
// by its own, which is what makes the two comparable. The sound model's
// band is from a handful of test searches (a tone scored 0.48 for "a beep",
// sound that had nothing to do with the words up to about 0.4).
#define SRCH_PICTURE_LOW  0.18f
#define SRCH_PICTURE_HIGH 0.33f
#define SRCH_SOUND_LOW    0.20f
#define SRCH_SOUND_HIGH   0.55f

// How far below a clip's best frame its other frames may score, in the
// model's own score.
#define SRCH_CLIP_MARGIN  0.02f
#define SRCH_SOUND_MARGIN 0.05f

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
};

typedef struct SRCH_Results SRCH_Results;
struct SRCH_Results {
   SRCH_Hit* v;
   U64       count;
   B32       by_name; // matched by file name, not by what is in the frames
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

// For working out the ranking, to be taken out again: each line of the file
// at queries_path as a query, and to out_path every stretch of sound's own
// score for it, and what the frames' scores are like.
internal void SRCH_DumpScores(SRCH_Search* search, LIB_Library* library, String8 queries_path, String8 out_path);

#endif // SEARCH_H
