////////////////////////////////
//~ bgz: library
//
// See library.h.

// How deep under a given folder its folders are walked. Deeper than this is
// a folder that loops (a link to one above it) or is not the user's videos.
#define _LIB_MAX_DEPTH 16

internal B32
LIB_NameIsVideo(String8 name) {
   String8 extensions[] = {
       Str8Lit("mp4"), Str8Lit("m4v"), Str8Lit("mov"), Str8Lit("mkv"), Str8Lit("webm"), Str8Lit("avi"),
       Str8Lit("wmv"), Str8Lit("flv"), Str8Lit("mpg"), Str8Lit("mpeg"), Str8Lit("ts"), Str8Lit("m2ts"),
       Str8Lit("mts"), Str8Lit("3gp"), Str8Lit("ogv"), Str8Lit("vob"),
   };

   B32     result = 0;
   String8 extension = Str8SkipLastDot(name);
   if (extension.size != name.size) {
      for (U64 i = 0; i < ArrayCount(extensions); i += 1) {
         if (Str8Match(extension, extensions[i], StringMatchFlag_CaseInsensitive)) {
            result = 1;
            break;
         }
      }
   }
   return result;
}

// What is in a file, as far as telling a copy from another file of the same
// size needs: a hash of three pieces of it, its start, its middle and its
// end. 0 if it can't be read.
internal U64
_LIB_Sample(String8 path, U64 size) {
   Temp scratch = ScratchBegin(0, 0);
   U64  result = 0;
   File file = FileOpen(AccessFlag_Read | AccessFlag_ShareRead, path);
   if (!FileMatch(file, FileZero())) {
      U64 piece = Min(size, KB(64));
      U64 at[3] = {0, (size - piece) / 2, size - piece};
      B32 ok = 1;
      for (U64 i = 0; i < ArrayCount(at) && ok; i += 1) {
         String8 data = StringFromFileRange(scratch.arena, file, R1U64(at[i], at[i] + piece));
         ok = (data.size == piece);
         result = result * 31 + U64HashFromStr8(data);
      }
      result = ok ? Max(result, 1) : 0;
      FileClose(file);
   }
   ScratchEnd(scratch);
   return result;
}

internal LIB_Library*
LIB_Alloc(void) {
   // On an arena of its own: the folders' is emptied when one is taken out.
   Arena*       arena = ArenaAlloc();
   LIB_Library* library = PushArray(arena, LIB_Library, 1);
   library->folders_arena = ArenaAlloc();
   library->videos_arena = ArenaAlloc();
   return library;
}

internal B32
LIB_AddFolder(LIB_Library* library, String8 folder) {
   B32 result = 0;

   FileProperties props = PropertiesFromFilePath(folder);
   if (props.flags & FilePropertyFlag_IsFolder) {
      result = 1;
      for (String8Node* node = library->folders.first; node != 0; node = node->next) {
         if (Str8Match(node->string, folder, StringMatchFlag_CaseInsensitive)) {
            result = 0;
            break;
         }
      }
   }

   if (result) {
      Str8ListPush(library->folders_arena, &library->folders, PushStr8Copy(library->folders_arena, folder));
      LIB_Rescan(library);
   }

   return result;
}

internal void
LIB_Rescan(LIB_Library* library) {
   ArenaClear(library->videos_arena);
   Arena* arena = library->videos_arena;
   Temp   scratch = ScratchBegin(&arena, 1);

   // A folder still to be walked, and how far under its given folder it is.
   typedef struct _LIB_ToWalk _LIB_ToWalk;
   struct _LIB_ToWalk {
      _LIB_ToWalk* next;
      String8      path;
      U64          depth;
   };

   // A video found, kept by its size: one found again is left out, whether
   // it is the same path (a folder given that is under another given) or a
   // copy somewhere else (the same size and the same _LIB_Sample, which is
   // only read for files whose sizes are the same).
   typedef struct _LIB_Found _LIB_Found;
   struct _LIB_Found {
      _LIB_Found* next; // of those whose sizes share a slot
      String8     path;
      U64         size;
      U64         sample;
      B32         sampled;
   };
   _LIB_Found** found = PushArray(scratch.arena, _LIB_Found*, 4096);

   String8List paths = {0};
   U64         left_out = 0;
   for (String8Node* given = library->folders.first; given != 0; given = given->next) {
      _LIB_ToWalk* first = PushArray(scratch.arena, _LIB_ToWalk, 1);
      first->path = given->string;

      for (_LIB_ToWalk* walk = first; walk != 0;) {
         _LIB_ToWalk* next = walk->next;

         FileIter* it = FileIterBegin(scratch.arena, walk->path, FileIterFlag_SkipHiddenFiles);
         for (FileInfo info = {0}; FileIterNext(scratch.arena, it, &info);) {
            String8 path = PushStr8F(scratch.arena, "%S\\%S", walk->path, info.name);
            if (info.props.flags & FilePropertyFlag_IsFolder) {
               if (walk->depth + 1 < _LIB_MAX_DEPTH) {
                  _LIB_ToWalk* sub = PushArray(scratch.arena, _LIB_ToWalk, 1);
                  sub->path = path;
                  sub->depth = walk->depth + 1;
                  sub->next = next;
                  next = sub;
               }
            } else if (LIB_NameIsVideo(info.name)) {
               U64         size = info.props.size;
               _LIB_Found* mine = PushArray(scratch.arena, _LIB_Found, 1);
               mine->path = path;
               mine->size = size;
               _LIB_Found** slot = &found[(size * 0x9E3779B97F4A7C15ull) >> 52];
               B32          again = 0;
               for (_LIB_Found* other = *slot; other != 0 && !again; other = other->next) {
                  if (other->size != size) {
                     continue;
                  }
                  if (Str8Match(other->path, path, StringMatchFlag_CaseInsensitive)) {
                     again = 1;
                  } else {
                     if (!other->sampled) {
                        other->sample = _LIB_Sample(other->path, size);
                        other->sampled = 1;
                     }
                     if (!mine->sampled) {
                        mine->sample = _LIB_Sample(path, size);
                        mine->sampled = 1;
                     }
                     again = (mine->sample != 0 && mine->sample == other->sample);
                  }
               }
               if (again) {
                  left_out += 1;
               } else {
                  mine->next = *slot;
                  *slot = mine;
                  Str8ListPush(scratch.arena, &paths, path);
               }
            }
         }
         FileIterEnd(it);

         walk = next;
      }
   }

   library->video_count = paths.node_count;
   library->videos = PushArray(arena, LIB_Video, library->video_count);
   U64 idx = 0;
   for (String8Node* node = paths.first; node != 0; node = node->next, idx += 1) {
      LIB_Video* video = &library->videos[idx];
      video->path = PushStr8Copy(arena, node->string);
      video->name = Str8SkipLastSlash(video->path);
      video->folder = Str8ChopLastSlash(video->path);
   }
   library->left_out = left_out;
   library->generation += 1;

   LogInfoF("%llu videos in %llu folders, %llu more left out as found already\n", (unsigned long long)library->video_count,
            (unsigned long long)library->folders.node_count, (unsigned long long)left_out);

   ScratchEnd(scratch);
}

internal B32
LIB_RemoveFolder(LIB_Library* library, String8 folder) {
   B32 result = 0;

   // The folders that stay, onto the arena again from its start: copied off
   // it first, since that is where they are.
   Temp        scratch = ScratchBegin(0, 0);
   String8List kept = {0};
   for (String8Node* node = library->folders.first; node != 0; node = node->next) {
      if (Str8Match(node->string, folder, StringMatchFlag_CaseInsensitive)) {
         result = 1;
      } else {
         Str8ListPush(scratch.arena, &kept, PushStr8Copy(scratch.arena, node->string));
      }
   }

   if (result) {
      ArenaClear(library->folders_arena);
      MemoryZeroStruct(&library->folders);
      for (String8Node* node = kept.first; node != 0; node = node->next) {
         Str8ListPush(library->folders_arena, &library->folders, PushStr8Copy(library->folders_arena, node->string));
      }
      LIB_Rescan(library);
   }

   ScratchEnd(scratch);
   return result;
}
