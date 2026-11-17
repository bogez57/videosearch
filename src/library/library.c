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

internal LIB_Library*
LIB_Alloc(void) {
   Arena*       folders_arena = ArenaAlloc();
   LIB_Library* library = PushArray(folders_arena, LIB_Library, 1);
   library->folders_arena = folders_arena;
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

   String8List paths = {0};
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
               Str8ListPush(scratch.arena, &paths, path);
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
   library->generation += 1;

   LogInfoF("%llu videos in %llu folders\n", (unsigned long long)library->video_count,
            (unsigned long long)library->folders.node_count);

   ScratchEnd(scratch);
}
