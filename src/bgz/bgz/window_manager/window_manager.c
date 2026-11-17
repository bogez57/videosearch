////////////////////////////////
//~ bgz: Window Manager

exported void
Core_WM_Init(void) {
   wm_init();
}

exported WM_SystemInfo*
Core_WM_GetSystemInfo(void) {
   WM_SystemInfo* result = wm_get_system_info();
   return result;
}

//- bgz: windows

// raddebugger's window manager handles a window's messages itself only once
// wm_get_events has given it an arena to push events on; before that, every
// message goes to Windows' default handling - including the WM_NCCALCSIZE sent
// while the window is created, which is what removes Windows' title bar.
// raddebugger opens its windows inside its frame loop, after events have been
// fetched; an app may open one before its loop, so the first open fetches once
// on a small arena of its own. What lands there is never read - the app's
// first Core_WM_GetEvents moves the window manager onto the app's arena.
exported WM_Window
Core_WM_WindowOpen(Rng2F32 rect, WM_WindowFlags flags, String8 title) {
   local_persist B32 events_hooked = 0;
   if (!events_hooked) {
      events_hooked = 1;
      wm_get_events(ArenaAlloc(.reserve_size = KB(64), .commit_size = KB(64)), 0);
   }
   WM_Window result = wm_window_open(rect, flags, title);
   return result;
}

exported void
Core_WM_WindowFirstPaint(WM_Window window) {
   wm_window_first_paint(window);
}

exported void
Core_WM_WindowFocus(WM_Window window) {
   wm_window_focus(window);
}

exported B32
Core_WM_WindowIsFocused(WM_Window window) {
   B32 result = wm_window_is_focused(window);
   return result;
}

exported B32
Core_WM_WindowIsMaximized(WM_Window window) {
   B32 result = wm_window_is_maximized(window);
   return result;
}

exported void
Core_WM_WindowSetMaximized(WM_Window window, B32 maximized) {
   wm_window_set_maximized(window, maximized);
}

exported B32
Core_WM_WindowIsMinimized(WM_Window window) {
   B32 result = wm_window_is_minimized(window);
   return result;
}

exported void
Core_WM_WindowSetMinimized(WM_Window window, B32 minimized) {
   wm_window_set_minimized(window, minimized);
}

exported void
Core_WM_WindowClose(WM_Window window) {
   wm_window_close(window);
}

exported B32
Core_WM_WindowIsFullscreen(WM_Window window) {
   B32 result = wm_window_is_fullscreen(window);
   return result;
}

exported void
Core_WM_WindowSetFullscreen(WM_Window window, B32 fullscreen) {
   wm_window_set_fullscreen(window, fullscreen);
}

exported void
Core_WM_WindowClearCustomBorderData(WM_Window handle) {
   wm_window_clear_custom_border_data(handle);
}

exported void
Core_WM_WindowPushCustomTitleBar(WM_Window handle, F32 thickness) {
   wm_window_push_custom_title_bar(handle, thickness);
}

exported void
Core_WM_WindowPushCustomTitleBarClientArea(WM_Window handle, Rng2F32 rect) {
   wm_window_push_custom_title_bar_client_area(handle, rect);
}

exported Rng2F32
Core_WM_RectFromWindow(WM_Window window) {
   Rng2F32 result = wm_rect_from_window(window);
   return result;
}

exported Rng2F32
Core_WM_ClientRectFromWindow(WM_Window window) {
   Rng2F32 result = wm_client_rect_from_window(window);
   return result;
}

exported Vec2F32
Core_WM_MouseFromWindow(WM_Window window) {
   Vec2F32 result = wm_mouse_from_window(window);
   return result;
}

//- bgz: events

exported WM_EventList
Core_WM_GetEvents(Arena* arena, B32 wait) {
   WM_EventList result = wm_get_events(arena, wait);
   return result;
}

exported void
Core_WM_EatEvent(WM_EventList* events, WM_Event* event) {
   wm_eat_event(events, event);
}

exported WM_Modifiers
Core_WM_GetModifiers(void) {
   WM_Modifiers result = wm_get_modifiers();
   return result;
}

exported B32
Core_WM_KeyIsDown(WM_Key key) {
   B32 result = wm_key_is_down(key);
   return result;
}

exported B32
Core_WM_KeyPress(WM_EventList* events, WM_Window window, WM_Modifiers modifiers, WM_Key key) {
   B32 result = wm_key_press(events, window, modifiers, key);
   return result;
}

exported String8
Core_WM_StringFromModifiersKey(Arena* arena, WM_Modifiers modifiers, WM_Key key) {
   String8 result = wm_string_from_modifiers_key(arena, modifiers, key);
   return result;
}

exported WM_Key
Core_WM_KeyFromName(String8 name) {
   WM_Key result = WM_Key_Null;
   for (WM_Key key = (WM_Key)(WM_Key_Null + 1); key < WM_Key_COUNT && result == WM_Key_Null; key = (WM_Key)(key + 1)) {
      String8 key_names[2] = {wm_key_display_name_table[key], wm_key_cfg_name_table[key]};
      for EachElement(idx, key_names) {
         String8 a = name;
         String8 b = key_names[idx];
         U64 ai = 0;
         U64 bi = 0;
         B32 match = 1;
         for (;;) {
            while (ai < a.size && (a.str[ai] == ' ' || a.str[ai] == '_')) { ai += 1; }
            while (bi < b.size && (b.str[bi] == ' ' || b.str[bi] == '_')) { bi += 1; }
            if (ai == a.size || bi == b.size) {
               match = (ai == a.size && bi == b.size);
               break;
            }
            if (lower_from_char(a.str[ai]) != lower_from_char(b.str[bi])) {
               match = 0;
               break;
            }
            ai += 1;
            bi += 1;
         }
         if (match) {
            result = key;
            break;
         }
      }
   }
   return result;
}

//- bgz: cursor and clipboard

exported void
Core_WM_SetCursor(WM_Cursor cursor) {
   wm_set_cursor(cursor);
}

exported String8
Core_WM_GetClipboardText(Arena* arena) {
   String8 result = wm_get_clipboard_text(arena);
   return result;
}

exported void
Core_WM_SetClipboardText(String8 string) {
   wm_set_clipboard_text(string);
}

// bgz's own window manager functions that need the OS, one file per OS.
#if OS_WINDOWS
# include "../win32/window_manager/win32_window_manager.c" // relative: through -I it would find rad's
#else
# error bgz window manager not implemented for this OS.
#endif
