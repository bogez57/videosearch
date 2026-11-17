////////////////////////////////
//~ bgz: picker: asking the user for a folder
//
// Windows' own window for picking a folder. bgz has one for a file
// (ShPickFile) and none for a folder, so it is here, in the app, as sound
// is: picker_win32.c is the only file that knows Windows is there.

#ifndef PICKER_H
#define PICKER_H

// Returns the folder picked, whole and on arena, and nothing when it was
// cancelled. It does not return until the user has answered, so the
// caller's frame waits with it.
internal String8 PICK_Folder(Arena* arena, String8 title);

#endif // PICKER_H
