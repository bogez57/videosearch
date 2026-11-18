////////////////////////////////
//~ bgz: picker: asking the user for a folder, or where to save a file
//
// Windows' own window for picking a folder, and its one for saving. bgz has one for a file
// (ShPickFile) and none for a folder, so it is here, in the app, as sound
// is: picker_win32.c is the only file that knows Windows is there.

#ifndef PICKER_H
#define PICKER_H

// Returns the folder picked, whole and on arena, and nothing when it was
// cancelled. It does not return until the user has answered, so the
// caller's frame waits with it.
internal String8 PICK_Folder(Arena* arena, String8 title);

// As PICK_Folder, for a file to be written: where the user wants it and
// under what name, whole and on arena. file_name is what it is called until
// the user says otherwise; extension ("fcpxml", no dot) is put on a name
// typed without one, and type_name says what such a file is. Windows has
// asked already if a file of that name may be written over.
internal String8 PICK_SaveFile(Arena* arena, String8 title, String8 file_name, String8 type_name, String8 extension);

#endif // PICKER_H
