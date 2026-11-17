////////////////////////////////
//~ bgz: Shell

exported void
ShMessage(B32 error, String8 title, String8 message) {
   sh_message(error, title, message);
}

exported void
ShShowInFileBrowser(String8 path) {
#if OS_WINDOWS
   // Explorer's call needs COM on this thread. A no-op when it is up already.
   CoInitializeEx(0, COINIT_APARTMENTTHREADED);
#endif
   sh_show_in_file_browser(path);
}

exported B32
ShMoveToTrash(String8 path) {
   B32 result = 0;
#if OS_WINDOWS
   Temp scratch = scratch_begin(0, 0);

   // SHFileOperationW takes a list of paths, each ended by a 0 and the list by
   // one more (push_array zeroes, so the two left over at the end are those),
   // with backslashes.
   String8 path_copy = PushStr8Copy(scratch.arena, path);

   for (U64 idx = 0; idx < path_copy.size; idx += 1) {
      if (path_copy.str[idx] == '/') {
         path_copy.str[idx] = '\\';
      }
   }

   String16 from16 = str16_from_8(scratch.arena, path_copy);
   U16*     from   = push_array(scratch.arena, U16, from16.size + 2);
   MemoryCopy(from, from16.str, from16.size * sizeof(U16));

   SHFILEOPSTRUCTW op = {0};
   op.hwnd            = GetActiveWindow();
   op.wFunc           = FO_DELETE;
   op.pFrom           = (WCHAR*)from;
   op.fFlags          = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

   result = SHFileOperationW(&op) == 0 && !op.fAnyOperationsAborted;

   scratch_end(scratch);
#endif
   return result;
}

// Not raddebugger's sh_pick_file: that one lets the window change the
// process's current folder, which relative paths here lean on, and has no
// filter or owner.
exported String8
ShPickFile(Arena* arena, String8 title, String8 initial_dir, String8 extension) {
   String8 result = {0};
#if OS_WINDOWS
   Temp scratch = scratch_begin(&arena, 1);

   // The filter is pairs of a name and a pattern, each ended by a 0 and the
   // list by one more. Written with '|' and swapped once it is UTF-16.
   String8 filter8 = extension.size != 0 ? PushStr8F(scratch.arena, "*.%S|*.%S|All files|*.*||", extension, extension) : PushStr8F(scratch.arena, "All files|*.*||");

   String16 filter16 = str16_from_8(scratch.arena, filter8);
   String16 title16  = str16_from_8(scratch.arena, title);
   String16 dir16    = str16_from_8(scratch.arena, initial_dir);

   for (U64 idx = 0; idx < filter16.size; idx += 1) {
      if (filter16.str[idx] == '|') {
         filter16.str[idx] = 0;
      }
   }

   for (U64 idx = 0; idx < dir16.size; idx += 1) {
      if (dir16.str[idx] == '/') {
         dir16.str[idx] = '\\';
      }
   }

   DWORD buffer_size = 4096;
   U16*  buffer      = push_array(scratch.arena, U16, buffer_size);

   OPENFILENAMEW params   = {0};
   params.lStructSize     = (DWORD)sizeof(params);
   params.hwndOwner       = GetActiveWindow(); // over the editor, which waits for it
   params.lpstrFilter     = (WCHAR*)filter16.str;
   params.lpstrFile       = (WCHAR*)buffer;
   params.nMaxFile        = buffer_size;
   params.lpstrInitialDir = dir16.size != 0 ? (WCHAR*)dir16.str : 0;
   params.lpstrTitle      = title16.size != 0 ? (WCHAR*)title16.str : 0;
   params.Flags           = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

   if (GetOpenFileNameW(&params)) {
      result = str8_from_16(arena, str16_cstring(buffer));
   }

   scratch_end(scratch);
#endif
   return result;
}
