////////////////////////////////
//~ bgz: search
//
// What the program asks of a search: the clips in the library that match a
// query, best first. The rest of the program sees only this file, so what
// matches - names now, what is in the frames later - can change without
// anything else knowing (search.md).
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
};

// The hits for query, best first, pushed on arena. No hits for an empty
// query.
internal SRCH_Results SRCH_Run(Arena* arena, LIB_Library* library, String8 query);

#endif // SEARCH_H
