// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

////////////////////////////////
//~ bgz: raddebugger's Files

exported File
FileOpen(AccessFlags flags, String8 path) {
   File result = file_open(flags, path);
   return result;
}

exported void
FileClose(File file) {
   file_close(file);
}

exported U64
FileWrite(File file, Rng1U64 rng, void* data) {
   U64 result = file_write(file, rng, data);
   return result;
}

exported B32
FileFlush(File file) {
   B32 result = file_flush(file);
   return result;
}

exported B32
FileMatch(File a, File b) {
   B32 result = file_match(a, b);
   return result;
}

exported File
FileZero(void) {
   File result = file_zero();
   return result;
}

exported FileProperties
PropertiesFromFile(File file) {
   FileProperties result = properties_from_file(file);
   return result;
}

exported FileID
IDFromFile(File file) {
   FileID result = id_from_file(file);
   return result;
}

exported String8
StringFromFileRange(Arena* arena, File file, Rng1U64 range) {
   String8 result = string_from_file_range(arena, file, range);
   return result;
}

exported FileProperties
PropertiesFromFilePath(String8 path) {
   FileProperties result = properties_from_file_path(path);
   return result;
}

exported FileID
IDFromFilePath(String8 path) {
   FileID result = id_from_file_path(path);
   return result;
}

exported B32
FilePathExists(String8 path) {
   B32 result = file_path_exists(path);
   return result;
}

exported B32
DeleteFileAtPath(String8 path) {
   B32 result = delete_file_at_path(path);
   return result;
}

exported B32
ReplaceFilePath(String8 dst, String8 src) {
   B32 result = replace_file_path(dst, src);
   return result;
}

exported B32
CopyFilePath(String8 dst, String8 src) {
   B32 result = copy_file_path(dst, src);
   return result;
}

exported B32
MakeDirectory(String8 path) {
   B32 result = make_directory(path);
   return result;
}

exported String8
DataFromFilePath(Arena* arena, String8 path) {
   String8 result = data_from_file_path(arena, path);
   return result;
}

exported B32
WriteDataToFilePath(String8 path, String8 data) {
   B32 result = write_data_to_file_path(path, data);
   return result;
}

exported FileIter*
FileIterBegin(Arena* arena, String8 path, FileIterFlags flags) {
   FileIter* result = file_iter_begin(arena, path, flags);
   return result;
}

exported B32
FileIterNext(Arena* arena, FileIter* iter, FileInfo* info_out) {
   B32 result = file_iter_next(arena, iter, info_out);
   return result;
}

exported void
FileIterEnd(FileIter* iter) {
   file_iter_end(iter);
}

////////////////////////////////
//~ bgz: File Identity

exported U64
FileWriteList(File file, U64 off, String8List data)
{
  U64 result = 0;
  for(String8Node *n = data.first; n != 0; n = n->next)
  {
    U64 written = file_write(file, r1u64(off + result, off + result + n->string.size), n->string.str);
    result += written;
    if(written != n->string.size)
    {
      break;
    }
  }
  return result;
}

exported B32
FileIDMatch(FileID a, FileID b)
{
  FileID zero = {0};
  B32 result = (file_id_compare(a, zero) != 0 && file_id_compare(a, b) == 0);
  return result;
}
