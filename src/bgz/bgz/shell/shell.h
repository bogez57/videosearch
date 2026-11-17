////////////////////////////////
//~ bgz: Shell

#ifndef BGZ_SHELL_H
#define BGZ_SHELL_H

exported void ShMessage(B32 error, String8 title, String8 message);
exported void ShShowInFileBrowser(String8 path);

// Moves a file or a folder, with everything in it, to the Recycle Bin, so it
// can be put back from there. Asks nothing and shows nothing. 1 when it is
// gone from path. Windows only for now: elsewhere it does nothing and says 0.
exported B32 ShMoveToTrash(String8 path);

// The system's open-file window, starting in initial_dir and listing files
// with extension ("spall", no dot; empty lists every file). Returns the path
// picked, on arena, and nothing when it was cancelled. It does not return
// until the user has answered, so the caller's frame waits with it.
exported String8 ShPickFile(Arena* arena, String8 title, String8 initial_dir, String8 extension);

#endif // BGZ_SHELL_H
