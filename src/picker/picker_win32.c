////////////////////////////////
//~ bgz: picker: done with Windows' file dialogs
//
// See picker.h. A dialog is a COM object (IFileOpenDialog told to pick
// folders, IFileSaveDialog); in C its functions are called through lpVtbl. Its window belongs
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
#pragma comment(lib, "uuid") // CLSID_FileOpenDialog, IID_IFileOpenDialog, and FileSaveDialog's

// text as UTF-16, ended with a 0, for Windows.
internal WCHAR*
_PICK_Str16(Arena* arena, String8 text) {
   Temp     scratch = ScratchBegin(&arena, 1);
   String32 text32 = Str32From8(scratch.arena, text);
   WCHAR*   result = PushArray(arena, WCHAR, text32.size * 2 + 1);
   U64      size = 0;
   for (U64 i = 0; i < text32.size; i += 1) {
      U32 c = text32.str[i];
      if (c >= 0x10000) {
         c -= 0x10000;
         result[size++] = (WCHAR)(0xD800 + (c >> 10));
         result[size++] = (WCHAR)(0xDC00 + (c & 0x3FF));
      } else {
         result[size++] = (WCHAR)c;
      }
   }
   ScratchEnd(scratch);
   return result;
}

// What the dialog was answered with, as a path on arena. Nothing if it was
// cancelled: Show returns a failure then. Both dialogs are an IFileDialog.
internal String8
_PICK_Show(Arena* arena, IFileDialog* dialog) {
   String8 result = {0};
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
   return result;
}

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
      dialog->lpVtbl->SetTitle(dialog, _PICK_Str16(scratch.arena, title));
      result = _PICK_Show(arena, (IFileDialog*)dialog);
      dialog->lpVtbl->Release(dialog);
   }

   if (SUCCEEDED(com)) {
      CoUninitialize();
   }
   ScratchEnd(scratch);
   return result;
}

internal String8
PICK_SaveFile(Arena* arena, String8 title, String8 file_name, String8 type_name, String8 extension) {
   String8 result = {0};
   Temp    scratch = ScratchBegin(&arena, 1);
   HRESULT com = CoInitializeEx(0, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

   IFileSaveDialog* dialog = 0;
   if (SUCCEEDED(CoCreateInstance(&CLSID_FileSaveDialog, 0, CLSCTX_INPROC_SERVER, &IID_IFileSaveDialog, (void**)&dialog))) {
      DWORD options = 0;
      dialog->lpVtbl->GetOptions(dialog, &options);
      dialog->lpVtbl->SetOptions(dialog, options | FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT);
      dialog->lpVtbl->SetTitle(dialog, _PICK_Str16(scratch.arena, title));

      COMDLG_FILTERSPEC type = {0};
      type.pszName = _PICK_Str16(scratch.arena, type_name);
      type.pszSpec = _PICK_Str16(scratch.arena, PushStr8F(scratch.arena, "*.%S", extension));
      dialog->lpVtbl->SetFileTypes(dialog, 1, &type);
      dialog->lpVtbl->SetDefaultExtension(dialog, _PICK_Str16(scratch.arena, extension));
      dialog->lpVtbl->SetFileName(dialog, _PICK_Str16(scratch.arena, file_name));
      result = _PICK_Show(arena, (IFileDialog*)dialog);
      dialog->lpVtbl->Release(dialog);
   }

   if (SUCCEEDED(com)) {
      CoUninitialize();
   }
   ScratchEnd(scratch);
   return result;
}
