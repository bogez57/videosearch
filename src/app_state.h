////////////////////////////////
//~ bgz: app state
//
// Everything the program knows, in one struct. There is one, made in
// _EntryPoint (main.c).

#ifndef APP_STATE_H
#define APP_STATE_H

// The longest search that can be typed, in bytes.
#define APP_QUERY_CAP 256

typedef struct App_State App_State;
struct App_State {
   WM_Window    window;
   R_Handle     window_eqp;
   Arena*       frame_arena; // cleared at the end of every frame
   WM_EventList events;      // this frame's, on frame_arena
   B32          quit;

   // Text. A change in the window's size changes every size text is drawn
   // at, and the font cache is emptied of the old ones - see _FR_BeginText
   // (frame.c).
   TXT_Fonts fonts;
   Vec2F32   text_window_dim; // the window's size when the font cache was last emptied

   // The UI layer (src\ui): its box cache, hot and active boxes, and so on,
   // made once at the start. A UI tree is built inside FrameWork every frame,
   // between _FR_BeginUI and UI_EndBuild (frame.c).
   UI_State* ui_state;
   FNT_Tag   ui_icon_font;     // raddebugger's icon font, data\fonts\Icons.ttf. The zero tag without it: no icons
   U64       ui_last_frame_us; // when the last UI build began, for its animations. 0: none yet

   // What is logged (LogInfoF, LogUserErrorF) is gathered a frame at a time
   // and written to local\logs\videosearch_log.txt beside the program, and
   // to the console (FR_WriteLogs).
   Log* log;
   File log_file;
   U64  log_file_off; // how much has been written to it

   // The folders of videos, and the videos in them (src\library).
   LIB_Library* library;

   // CLIP, run by the indexer on frames and by the search on what is typed
   // (src\embed). 0 if it could not be opened.
   EMB_Model* model;

   // Runs the videos' frames through the model on a thread of its own
   // (src\indexer).
   IDX_Indexer* indexer;

   // The frames indexed so far, and the search through them (src\search).
   SRCH_Search* search;

   // The Folders panel is up, in the list's place. It is also up whenever
   // there are no folders, whatever this says.
   B32 folders_open;

   // The search bar: what is typed, as UTF-8.
   U8  query[APP_QUERY_CAP];
   U64 query_size;
   U64 query_hash;       // of what was typed as of the last frame
   U64 query_changed_us; // when it last became something else

   // The search bar is where the user is: it was clicked or typed into last,
   // and shows a blinking caret. Typing goes to it either way. caret_us is
   // when the caret last started a blink, so it is on just after a key.
   B32 search_focused;
   U64 search_caret_us;

   // What the list shows: the search's hits, for the query they were found
   // for. Searched again when the query or the library changes (FrameWork).
   Arena*        results_arena;
   String8       results_query;       // on results_arena
   U64           results_library_gen; // the library's generation they were found in
   U64           results_search_gen;  // the search's
   SRCH_Results  results;
   UI_ScrollRows results_scroll;

   // The rows' pictures (src\thumbs).
   THM_Thumbs* thumbs;

   // The clip being played, in place of the list: a hit that was clicked.
   // 0: none. play_hit is a copy - the results can be searched again under
   // it - and play_name is on play_arena.
   VID_Video* play_video;
   Arena*     play_arena;
   SRCH_Hit   play_hit;
   String8    play_name;

   // A line of text shown for a few seconds: a folder added, a folder
   // refused. Always a string that lasts (a literal).
   String8 notice;
   U64     notice_us; // when it was set
};

#endif // APP_STATE_H
