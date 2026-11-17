////////////////////////////////
//~ bgz: search
//
// What the program asks of a search: the clips in the library that match a
// query, best first. The rest of the program sees only this file.
//
// The search holds the index files of the library's videos, loaded as the
// indexer finishes them. A query is put through the text model (embed.h),
// and each frame scores the dot product of its vector and the query's: the
// frames that score highest are the hits, for now each a frame on its own.
// Until there are frames to search, or if there is no model, it matches
// file names instead (search.md).
//
// All of it is called from the main thread.

#ifndef SEARCH_H
#define SEARCH_H

// The most hits a search gives.
#define SRCH_MAX_HITS 500

// A clip that matches: a stretch of one of the library's videos. start_s and
// end_s are seconds from the video's start; both 0 is the whole video.
typedef struct SRCH_Hit SRCH_Hit;
struct SRCH_Hit {
   U64 video_idx; // in the library's videos
   F64 start_s;
   F64 end_s;
   F32 score; // higher is a better match. Only for comparing hits of one search
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

// The hits for query, best first, pushed on arena. No hits for an empty
// query.
internal SRCH_Results SRCH_Run(Arena* arena, SRCH_Search* search, LIB_Library* library, String8 query);

#endif // SEARCH_H
