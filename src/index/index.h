////////////////////////////////
//~ bgz: index: the vectors of a video, on disk
//
// One file per video in bin\local\index, named by a hash of the video's
// path: what the image model made of each frame the indexer took, with the
// frame's time. The indexer writes them; the search reads them.
//
// A video's sound has a second file of the same kind beside it: what the
// sound model made of each stretch of sound, with the time it starts. There
// "frame" below is a stretch of sound, and a video with no sound has a file
// with none.
//
// A file is the INX_Header, then the video's path (path_size bytes), then a
// F32 time in seconds for each frame, then each frame's vector as EMB_DIM
// F16s. It is up to date while the video's size and time written are the
// ones in it, and the model and the time between frames are the program's.

#ifndef INDEX_H
#define INDEX_H

#define INX_MAGIC   "VSINDEX1"
#define INX_VERSION 1

typedef struct INX_Header INX_Header;
struct INX_Header {
   U8        magic[8];       // INX_MAGIC
   U32       version;        // INX_VERSION
   U32       dim;            // numbers in a vector
   U64       video_size;     // in bytes
   DenseTime video_modified;
   F64       sample_seconds;
   U8        model[64];      // the model folder's name, zero after it
   U64       count;          // frames
   U64       path_size;
};

// What a file is for, all but count.
internal INX_Header INX_HeaderFor(String8 video_path, FileProperties video, String8 model, F64 sample_seconds);

// The file for a video, in folder: its pictures' (INX_PICTURES) or its
// sound's (INX_SOUND).
#define INX_PICTURES "vsi"
#define INX_SOUND    "vsa"
internal String8 INX_FilePath(Arena* arena, String8 folder, String8 video_path, char* ext);

// The file is there and was made from the video as it is now, by this model.
// Its frame count goes to count_out.
internal B32 INX_IsUpToDate(String8 file_path, INX_Header* want, U64* count_out);

// Writes the file: header (its count set), path, times and vectors. Written
// beside it first and then moved over it, so a file is whole or not there.
internal B32 INX_Write(String8 file_path, INX_Header header, String8 video_path, String8List times, String8List vectors);

// A file's frames, as INX_Write wrote them: count_out of them, their times
// and their vectors (EMB_DIM F16s each) pushed on arena. 0 if it is not
// there or not a whole index file of this version. Whether it is up to date
// is not checked: that is INX_IsUpToDate's.
internal B32 INX_Read(Arena* arena, String8 file_path, U64* count_out, F32** times_out, U16** vectors_out);

// F32 into F16, rounded to the nearest, and back.
internal U16 INX_F16FromF32(F32 f);
internal F32 INX_F32FromF16(U16 h);

#endif // INDEX_H
