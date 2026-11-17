// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

#ifndef BGZ_BASE_FILES_H
#define BGZ_BASE_FILES_H

////////////////////////////////
//~ bgz: raddebugger's Files
//
// bgz's functions for raddebugger's files (rad/base/base_files.h). Each only
// calls raddebugger's function of the same name.

exported File           FileOpen(AccessFlags flags, String8 path);
exported void           FileClose(File file);
exported U64            FileWrite(File file, Rng1U64 rng, void* data);
exported B32            FileFlush(File file);
exported B32            FileMatch(File a, File b);
exported File           FileZero(void);
exported FileProperties PropertiesFromFile(File file);
exported FileID         IDFromFile(File file);
exported String8        StringFromFileRange(Arena* arena, File file, Rng1U64 range);

exported FileProperties PropertiesFromFilePath(String8 path);
exported FileID         IDFromFilePath(String8 path);
exported B32            FilePathExists(String8 path);
exported B32            DeleteFileAtPath(String8 path);
exported B32            ReplaceFilePath(String8 dst, String8 src);
exported B32            CopyFilePath(String8 dst, String8 src);
exported B32            MakeDirectory(String8 path);
exported String8        DataFromFilePath(Arena* arena, String8 path);
exported B32            WriteDataToFilePath(String8 path, String8 data);

exported FileIter*      FileIterBegin(Arena* arena, String8 path, FileIterFlags flags);
exported B32            FileIterNext(Arena* arena, FileIter* iter, FileInfo* info_out);
exported void           FileIterEnd(FileIter* iter);

////////////////////////////////
//~ bgz: File Identity

// raddebugger's file_id_compare says whether two IDs are the same bytes. This
// says whether they are the same file: an ID that could not be determined is
// zero, and is not equal to anything - not even another zero.
exported B32 FileIDMatch(FileID a, FileID b);

////////////////////////////////
//~ bgz: Writing A List

// raddebugger's file_write writes one buffer. This writes a whole list, node
// after node from off, and says how many bytes made it - all of
// data.total_size, or the write stopped part way.
exported U64 FileWriteList(File file, U64 off, String8List data);

////////////////////////////////
//~ bgz: Directory Watching

// A watched directory. Opaque; defined by each backend.
typedef struct FileWatch FileWatch;

// What changed in a watched directory since the last poll. Paths only, on
// purpose: what an event claims happened (renamed, removed, ...) is reported
// differently per OS and cannot be trusted anyway, so callers are expected to
// look at the disk themselves.
typedef struct FileChanges FileChanges;
struct FileChanges
{
  String8List paths;      // full paths of changed entries
  B32         overflowed; // too much changed at once and the details were lost - rescan
};

exported FileWatch  *FileWatchOpen(Arena *arena, String8 dir_path, B32 recursive); // 0 on failure
exported FileChanges FileWatchPoll(Arena *arena, FileWatch *watch);                // never blocks
exported void        FileWatchClose(FileWatch *watch);

#endif // BGZ_BASE_FILES_H
