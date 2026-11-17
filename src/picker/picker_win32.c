////////////////////////////////
//~ bgz: picker: done with Windows' file dialog
//
// See picker.h. The dialog is a COM object (IFileOpenDialog) told to pick
// folders; in C its functions are called through lpVtbl. Its window belongs
// to the program's, so it stays in front of it.

// Windows' headers have fields named internal and global, which here are
// words for static: put aside while the headers are read.
#pragma push_macro("internal")
#pragma push_macro("global")
#undef internal
#undef global
#include <windows.h>
#include <shobjidl.h>
#pragma pop_macro("global")
#pragma pop_macro("internal")

#pragma comment(lib, "ole32")
#pragma comment(lib, "uuid") // CLSID_FileOpenDialog, IID_IFileOpenDialog

internal String8
PICK_Folder(Arena* arena, String8 title) {
   String8 result = {0};
   Temp    scratch = ScratchBegin(&arena, 1);

   // COM may be started on this thread already, in either way: both are
   // fine, and only a start of ours is ended.
   HRESULT com = CoInitializeEx(0, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

   IFileOpenDialog* dialog = 0;
   if (SUCCEEDED(CoCreateInstance(&CLSID_FileOpenDialog, 0, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, (void**)&dialog))) {
      DWORD options = 0;
      dialog->lpVtbl->GetOptions(dialog, &options);
      dialog->lpVtbl->SetOptions(dialog, options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);

      // The title, as UTF-16.
      String32 title32 = Str32From8(scratch.arena, title);
      WCHAR*   title16 = PushArray(scratch.arena, WCHAR, title32.size * 2 + 1);
      U64      title16_size = 0;
      for (U64 i = 0; i < title32.size; i += 1) {
         U32 c = title32.str[i];
         if (c >= 0x10000) {
            c -= 0x10000;
            title16[title16_size++] = (WCHAR)(0xD800 + (c >> 10));
            title16[title16_size++] = (WCHAR)(0xDC00 + (c & 0x3FF));
         } else {
            title16[title16_size++] = (WCHAR)c;
         }
      }
      dialog->lpVtbl->SetTitle(dialog, title16);

      // Show returns a failure when the user cancels.
      if (SUCCEEDED(dialog->lpVtbl->Show(dialog, GetActiveWindow()))) {
         IShellItem* item = 0;
         if (SUCCEEDED(dialog->lpVtbl->GetResult(dialog, &item))) {
            WCHAR* path16 = 0;
            if (SUCCEEDED(item->lpVtbl->GetDisplayName(item, SIGDN_FILESYSPATH, &path16))) {
               // UTF-16 to UTF-8: three bytes at most for each unit.
               U64 size16 = 0;
               while (path16[size16] != 0) size16 += 1;
               U8* out = PushArray(arena, U8, size16 * 3 + 1);
               int size = WideCharToMultiByte(CP_UTF8, 0, path16, (int)size16, (char*)out, (int)(size16 * 3), 0, 0);
               result = Str8(out, (U64)Max(size, 0));
               CoTaskMemFree(path16);
            }
            item->lpVtbl->Release(item);
         }
      }
      dialog->lpVtbl->Release(dialog);
   }

   if (SUCCEEDED(com)) {
      CoUninitialize();
   }
   ScratchEnd(scratch);
   return result;
}
