////////////////////////////////
//~ bgz: frame
//
// One frame's work (frame.c): this frame's input, then the screen - the
// search bar, and under it the list of what matches.

#ifndef FRAME_H
#define FRAME_H

// How long a notice stays up.
#define FR_NOTICE_SECONDS 4.0f

// Everything between the renderer's begin and end of a frame (main.c).
internal void FrameWork(App_State* app_state);

// path as a whole path, from the drive, with \ between its parts: one that
// is not whole already is taken from the folder the program was started in.
internal String8 FR_WholePath(Arena* arena, String8 path);

// The program's own folder for what it keeps from run to run - the log, the
// folders of videos: local\ beside the program. Made if it is not there.
internal String8 FR_LocalFolder(Arena* arena);

// Starts the log's file for this run, keeping the last run's beside it.
internal void FR_OpenLogFile(App_State* app_state);

// Writes what a log scope gathered to the log's file and the console: what
// went wrong marked !!!, warnings ???, the rest >>>.
internal void FR_WriteLogs(App_State* app_state, LogScopeResult logs);

// The folders of videos searched last time, from their file beside the
// program, into the library. Once, at the start.
internal void FR_LoadFolders(App_State* app_state);

// Adds path to the library - a folder, or the folder of a file in one - and
// remembers it for next time. A notice says what came of it.
internal void FR_AddFolder(App_State* app_state, String8 path);

// notice must last: a literal.
internal void    FR_Notice(App_State* app_state, String8 notice);
// The notice, if one is up now, else an empty string.
internal String8 FR_CurrentNotice(App_State* app_state);

// The screen's sizes are for a window 900 pixels tall, and grow with a
// taller one up to twice, but do not shrink with a shorter: how many pixels
// a unit is.
internal F32 FR_UIScale(Vec2F32 window_dim);

// The UI layer's font size for text px pixels tall, as the font cache takes
// it: points, whole ones.
internal F32 FR_UIFontSize(F32 px);

#endif // FRAME_H
