////////////////////////////////
//~ bgz: one frame's work
//
// FrameWork is everything between the renderer's begin and end of a frame
// (main.c): read this frame's input, search again if what was typed or the
// videos changed, build the UI layer's tree - the search bar, the status
// line, the list of hits - and draw it. It also runs with no events at all,
// when Windows asks for a repaint during a resize - see _Main_RepaintFrame in
// main.c.
//
//    +--------------------------------------------------------+
//    |  [ a car driving down a highway|                    ]  |
//    |  3 hits. 120 videos in 2 folders.                      |
//    |    highway_trip.mp4      at 1:23   0.31   D:\videos\2024  |
//    |    ...                                                 |
//    |  Type to search. Drop a folder of videos to add it.    |
//    +--------------------------------------------------------+
//
// The UI parts are ported from the slideshow's frame.c (_FR_UIEventsFromEvents,
// _FR_BeginUI, _FR_DrawUITree), which has them from the editor's.

internal void
FR_Notice(App_State* app_state, String8 notice) {
   app_state->notice = notice;
   app_state->notice_us = NowTimeUS();
}

internal String8
FR_CurrentNotice(App_State* app_state) {
   String8 result = {0};
   U64     notice_us = (U64)(FR_NOTICE_SECONDS * 1000000.0f);
   if (app_state->notice.size != 0 && NowTimeUS() - app_state->notice_us < notice_us) {
      result = app_state->notice;
   }
   return result;
}

internal String8
FR_WholePath(Arena* arena, String8 path) {
   B32 is_whole = ((path.size >= 2 && path.str[1] == ':') ||
                   (path.size >= 1 && (path.str[0] == '\\' || path.str[0] == '/')));

   String8 result = {0};
   if (is_whole) {
      result = PushStr8Copy(arena, path);
   } else {
      Temp    scratch = ScratchBegin(&arena, 1);
      String8 start = GetCurrentPath(scratch.arena);
      while (start.size > 0 && (start.str[start.size - 1] == '\\' || start.str[start.size - 1] == '/')) {
         start.size -= 1;
      }
      // "." and ".\\name" are the folder itself and name in it.
      while (path.size >= 1 && path.str[0] == '.' &&
             (path.size == 1 || path.str[1] == '\\' || path.str[1] == '/')) {
         path = Str8Skip(path, Min(path.size, 2));
      }
      result = (path.size != 0) ? PushStr8F(arena, "%S\\%S", start, path) : PushStr8Copy(arena, start);
      ScratchEnd(scratch);
   }

   for (U64 i = 0; i < result.size; i += 1) {
      if (result.str[i] == '/') {
         result.str[i] = '\\';
      }
   }
   // No \ at the end - but a drive's own folder keeps its one: C:\ .
   while (result.size > 3 && result.str[result.size - 1] == '\\') {
      result.size -= 1;
   }

   return result;
}

////////////////////////////////
//~ bgz: the log

internal String8
FR_LocalFolder(Arena* arena) {
   String8 result = PushStr8F(arena, "%S\\local", GetProcessInfo()->binary_path);
   if (!FilePathExists(result)) {
      MakeDirectory(result);
   }
   return result;
}

internal void
FR_OpenLogFile(App_State* app_state) {
   Temp    scratch = ScratchBegin(0, 0);
   String8 folder = PushStr8F(scratch.arena, "%S\\logs", FR_LocalFolder(scratch.arena));
   if (!FilePathExists(folder)) {
      MakeDirectory(folder);
   }

   // The last run's log is kept, one back: it is the one wanted after a run
   // that went wrong.
   String8 path = PushStr8F(scratch.arena, "%S\\videosearch_log.txt", folder);
   if (FilePathExists(path)) {
      CopyFilePath(PushStr8F(scratch.arena, "%S\\videosearch_log_previous.txt", folder), path);
   }

   // ShareRead: it can be read while the program runs.
   app_state->log_file = FileOpen(AccessFlag_Write | AccessFlag_ShareRead, path);
   app_state->log_file_off = 0;

   ScratchEnd(scratch);
}

internal void
FR_WriteLogs(App_State* app_state, LogScopeResult logs) {
   Temp scratch = ScratchBegin(0, 0);

   char* prefixes[LogMsgKind_COUNT] = {0};
   prefixes[LogMsgKind_Info] = ">>> ";
   prefixes[LogMsgKind_UserError] = "!!! ";
   prefixes[LogMsgKind_UserWarning] = "??? ";

   for (U64 kind = 0; kind < LogMsgKind_COUNT; kind += 1) {
      // A line at a time, each with what kind it is in front.
      String8 text = logs.strings[kind];
      U64     pos = 0;
      while (pos < text.size) {
         U64 line_start = pos;
         while (pos < text.size && text.str[pos] != '\n') pos += 1;
         String8 line = Str8SkipChopWhitespace(Str8Substr(text, R1U64(line_start, pos)));
         pos += 1;
         if (line.size == 0) {
            continue;
         }

         String8 out = PushStr8F(scratch.arena, "%s%S\n", prefixes[kind], line);
         if (!FileMatch(app_state->log_file, FileZero())) {
            app_state->log_file_off +=
                FileWrite(app_state->log_file, R1U64(app_state->log_file_off, app_state->log_file_off + out.size), out.str);
         }
         // And to the console, for a run started from one.
         printf("%.*s", Str8VArg(out));
      }
   }

   ScratchEnd(scratch);
}

////////////////////////////////
//~ bgz: the folders of videos

// In the program's local folder, beside it, so that it goes where the
// program goes and is found wherever the program is started from. A folder
// a line.
internal String8
_FR_FoldersFilePath(Arena* arena) {
   Temp    scratch = ScratchBegin(&arena, 1);
   String8 result = PushStr8F(arena, "%S\\folders.txt", FR_LocalFolder(scratch.arena));
   ScratchEnd(scratch);
   return result;
}

internal void
_FR_SaveFolders(App_State* app_state) {
   Temp        scratch = ScratchBegin(0, 0);
   String8List lines = {0};
   for (String8Node* node = app_state->library->folders.first; node != 0; node = node->next) {
      Str8ListPush(scratch.arena, &lines, node->string);
      Str8ListPush(scratch.arena, &lines, Str8Lit("\n"));
   }
   // Not being able to write it loses only the list.
   WriteDataToFilePath(_FR_FoldersFilePath(scratch.arena), Str8ListJoin(scratch.arena, &lines, 0));
   ScratchEnd(scratch);
}

internal void
FR_LoadFolders(App_State* app_state) {
   Temp    scratch = ScratchBegin(0, 0);
   String8 data = DataFromFilePath(scratch.arena, _FR_FoldersFilePath(scratch.arena));

   // Ones that are no longer there are left out: LIB_AddFolder takes only a
   // folder that is.
   U64 pos = 0;
   while (pos < data.size) {
      U64 line_start = pos;
      while (pos < data.size && data.str[pos] != '\n') pos += 1;
      String8 line = Str8SkipChopWhitespace(Str8Substr(data, R1U64(line_start, pos)));
      pos += 1;
      if (line.size != 0) {
         LIB_AddFolder(app_state->library, line);
      }
   }

   ScratchEnd(scratch);
}

internal void
FR_AddFolder(App_State* app_state, String8 path) {
   Temp    scratch = ScratchBegin(0, 0);
   String8 folder = FR_WholePath(scratch.arena, path);

   // A file stands for the folder it is in.
   FileProperties props = PropertiesFromFilePath(folder);
   if (folder.size != 0 && !(props.flags & FilePropertyFlag_IsFolder) && FilePathExists(folder)) {
      folder = Str8ChopLastSlash(folder);
   }

   if (LIB_AddFolder(app_state->library, folder)) {
      _FR_SaveFolders(app_state);
      FR_Notice(app_state, Str8Lit("Folder added."));
   } else if (PropertiesFromFilePath(folder).flags & FilePropertyFlag_IsFolder) {
      FR_Notice(app_state, Str8Lit("That folder is already being searched."));
   } else {
      LogUserErrorF("Not a folder: %S\n", folder);
      FR_Notice(app_state, Str8Lit("Not a folder, so not added."));
   }

   ScratchEnd(scratch);
}

////////////////////////////////
//~ bgz: input

// The search bar takes what is typed: there is nothing else on the screen to
// type into. Backspace takes off the last character, Esc empties it.
internal void
_FR_Input(App_State* app_state) {
   WM_EventList* events = &app_state->events;
   WM_Window     window = app_state->window;

   for (WM_Event* event = events->first; event != 0; event = event->next) {
      if (event->kind == WM_EventKind_WindowClose) {
         app_state->quit = 1;
      }
      // Of several things dropped at once, each is added.
      if (event->kind == WM_EventKind_FileDrop) {
         for (String8Node* node = event->strings.first; node != 0; node = node->next) {
            FR_AddFolder(app_state, node->string);
         }
      }
      // A character typed, not a key that only moves or edits.
      if (event->kind == WM_EventKind_Text && event->character >= 32 && event->character != 127) {
         Temp    scratch = ScratchBegin(0, 0);
         String8 text = Str8From32(scratch.arena, Str32(&event->character, 1));
         if (app_state->query_size + text.size <= APP_QUERY_CAP) {
            MemoryCopy(app_state->query + app_state->query_size, text.str, text.size);
            app_state->query_size += text.size;
         }
         ScratchEnd(scratch);
      }
   }

   // Backspace takes off a whole character: the bytes after its first are
   // 10xxxxxx in UTF-8.
   while (Core_WM_KeyPress(events, window, 0, WM_Key_Backspace) && app_state->query_size != 0) {
      app_state->query_size -= 1;
      while (app_state->query_size != 0 && (app_state->query[app_state->query_size] & 0xC0) == 0x80) {
         app_state->query_size -= 1;
      }
   }
   if (Core_WM_KeyPress(events, window, 0, WM_Key_Esc)) {
      app_state->query_size = 0;
   }
}

// Searches again if what was typed, or the videos, are not what the results
// are for. A search of names is quick enough to run on every key.
internal void
_FR_UpdateResults(App_State* app_state) {
   String8 query = Str8SkipChopWhitespace(Str8(app_state->query, app_state->query_size));
   B32     same_query = Str8Match(query, app_state->results_query, 0);
   B32     same_library = (app_state->results_library_gen == app_state->library->generation);
   B32     same_search = (app_state->results_search_gen == SRCH_Generation(app_state->search));

   if (!same_query || !same_library || !same_search) {
      ArenaClear(app_state->results_arena);
      app_state->results_query = PushStr8Copy(app_state->results_arena, query);
      app_state->results_library_gen = app_state->library->generation;
      app_state->results_search_gen = SRCH_Generation(app_state->search);
      app_state->results =
          SRCH_Run(app_state->results_arena, app_state->search, app_state->library, app_state->results_query);
      // More frames found while the query stays the same keep the list where
      // it was scrolled to.
      if (!same_query || !same_library) {
         MemoryZeroStruct(&app_state->results_scroll);
      }
   }
}

////////////////////////////////
//~ bgz: the UI layer

// This frame's window events as the UI's. Selects the UI's state first: the
// list is on UI_BuildArena, which is the selected state's, and lasts until
// the next UI_EndBuild.
internal UI_EventList
_FR_UIEventsFromEvents(App_State* app_state) {
   UI_SelectState(app_state->ui_state);

   UI_EventList ui_events = {0};
   for (WM_Event* event = app_state->events.first; event != 0; event = event->next) {
      UI_EventKind kind = UI_EventKind_Null;
      switch (event->kind) {
         default: {
         } break;
         case WM_EventKind_Press: {
            kind = UI_EventKind_Press;
         } break;
         case WM_EventKind_Release: {
            kind = UI_EventKind_Release;
         } break;
         case WM_EventKind_MouseMove: {
            kind = UI_EventKind_MouseMove;
         } break;
         case WM_EventKind_Text: {
            kind = UI_EventKind_Text;
         } break;
         case WM_EventKind_Scroll: {
            // Not a sideways wheel: with Shift held the UI turns one into an
            // up and down one, and a touchpad sends a steady trickle of them
            // (the editor's frame.c).
            if (event->delta.y != 0) {
               kind = UI_EventKind_Scroll;
            }         } break;
      }

      UI_Event ui_event = zero_struct;
      ui_event.kind = kind;
      ui_event.key = event->key;
      ui_event.modifiers = event->modifiers;
      ui_event.string = event->character ? Str8From32(UI_BuildArena(), Str32(&event->character, 1)) : Str8Zero();
      ui_event.pos = event->pos;
      ui_event.timestamp_us = event->timestamp_us;
      ui_event.delta_2f32 = event->delta;
      UI_EventListPush(UI_BuildArena(), &ui_events, &ui_event);
   }

   return ui_events;
}

internal F32
FR_UIScale(Vec2F32 window_dim) {
   return Clamp(1.0f, window_dim.y / 900.0f, 2.0f);
}

internal F32
FR_UIFontSize(F32 px) {
   // As text.c's _TXT_PointsFromPx: the UI hands its size to the same font
   // cache, which takes points, 72 to an inch where pixels are 96.
   return Max((F32)(S32)(px * 0.75f), 1.0f);
}

// Starts the frame's UI build: the theme, the icons, and the fonts every box
// starts from.
internal void
_FR_BeginUI(App_State* app_state, UI_EventList* ui_events, Vec2F32 window_dim) {
   // The colours, by tag. A box asks under the tags pushed with UI_ColorTag,
   // and the pattern with the most of them wins; a pattern counts only if
   // the box has all of its tags. The first part is the editor's palette
   // (ui_colors.h), for what the basic widgets ask for; the rest are the
   // program's own.
   static UI_ThemePattern theme_patterns[] = {
       //  tags          slot                      colour
       {"", UI_ColorSlot_Text, {{0.84f, 0.86f, 0.90f, 1.00f}}},
       {"", UI_ColorSlot_Background, UI_BG_1},
       {"", UI_ColorSlot_Border, UI_TRANSPARENT},
       {"", UI_ColorSlot_Selection, UI_ACCENT_BG},
       {"", UI_ColorSlot_Cursor, UI_FG},
       {"selected", UI_ColorSlot_Background, UI_ACCENT_BG},
       {"weak", UI_ColorSlot_Text, UI_FG_WEAK},
       {"soft", UI_ColorSlot_Text, UI_FG_SOFT},
       {"error", UI_ColorSlot_Text, UI_ERROR},
       {"warning", UI_ColorSlot_Text, UI_WARNING},
       {"highlight", UI_ColorSlot_Text, UI_HIGHLIGHT},
       {"button", UI_ColorSlot_Background, UI_BG_4},
       {"button", UI_ColorSlot_Text, UI_FG},
       {"popup", UI_ColorSlot_Background, UI_BG_3},
       {"popup", UI_ColorSlot_Border, UI_LINE},
       {"menu", UI_ColorSlot_Background, UI_TRANSPARENT},
       {"scroll track", UI_ColorSlot_Background, UI_TRANSPARENT},
       {"scroll thumb", UI_ColorSlot_Background, {{1.00f, 1.00f, 1.00f, 0.18f}}},

       //- bgz: the program's own: the search bar, the status line, the list
       {"panel", UI_ColorSlot_Background, {{0.11f, 0.115f, 0.135f, 1.00f}}},
       {"panel_bar", UI_ColorSlot_Background, {{0.16f, 0.17f, 0.20f, 1.00f}}}, // status line, foot
       {"search", UI_ColorSlot_Background, {{0.07f, 0.075f, 0.09f, 1.00f}}},   // the search bar's field
       {"search", UI_ColorSlot_Border, {{0.35f, 0.45f, 0.65f, 1.00f}}},
       {"row", UI_ColorSlot_Background, UI_TRANSPARENT},                       // a list's row: the panel shows through
       {"row hot", UI_ColorSlot_Background, {{1.00f, 1.00f, 1.00f, 0.07f}}},   // under the mouse
       {"dim", UI_ColorSlot_Text, {{0.84f, 0.86f, 0.90f, 0.55f}}},             // hints, folders, times
       {"notice", UI_ColorSlot_Text, {{1.00f, 0.85f, 0.50f, 1.00f}}},          // FR_Notice's
       {"video", UI_ColorSlot_Text, {{1.00f, 0.80f, 0.40f, 1.00f}}},           // a hit's file name
   };
   static UI_Theme theme = {theme_patterns, ArrayCount(theme_patterns)};

   // The UI's icons - scroll arrows, carets, check boxes - are single
   // characters of raddebugger's icon font, the ones raddbg uses.
   UI_IconInfo icon_info = {0};
   icon_info.icon_font = app_state->ui_icon_font;
   icon_info.icon_kind_text_map[UI_IconKind_RightArrow] = Str8Lit("8");
   icon_info.icon_kind_text_map[UI_IconKind_DownArrow] = Str8Lit("0");
   icon_info.icon_kind_text_map[UI_IconKind_LeftArrow] = Str8Lit("7");
   icon_info.icon_kind_text_map[UI_IconKind_UpArrow] = Str8Lit("9");
   icon_info.icon_kind_text_map[UI_IconKind_RightCaret] = Str8Lit(">");
   icon_info.icon_kind_text_map[UI_IconKind_DownCaret] = Str8Lit("v");
   icon_info.icon_kind_text_map[UI_IconKind_LeftCaret] = Str8Lit("<");
   icon_info.icon_kind_text_map[UI_IconKind_UpCaret] = Str8Lit("^");
   icon_info.icon_kind_text_map[UI_IconKind_CheckHollow] = Str8Lit("!");
   icon_info.icon_kind_text_map[UI_IconKind_CheckFilled] = Str8Lit("1");
   icon_info.icon_kind_text_map[UI_IconKind_WindowMinimize] = Str8Lit(")");
   icon_info.icon_kind_text_map[UI_IconKind_WindowMaximize] = Str8Lit("n");
   icon_info.icon_kind_text_map[UI_IconKind_WindowRestore] = Str8Lit("(");
   icon_info.icon_kind_text_map[UI_IconKind_WindowClose] = Str8Lit("x");
   icon_info.icon_kind_text_map[UI_IconKind_FolderClosed] = Str8Lit("C");
   icon_info.icon_kind_text_map[UI_IconKind_FolderOpen] = Str8Lit("B");
   icon_info.icon_kind_text_map[UI_IconKind_File] = Str8Lit("f");

   // The time since the last build, for hover fades and scroll glides. A
   // long gap - the first frame, or a wait inside a drag of the window -
   // counts as one frame at 60 a second, so nothing jumps.
   U64 now_us = NowTimeUS();
   F32 dt = 1.0f / 60.0f;
   if (app_state->ui_last_frame_us != 0 && now_us - app_state->ui_last_frame_us < 250000) {
      dt = (F32)(now_us - app_state->ui_last_frame_us) / 1000000.0f;
   }
   app_state->ui_last_frame_us = now_us;

   // Hover and press come in fast, as raddebugger's do: the scroll bar's thumb
   // widens under the mouse by them (_UI_DrawScrollThumb). The rest is not
   // used here.
   UI_AnimationInfo animation_info = {0};
   animation_info.hot_animation_rate = 1.0f - PowF32(2.0f, -60.0f * dt);
   animation_info.active_animation_rate = animation_info.hot_animation_rate;
   UI_BeginBuild(app_state->window, ui_events, &icon_info, &theme, &animation_info, dt, dt);

   // The regular font, 15 pixels at FR_UIScale 1, drawn as text.c draws it
   // (_TXT_RASTER_FLAGS). Parts push their own sizes and fonts where they
   // differ.
   FNT_Tag font = app_state->fonts.regular;
   UI_PushFont(font);
   UI_PushFontSize(FR_UIFontSize((F32)(S32)(15.0f * FR_UIScale(window_dim))));
   UI_PushTextRasterFlags(_TXT_RASTER_FLAGS);
   UI_SetLabelFont(font, _TXT_RASTER_FLAGS);

   // A ctx menu builds with the stack's bottom value, which is whatever was
   // last pushed on the empty stack. Pinned here, or a UI_PrefWidth(UI_Pct(..))
   // anywhere earlier in the frame makes the menu's root as wide as the window
   // (the editor's frame.c).
   UI_PushPrefWidth(UI_TopPrefWidth());
   UI_PushPrefHeight(UI_TopPrefHeight());
}

// Draws the UI's box tree into the draw bucket that is open, over whatever
// is already in it. The one place a box becomes draw commands, so a new
// UI_BoxFlag is handled once, here. The slideshow's _FR_DrawUITree, which is
// the editor's less its own Core_D_BeginFrame and bucket: the frame has one
// bucket for everything (FrameWork).
internal void
_FR_DrawUITree(App_State* app_state) {
   for (UI_Box* box = UI_RootFromState(app_state->ui_state); !UI_BoxIsNil(box);) {
      UI_BoxRec rec = UI_BoxRecDfPost(box, &ui_nil_box);

      // Under the box, down and right of it, soft. raddebugger's sizes. Not
      // for an empty box: the UI's own ctx menu root always has the flag.
      if ((box->flags & UI_BoxFlag_DrawDropShadow) && box->rect.x1 > box->rect.x0 && box->rect.y1 > box->rect.y0) {
         Vec4F32 shadow_color = {{0, 0, 0, 0.40f}};

         // A box that is fading takes its shadow with it.
         if (box->flags & UI_BoxFlag_DrawBackground) {
            shadow_color.w *= Min(1.0f, box->background_color.w);
         }
         Rng2F32       shadow_rect = R2F32P(box->rect.x0 - 6, box->rect.y0 - 2, box->rect.x1 + 6, box->rect.y1 + 10);
         R_Rect2DInst* inst = Core_D_Rect(shadow_rect, R2F32(V2F32(0, 0), V2F32(1, 1)), shadow_color);
         inst->edge_softness = 12.0f;
         for EachEnumVal(Corner, corner) {
            inst->corner_radii[corner] = box->corner_radii[corner] + 6.0f;
         }
      }

      if (box->flags & UI_BoxFlag_DrawBackground) {
         R_Rect2DInst* inst = Core_D_Rect(box->rect, R2F32(V2F32(0, 0), V2F32(1, 1)), box->background_color);
         MemoryCopyArray(inst->corner_radii, box->corner_radii);
      }

      // A pixel outside the rect, so a rounded corner's is that much wider.
      if (box->flags & UI_BoxFlag_DrawBorder) {
         R_Rect2DInst* inst = Core_D_Rect(Pad2F32(box->rect, 1.0f), R2F32(V2F32(0, 0), V2F32(1, 1)), box->border_color);
         inst->border_thickness = 1.0f;
         for EachEnumVal(Corner, corner) {
            inst->corner_radii[corner] = box->corner_radii[corner] > 0 ? box->corner_radii[corner] + 1.0f : 0;
         }
      }

      if (box->flags & UI_BoxFlag_DrawText) {
         Vec2F32 text_pos = UI_BoxTextPosition(box);
         F32     max_x = box->rect.x1 - text_pos.x;
         FNT_Run ellipses_run = {0};
         Core_D_TruncatedFancyRunList(text_pos, &box->display_fruns, max_x, ellipses_run);
      }

      if (box->custom_draw != 0) {
         box->custom_draw(box, box->custom_draw_user_data);
      }

      // A UI_BoxFlag_Clip box keeps everything under it inside its rect, and
      // inside any clip above it. Pushed here, on the way down to its
      // children, and popped below once the walk has left them.
      if (box->flags & UI_BoxFlag_Clip) {
         Rng2F32 top_clip = Core_D_TopClip();
         Rng2F32 new_clip = box->rect;

         // A zero clip is "none", not a rect at the origin to intersect with.
         if (top_clip.x1 != 0 || top_clip.y1 != 0) {
            new_clip = Intersect2F32(new_clip, top_clip);
         }
         Core_D_PushClip(new_clip);
      }

      // Pop the clip of every box this step finishes: this one, unless it has
      // children still to draw (the last of them pops it), and each ancestor
      // the walk climbs out of - rec.pop_count of them.
      {
         S32 pop_idx = 0;
         for (UI_Box* b = box; !UI_BoxIsNil(b) && pop_idx <= rec.pop_count; b = b->parent) {
            pop_idx += 1;
            if (b == box && rec.push_count != 0) {
               continue;
            }
            if (b->flags & UI_BoxFlag_Clip) {
               Core_D_PopClip();
            }
         }
      }

      box = rec.next;
   }
}

////////////////////////////////
//~ bgz: the screen

// A bar the width of rect, its one line of text pad in from the left.
internal void
_FR_BuildBar(Rng2F32 rect, F32 pad, String8 color_tag, String8 text, char* key) {
   UI_SetNextPrefWidth(UI_Px(Dim2F32(rect).x, 1));
   UI_SetNextPrefHeight(UI_Px(Dim2F32(rect).y, 1));
   UI_SetNextChildLayoutAxis(Axis2_X);
   UI_SetNextColorTag(Str8Lit("panel_bar"));
   UI_Box* bar = UI_BuildBoxFromStringf(UI_BoxFlag_DrawBackground | UI_BoxFlag_Clip, "###%s", key);
   UI_Parent(bar) UI_PrefHeight(UI_Pct(1, 1)) {
      UI_Spacer(UI_Px(pad, 1));
      UI_SetNextPrefWidth(UI_Pct(1, 0));
      UI_SetNextColorTag(color_tag);
      UI_Label(text);
   }
}

internal void
_FR_BuildScreen(App_State* app_state, Vec2F32 window_dim) {
   F32  ui = FR_UIScale(window_dim);
   F32  pad = 18.0f * ui;
   F32  row_h = (F32)(S32)(30.0f * ui);
   F32  search_h = (F32)(S32)(64.0f * ui);
   F32  bar_h = (F32)(S32)(32.0f * ui);
   Temp scratch = ScratchBegin(0, 0);

   LIB_Library*  library = app_state->library;
   SRCH_Results* results = &app_state->results;

   UI_SetNextRect(R2F32P(0, 0, window_dim.x, window_dim.y));
   UI_SetNextChildLayoutAxis(Axis2_Y);
   UI_SetNextColorTag(Str8Lit("panel"));
   UI_Box* panel = UI_BuildBoxFromStringf(UI_BoxFlag_DrawBackground | UI_BoxFlag_Clip, "###screen");

   UI_Parent(panel) {
      //- bgz: the search bar: what is typed, with a caret after it - or, with
      // nothing typed, what can be.
      UI_SetNextPrefWidth(UI_Px(window_dim.x, 1));
      UI_SetNextPrefHeight(UI_Px(search_h, 1));
      UI_SetNextChildLayoutAxis(Axis2_X);
      UI_Box* search_row = UI_BuildBoxFromStringf(0, "###search row");
      UI_Parent(search_row) {
         UI_Spacer(UI_Px(pad, 1));
         UI_SetNextPrefWidth(UI_Px(Max(window_dim.x - pad * 2.0f, 0), 1));
         UI_SetNextPrefHeight(UI_Px(search_h - pad, 1));
         UI_SetNextChildLayoutAxis(Axis2_X);
         UI_SetNextColorTag(Str8Lit("search"));
         UI_Box* field = UI_BuildBoxFromStringf(UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawBorder | UI_BoxFlag_Clip,
                                                "###search field");
         UI_Parent(field) UI_PrefHeight(UI_Pct(1, 1)) UI_FontSize(FR_UIFontSize((F32)(S32)(20.0f * ui))) {
            UI_Spacer(UI_Px(12.0f * ui, 1));
            UI_SetNextPrefWidth(UI_Pct(1, 0));
            if (app_state->query_size != 0) {
               UI_Label(PushStr8F(scratch.arena, "%S|", Str8(app_state->query, app_state->query_size)));
            } else {
               UI_SetNextColorTag(Str8Lit("dim"));
               UI_Label(Str8Lit("Search your videos: a man screaming, a car driving down a highway..."));
            }
         }
      }

      //- bgz: the status line: a notice if one is up, else what there is
      String8 notice = FR_CurrentNotice(app_state);
      String8 status = {0};
      if (notice.size != 0) {
         status = notice;
      } else if (library->folders.node_count == 0) {
         status = Str8Lit("No videos yet. Drop a folder of videos on the window to search it.");
      } else {
         String8 hits = (app_state->results_query.size == 0)
                            ? Str8Lit("")
                            : PushStr8F(scratch.arena, "%llu hits. ", (unsigned long long)results->count);
         // How far the indexer has got: the video under way, or all done.
         IDX_Status idx = IDX_GetStatus(app_state->indexer);
         String8    reading = {0};
         if (idx.no_model) {
            reading = Str8Lit(" No image model, so nothing is indexed: see data\\models\\README.md.");
         } else if (idx.videos_done < idx.video_count) {
            reading = PushStr8F(scratch.arena, " Indexing: %llu of %llu videos, %llu frames (%S).",
                                (unsigned long long)idx.videos_done, (unsigned long long)idx.video_count,
                                (unsigned long long)idx.frames, idx.current);
         } else if (idx.video_count != 0) {
            reading = PushStr8F(scratch.arena, " %llu frames indexed.", (unsigned long long)idx.frames);
         }
         String8 failed = {0};
         if (idx.videos_failed != 0) {
            failed = PushStr8F(scratch.arena, " %llu could not be indexed.", (unsigned long long)idx.videos_failed);
         }
         String8 by_name = results->by_name ? Str8Lit(" Matched by file name: no frames to search yet.") : Str8Lit("");
         status = PushStr8F(scratch.arena, "%S%llu videos in %llu folders.%S%S%S", hits,
                            (unsigned long long)library->video_count, (unsigned long long)library->folders.node_count,
                            reading, failed, by_name);
      }
      _FR_BuildBar(R2F32P(0, 0, window_dim.x, bar_h), pad, (notice.size != 0) ? Str8Lit("notice") : Str8Lit("dim"), status,
                   "status");

      //- bgz: the hits
      F32                rows_h = Max(window_dim.y - search_h - bar_h * 2.0f, 0);
      UI_ScrollRowsFlags scroll_flags = UI_ScrollRowsFlag_RowClicks;
      UI_ScrollRowsFrame rows_frame = UI_ScrollRowsBegin(&app_state->results_scroll, V2F32(window_dim.x, rows_h),
                                                         (S64)results->count, row_h, 0, scroll_flags, "###hits");
      UI_ScrollRowsEnd(&app_state->results_scroll, &rows_frame, 0, scroll_flags);

      UI_Parent(rows_frame.rows_box) UI_ColorTag(Str8Lit("row")) {
         for (S64 row_idx = rows_frame.first_row; row_idx < rows_frame.last_row; row_idx += 1) {
            SRCH_Hit*  hit = &results->v[row_idx];
            LIB_Video* video = &library->videos[hit->video_idx];

            UI_Box* row_box = UI_ListRow(row_h, 0, UI_BoxFlag_Clickable | UI_BoxFlag_DrawBackground, "###hit %lld", row_idx);

            // A frame's hit says how well it matched, for telling a good match
            // from a poor one: CLIP's scores are about 0.2 to 0.35.
            String8 when = Str8Lit("whole video");
            if (hit->end_s > hit->start_s) {
               U64 s = (U64)hit->start_s;
               when = PushStr8F(scratch.arena, "at %llu:%02llu   %.2f", (unsigned long long)(s / 60), (unsigned long long)(s % 60),
                                hit->score);
            }

            UI_Parent(row_box) UI_PrefHeight(UI_Pct(1, 1)) {
               UI_Spacer(UI_Px(pad + 22.0f * ui, 1));
               UI_SetNextPrefWidth(UI_TextDim(1, 1));
               UI_SetNextColorTag(Str8Lit("video"));
               UI_Label(video->name);
               UI_Spacer(UI_Px(18.0f * ui, 1));
               UI_SetNextPrefWidth(UI_TextDim(1, 1));
               UI_SetNextColorTag(Str8Lit("dim"));
               UI_Label(when);
               UI_Spacer(UI_Px(18.0f * ui, 1));
               UI_SetNextPrefWidth(UI_TextDim(1, 0));
               UI_SetNextColorTag(Str8Lit("dim"));
               UI_Label(video->folder);
            }

            UI_Signal signal = UI_SignalFromBox(row_box);
            if (UI_Hovering(signal)) {
               row_box->background_color = UI_ThemeColor(Str8Lit("hot"), UI_ColorSlot_Background);
            }
         }
      }

      //- bgz: the foot: what can be done
      _FR_BuildBar(R2F32P(0, 0, window_dim.x, bar_h), pad, Str8Lit("dim"),
                   Str8Lit("Type to search. Backspace takes a letter off, Esc empties it. Drop a folder of videos to add it."),
                   "foot");
   }

   ScratchEnd(scratch);
}

////////////////////////////////
//~ bgz: the frame

// The font cache's turn of the frame, before any text is measured or drawn.
internal void
_FR_BeginText(App_State* app_state, Vec2F32 window_dim) {
   // Text is drawn at sizes that follow the window's (FR_UIScale), and the font
   // cache keeps every glyph of every size it has drawn, in textures. Left
   // alone, dragging the window's edge would fill it with sizes never used
   // again. So a new window size empties it: what this frame draws is put
   // back in as it is drawn.
   if (window_dim.x != app_state->text_window_dim.x || window_dim.y != app_state->text_window_dim.y) {
      app_state->text_window_dim = window_dim;
      Core_FNT_Reset();
   }

   // Lets go of the last frame's measured strings.
   Core_FNT_Frame();
}

internal void
FrameWork(App_State* app_state) {
   // What is logged this frame is gathered, and written at the frame's end.
   LogSelect(app_state->log);
   LogScopeBegin();

   Vec2F32 window_dim = Dim2F32(Core_WM_ClientRectFromWindow(app_state->window));

   _FR_BeginText(app_state, window_dim);
   _FR_Input(app_state);
   IDX_Update(app_state->indexer, app_state->library);
   SRCH_Update(app_state->search, app_state->library, app_state->indexer);
   _FR_UpdateResults(app_state);

   // The UI tree, built after the input. UI_EndBuild lays it out; it is
   // drawn below.
   {
      UI_EventList ui_events = _FR_UIEventsFromEvents(app_state);
      _FR_BeginUI(app_state, &ui_events, window_dim);

      // What is shown is text as it is - names, paths - never a box's key:
      // the UI would show only what is before a ##.
      UI_FlagsAdd(UI_BoxFlag_DisableIDString) {
         _FR_BuildScreen(app_state, window_dim);
      }

      UI_EndBuild();
   }

   Core_D_BeginFrame();
   D_Bucket* bucket = Core_D_BucketMake();
   Core_D_BucketScope(bucket) {
      _FR_DrawUITree(app_state);
   }
   Core_D_SubmitBucket(app_state->window, app_state->window_eqp, bucket);

   FR_WriteLogs(app_state, LogScopeEnd(app_state->frame_arena));
}
