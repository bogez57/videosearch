////////////////////////////////
//~ bgz: library
//
// The videos to search: the folders the user gave, and every video file in
// them and in the folders under them. Nothing is decoded or opened here -
// only names, found by walking the folders (library.md).
//
// All of it is called from the main thread.

#ifndef LIBRARY_H
#define LIBRARY_H

typedef struct LIB_Video LIB_Video;
struct LIB_Video {
   String8 path;   // whole, from the drive
   String8 name;   // the file's name, a piece of path
   String8 folder; // the folder it is in, a piece of path
};

typedef struct LIB_Library LIB_Library;
struct LIB_Library {
   // The folders given, in the order they were. Grows only: a folder is not
   // taken out yet.
   Arena*       folders_arena;
   String8List  folders;

   // The videos found in them, by the last LIB_Rescan. Made again, all of
   // it, by each.
   Arena*     videos_arena;
   LIB_Video* videos;
   U64        video_count;

   // Goes up by one whenever the videos are found again: what was worked out
   // from them (a search's results) knows to be worked out again.
   U64 generation;
};

// Whether a file name ends in an extension that is a video's.
internal B32 LIB_NameIsVideo(String8 name);

internal LIB_Library* LIB_Alloc(void);

// Adds the folder, if it is one and is not there already, and finds the
// videos again. folder is whole, from the drive. 0 if it was not added.
internal B32 LIB_AddFolder(LIB_Library* library, String8 folder);

// Walks every folder again for its videos.
internal void LIB_Rescan(LIB_Library* library);

#endif // LIBRARY_H
