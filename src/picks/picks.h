////////////////////////////////
//~ bgz: picks
//
// The clips the user has picked out of the searches' hits, in the order
// they are to be cut together: kept in a file between runs, and written out
// as a timeline an editing program reads (PCK_Export, picks.md).
//
// All of it is called from the main thread.

#ifndef PICKS_H
#define PICKS_H

// The most there can be.
#define PCK_CAP 500

typedef struct PCK_Pick PCK_Pick;
struct PCK_Pick {
   String8 path;    // the video's, whole
   String8 query;   // what was searched for when it was picked
   F64     start_s; // seconds from the video's start. start and end both 0: the whole video
   F64     end_s;
   F64     best_s;  // the moment its picture is of
};

typedef struct PCK_Picks PCK_Picks;
struct PCK_Picks {
   Arena*   arena; // the picks' strings; emptied by PCK_Clear
   String8  file_path;
   PCK_Pick v[PCK_CAP];
   U64      count;
};

// The picks of last time, read from file_path, which every change is
// written to.
internal PCK_Picks* PCK_Alloc(String8 file_path);

// Which pick is this clip of this video, or -1.
internal S64 PCK_Find(PCK_Picks* picks, String8 path, F64 start_s, F64 end_s);

// Added at the end. 0 if it is there already, or there is no room.
internal B32  PCK_Add(PCK_Picks* picks, String8 path, String8 query, F64 start_s, F64 end_s, F64 best_s);
internal void PCK_Remove(PCK_Picks* picks, U64 idx);
internal void PCK_Clear(PCK_Picks* picks);

// One place up (by -1) or down (by 1) in the order.
internal void PCK_Move(PCK_Picks* picks, U64 idx, S64 by);
internal void PCK_SetRange(PCK_Picks* picks, U64 idx, F64 start_s, F64 end_s);

// How long they are together, in seconds. A whole video counts as 0: its
// length is not known here.
internal F64 PCK_Seconds(PCK_Picks* picks);

typedef struct PCK_Exported PCK_Exported;
struct PCK_Exported {
   B32 written;
   U64 clips;    // in the timeline
   U64 left_out; // their videos could not be read
};

// Writes the picks as one timeline, each a cut of its video from its start
// to its end, one after another in their order: an FCPXML file, which
// DaVinci Resolve and Final Cut import. The timeline is called what the
// file is.
internal PCK_Exported PCK_Export(PCK_Picks* picks, String8 file_path);

#endif // PICKS_H
