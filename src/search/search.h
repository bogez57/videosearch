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
// clips are hits in the same list. A clip of frames and a clip of sound that
// both match well, at the same time in the same video, are one hit.
// Until there are frames to search, or if there is no model, it matches
// file names instead (search.md).
//
// All of it is called from the main thread.

#ifndef SEARCH_H
#define SEARCH_H

// The most hits a search gives.
#define SRCH_MAX_HITS 500

// A model's own score for a picture (or a sound) and words is in a narrow
// band, and where the band is depends on the words: CLIP gives a frame
// about 0.19 to 0.23 for words it has nothing to do with, and only some
// 0.03 to 0.10 more than that for words it matches. So a frame's score has
// taken from it what the library's frames score for the query on average,
// and a hit's score is what is left, stretched so the band's low end is 0
// and its high end 1 - CLIP's and the sound model's each by its own, which
// is what makes the two comparable. The picture's band is from the same 58
// test searches as the sound's: the best frame for words that were in no
// video ("a cat", "something") was left with about 0.04, for words that
// were ("a desert", "a soldier in a trench") with 0.07 to 0.11.
//
// The sound model gives some stretches of sound a good score for any words
// at all, so a stretch's score has taken from it what it scores for words
// that say nothing ("a sound", "something", ...: search.c). The sound's band
// is for what is left, and is from 58 test searches on five film trailers:
// sound that matched the words was left with 0.3 to 0.5, and sound for words
// about something seen, not heard, mostly with under 0.3 - though up to 0.4:
// the sound model can't tell them apart well.
#define SRCH_PICTURE_LOW  0.03f
#define SRCH_PICTURE_HIGH 0.10f
#define SRCH_SOUND_LOW    0.20f
#define SRCH_SOUND_HIGH   0.50f

// How far below a clip's best frame its other frames may score, in the
// model's own score.
#define SRCH_CLIP_MARGIN  0.02f
#define SRCH_SOUND_MARGIN 0.05f

// A clip of frames and a clip of sound become one hit when both score at
// least this. The hit scores as the better of the two, and this much of the
// other on top.
#define SRCH_BOTH_FLOOR 0.35f
#define SRCH_BOTH_SHARE 0.25f

// A clip is a hit only if it scores at least this much of what the best
// clip found the same way does (of 1, if the best scores more than that):
// by its frames, by its sound, or by both.
#define SRCH_KEEP 0.5f

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

#endif // SEARCH_H
