////////////////////////////////
//~ bgz: Window Manager
//
// bgz's functions for raddebugger's window manager (rad/window_manager/
// window_manager.h). Most only call raddebugger's function of the same name;
// the ones that do more, or are bgz's own, say so.

#ifndef BGZ_WINDOW_MANAGER_H
#define BGZ_WINDOW_MANAGER_H

exported void            Core_WM_Init(void);
exported WM_SystemInfo*  Core_WM_GetSystemInfo(void);

//- bgz: windows

// raddebugger's, but fine to call before the app's first Core_WM_GetEvents -
// see window_manager.c.
exported WM_Window       Core_WM_WindowOpen(Rng2F32 rect, WM_WindowFlags flags, String8 title);
exported void            Core_WM_WindowFirstPaint(WM_Window window);

// Closes a window the app opened. Its render equipment is the app's to let go
// of first (Core_R_WindowUnequip).
exported void            Core_WM_WindowClose(WM_Window window);

// Brings the window to the front with the keyboard. Windows only allows it when
// this program was given the right - by being started from the program in
// front, or by that program calling AllowOtherProcessToTakeFocus.
exported void            Core_WM_WindowFocus(WM_Window window);
exported B32             Core_WM_WindowIsFocused(WM_Window window);
exported B32             Core_WM_WindowIsMaximized(WM_Window window);
exported void            Core_WM_WindowSetMaximized(WM_Window window, B32 maximized);
exported B32             Core_WM_WindowIsMinimized(WM_Window window);
exported void            Core_WM_WindowSetMinimized(WM_Window window, B32 minimized);

// Fullscreen is the window with no frame, covering the monitor it is on. Its
// place and size from before come back when fullscreen is turned off.
exported B32             Core_WM_WindowIsFullscreen(WM_Window window);
exported void            Core_WM_WindowSetFullscreen(WM_Window window, B32 fullscreen);

exported void            Core_WM_WindowClearCustomBorderData(WM_Window handle);
exported void            Core_WM_WindowPushCustomTitleBar(WM_Window handle, F32 thickness);
exported void            Core_WM_WindowPushCustomTitleBarClientArea(WM_Window handle, Rng2F32 rect);

// Where the window is on screen, frame included - what Core_WM_WindowOpen
// takes back to open it there again.
exported Rng2F32         Core_WM_RectFromWindow(WM_Window window);
exported Rng2F32         Core_WM_ClientRectFromWindow(WM_Window window);
exported Vec2F32         Core_WM_MouseFromWindow(WM_Window window);

// Whether any part of rect, in screen coordinates, is on a monitor that is
// connected now. bgz's own - raddebugger's window manager has no monitor
// positions.
exported B32             Core_WM_RectIsOnAMonitor(Rng2F32 rect);

//- bgz: events

exported WM_EventList    Core_WM_GetEvents(Arena* arena, B32 wait);
exported void            Core_WM_EatEvent(WM_EventList* events, WM_Event* event);
exported WM_Modifiers    Core_WM_GetModifiers(void);
exported B32             Core_WM_KeyIsDown(WM_Key key);
exported B32             Core_WM_KeyPress(WM_EventList* events, WM_Window window, WM_Modifiers modifiers, WM_Key key);
exported String8         Core_WM_StringFromModifiersKey(Arena* arena, WM_Modifiers modifiers, WM_Key key);

// bgz's own: a key by its display name ("Left Bracket") or config name
// ("left_bracket"), ignoring case, spaces and underscores. WM_Key_Null if none.
exported WM_Key          Core_WM_KeyFromName(String8 name);

//- bgz: cursor and clipboard

exported void            Core_WM_SetCursor(WM_Cursor cursor);
exported String8         Core_WM_GetClipboardText(Arena* arena);
exported void            Core_WM_SetClipboardText(String8 string);

#endif // BGZ_WINDOW_MANAGER_H
