// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rjf: Globals

thread_static UI_State* ui_state = 0;

////////////////////////////////
//~ rjf: Basic Type Functions

String8
UI_HashPartFromKeyString(String8 string) {
   String8 result = string;

   // rjf: look for ### patterns, which can replace the entirety of the part of
   // the string that is hashed.
   U64 hash_replace_signifier_pos = Str8FindNeedle(string, 0, Str8Lit("###"), 0);
   if (hash_replace_signifier_pos < string.size) {
      result = Str8Skip(string, hash_replace_signifier_pos);
   }

   return result;
}

String8
UI_DisplayPartFromKeyString(String8 string) {
   U64 hash_pos = Str8FindNeedle(string, 0, Str8Lit("##"), 0);
   string.size = hash_pos;
   return string;
}

UI_Key
UI_KeyZero(void) {
   UI_Key result = {0};
   return result;
}

UI_Key
UI_KeyMake(U64 v) {
   UI_Key result = {v};
   return result;
}

UI_Key
UI_KeyFromString(UI_Key seed_key, String8 string) {
   UI_Key result = {0};
   if (string.size != 0) {
      String8 hash_part = UI_HashPartFromKeyString(string);
      result.u64[0] = U64HashFromSeedStr8(seed_key.u64[0], hash_part);
   }
   return result;
}

UI_Key
UI_KeyFromStringf(UI_Key seed_key, char* fmt, ...) {
   Temp    scratch = ScratchBegin(0, 0);
   va_list args;
   va_start(args, fmt);
   String8 string = PushStr8FV(scratch.arena, fmt, args);
   va_end(args);
   UI_Key key = UI_KeyFromString(seed_key, string);
   ScratchEnd(scratch);
   return key;
}

B32 UI_KeyMatch(UI_Key a, UI_Key b) {
   return a.u64[0] == b.u64[0];
}

////////////////////////////////
//~ rjf: Event Type Functions

UI_EventNode*
UI_EventListPush(Arena* arena, UI_EventList* list, UI_Event* v) {
   UI_EventNode* n = PushArray(arena, UI_EventNode, 1);
   MemoryCopyStruct(&n->v, v);
   n->v.string = PushStr8Copy(arena, n->v.string);
   DLLPushBack(list->first, list->last, n);
   list->count += 1;
   return n;
}

void UI_EatEventNode(UI_EventList* list, UI_EventNode* node) {
   DLLRemove(list->first, list->last, node);
   list->count -= 1;
}

////////////////////////////////
//~ rjf: Text Operation Functions

B32 UI_CharIsScanBoundary(U8 c) {
   return (CharIsAlpha(c) || CharIsDigit(c, 10) || c == '_');
}

S64 UI_ScannedColumnFromColumn(String8 string, S64 start_column, Side side) {
   S64 new_column = start_column;
   S64 delta = (!!side) * 2 - 1;
   B32 found_text = 0;
   B32 found_non_space = 0;
   S64 start_off = delta < 0 ? delta : 0;
   for (S64 col = start_column + start_off; 1 <= col && col <= (S64)string.size + 1; col += delta) {
      U8  byte = (col <= (S64)string.size) ? string.str[col - 1] : 0;
      B32 is_non_space = !CharIsSpace(byte);
      B32 is_name = UI_CharIsScanBoundary(byte);
      if (((side == Side_Min) && (col == 1)) ||
          ((side == Side_Max) && (col == (S64)string.size + 1)) ||
          (found_non_space && !is_non_space) ||
          (found_text && !is_name)) {
         new_column = col + (!side && col != 1);
         break;
      }
      else if (!found_text && is_name) {
         found_text = 1;
      }
      else if (!found_non_space && is_non_space) {
         found_non_space = 1;
      }
   }
   return new_column;
}

UI_TxtOp
UI_SingleLineTxtOpFromEvent(Arena* arena, UI_Event* event, String8 string, TxtPt cursor, TxtPt mark) {
   TxtPt         next_cursor = cursor;
   TxtPt         next_mark = mark;
   TxtRng        range = {0};
   String8       replace = {0};
   String8       copy = {0};
   UI_TxtOpFlags flags = 0;
   Vec2S32       delta = event->delta_2s32;
   Vec2S32       original_delta = delta;

   //- rjf: resolve high-level delta into byte delta, based on unit
   switch (event->delta_unit) {
   default: {
   } break;
   case UI_EventDeltaUnit_Char: {
      // TODO(rjf): this should account for multi-byte characters in UTF-8... for now, just assume ASCII and
      // no-op
   } break;
   case UI_EventDeltaUnit_Word: {
      delta.x = (S32)UI_ScannedColumnFromColumn(string, cursor.column, delta.x > 0 ? Side_Max : Side_Min) - cursor.column;
   } break;
   case UI_EventDeltaUnit_Line:
   case UI_EventDeltaUnit_Whole:
   case UI_EventDeltaUnit_Page: {
      S64 first_nonwhitespace_column = 1;
      for (U64 idx = 0; idx < string.size; idx += 1) {
         if (!CharIsSpace(string.str[idx])) {
            first_nonwhitespace_column = (S64)idx + 1;
            break;
         }
      }
      S64 home_dest_column = (cursor.column == first_nonwhitespace_column) ? 1 : first_nonwhitespace_column;
      delta.x = (delta.x > 0) ? ((S64)string.size + 1 - cursor.column) : (home_dest_column - cursor.column);
   } break;
   }

   //- rjf: zero delta
   if (!TxtPtMatch(cursor, mark) && event->flags & UI_EventFlag_ZeroDeltaOnSelect) {
      delta = V2S32(0, 0);
   }

   //- rjf: form next cursor
   if (TxtPtMatch(cursor, mark) || !(event->flags & UI_EventFlag_ZeroDeltaOnSelect)) {
      next_cursor.column += delta.x;
   }

   //- rjf: cap at line
   if (event->flags & UI_EventFlag_CapAtLine) {
      next_cursor.column = Clamp(1, next_cursor.column, (S64)(string.size + 1));
   }

   //- rjf: in some cases, we want to pick a selection side based on the delta
   if (!TxtPtMatch(cursor, mark) && event->flags & UI_EventFlag_PickSelectSide) {
      if (original_delta.x < 0 || original_delta.y < 0) {
         next_cursor = next_mark = TxtPtMin(cursor, mark);
      }
      else if (original_delta.x > 0 || original_delta.y > 0) {
         next_cursor = next_mark = TxtPtMax(cursor, mark);
      }
   }

   //- rjf: copying
   if (event->flags & UI_EventFlag_Copy) {
      if (cursor.line == mark.line) {
         copy = Str8Substr(string, R1U64(cursor.column - 1, mark.column - 1));
         flags |= UI_TxtOpFlag_Copy;
      }
      else {
         flags |= UI_TxtOpFlag_Invalid;
      }
   }

   //- rjf: pasting
   if (event->flags & UI_EventFlag_Paste) {
      range = TxtRngMake(cursor, mark);
      replace = Core_WM_GetClipboardText(arena);
      next_cursor = next_mark = TxtPtMake(cursor.line, cursor.column + replace.size);
   }

   //- rjf: deletion
   if (event->flags & UI_EventFlag_Delete) {
      TxtPt new_pos = TxtPtMin(next_cursor, next_mark);
      range = TxtRngMake(next_cursor, next_mark);
      replace = Str8Lit("");
      next_cursor = next_mark = new_pos;
   }

   //- rjf: stick mark to cursor, when we don't want to keep it in the same spot
   if (!(event->flags & UI_EventFlag_KeepMark)) {
      next_mark = next_cursor;
   }

   //- rjf: insertion
   if (event->string.size != 0) {
      range = TxtRngMake(cursor, mark);
      replace = PushStr8Copy(arena, event->string);
      next_cursor = next_mark = TxtPtMake(range.min.line, range.min.column + event->string.size);
   }

   //- rjf: determine if this event should be taken, based on bounds of cursor
   {
      if (next_cursor.column > (S64)string.size + 1 || 1 > next_cursor.column || event->delta_2s32.y != 0) {
         flags |= UI_TxtOpFlag_Invalid;
      }
      next_cursor.column = Clamp(1, next_cursor.column, (S64)(string.size + replace.size + 1));
      next_mark.column = Clamp(1, next_mark.column, (S64)(string.size + replace.size + 1));
   }

   //- rjf: build+fill
   UI_TxtOp op = {0};
   {
      op.flags = flags;
      op.replace = replace;
      op.copy = copy;
      op.range = range;
      op.cursor = next_cursor;
      op.mark = next_mark;
   }
   return op;
}

String8
UI_PushStringReplaceRange(Arena* arena, String8 string, Rng1S64 col_range, String8 replace) {
   //- rjf: convert to offset range
   Rng1U64 range =
       {{
           (U64)(col_range.min - 1),
           (U64)(col_range.max - 1),
       }};

   //- rjf: clamp range
   if (range.min > string.size) {
      range.min = 0;
   }
   if (range.max > string.size) {
      range.max = string.size;
   }

   //- rjf: calculate new size
   U64 old_size = string.size;
   U64 new_size = old_size - (range.max - range.min) + replace.size;

   //- rjf: push+fill new string storage
   U8* push_base = PushArray(arena, U8, new_size);
   {
      MemoryCopy(push_base + 0, string.str, range.min);
      MemoryCopy(push_base + range.min + replace.size, string.str + range.max, string.size - range.max);
      if (replace.str != 0) {
         MemoryCopy(push_base + range.min, replace.str, replace.size);
      }
   }

   String8 result = Str8(push_base, new_size);
   return result;
}

////////////////////////////////
//~ rjf: Sizes

UI_Size
UI_SizeMake(UI_SizeKind kind, F32 value, F32 strictness) {
   UI_Size size = {kind, value, strictness};
   return size;
}

////////////////////////////////
//~ rjf: Scroll Point Type Functions

UI_ScrollPt
UI_ScrollPtMake(S64 idx, F32 off) {
   UI_ScrollPt pt = {idx, off};
   return pt;
}

void UI_ScrollPtTargetIdx(UI_ScrollPt* v, S64 idx) {
   v->off = ModF32(v->off, 1.f) + (F32)(v->idx + (S64)v->off - idx);
   v->idx = idx;
}

void UI_ScrollPtClampIdx(UI_ScrollPt* v, Rng1S64 range) {
   if (v->idx < range.min || range.max < v->idx) {
      S64 clamped = range.min;
      UI_ScrollPtTargetIdx(v, clamped);
   }
}

////////////////////////////////
//~ rjf: Boxes

B32 UI_BoxIsNil(UI_Box* box) {
   return box == 0 || box == &ui_nil_box;
}

UI_BoxRec
UI_BoxRecDf(UI_Box* box, UI_Box* root, U64 sib_member_off, U64 child_member_off) {
   UI_BoxRec result = {0};
   result.next = &ui_nil_box;
   if (!UI_BoxIsNil(*MemberFromOffset(UI_Box**, box, child_member_off))) {
      result.next = *MemberFromOffset(UI_Box**, box, child_member_off);
      result.push_count = 1;
   }
   else
      for (UI_Box* p = box; !UI_BoxIsNil(p) && p != root; p = p->parent) {
         if (!UI_BoxIsNil(*MemberFromOffset(UI_Box**, p, sib_member_off))) {
            result.next = *MemberFromOffset(UI_Box**, p, sib_member_off);
            break;
         }
         result.pop_count += 1;
      }
   return result;
}

void UI_BoxListPush(Arena* arena, UI_BoxList* list, UI_Box* box) {
   UI_BoxNode* n = PushArray(arena, UI_BoxNode, 1);
   n->box = box;
   SLLQueuePush(list->first, list->last, n);
   list->count += 1;
}

////////////////////////////////
//~ rjf: State Building / Selecting

UI_State*
UI_StateAlloc(void) {
   Arena*    arena = ArenaAlloc();
   UI_State* ui = PushArray(arena, UI_State, 1);
   ui->arena = arena;
   ui->external_key = UI_KeyFromString(UI_KeyZero(), Str8Lit("###external_interaction_key###"));
   ui->build_arenas[0] = ArenaAlloc();
   ui->build_arenas[1] = ArenaAlloc();
   ui->drag_state_arena = ArenaAlloc();
   ui->string_hover_arena = ArenaAlloc();
   ui->box_table_size = 4096;
   ui->box_table = PushArray(arena, UI_BoxHashSlot, ui->box_table_size);
   ui->anim_slots_count = 4096;
   ui->anim_slots = PushArray(arena, UI_AnimSlot, ui->anim_slots_count);
   ui->theme_pattern_cache_slots_count = 1024;
   ui->theme_pattern_cache_slots = PushArray(arena, UI_ThemePatternCacheSlot, ui->theme_pattern_cache_slots_count);
   UI_InitStackNils(ui);
   return ui;
}

void UI_StateRelease(UI_State* state) {
   ArenaRelease(state->string_hover_arena);
   ArenaRelease(state->drag_state_arena);
   for (U64 i = 0; i < ArrayCount(state->build_arenas); i += 1) {
      ArenaRelease(state->build_arenas[i]);
   }
   ArenaRelease(state->arena);
}

UI_Box*
UI_RootFromState(UI_State* state) {
   return state->root;
}

B32 UI_AnimatingFromState(UI_State* state) {
   return state->is_animating;
}

void UI_SelectState(UI_State* state) {
   ui_state = state;
}

UI_State*
UI_GetSelectedState(void) {
   return ui_state;
}

////////////////////////////////
//~ rjf: Implicit State Accessors/Mutators

//- rjf: per-frame info

Arena*
UI_BuildArena(void) {
   Arena* result = ui_state->build_arenas[ui_state->build_index % ArrayCount(ui_state->build_arenas)];
   return result;
}

WM_Window
UI_Window(void) {
   return ui_state->window;
}

Vec2F32
UI_Mouse(void) {
   return ui_state->mouse;
}

FNT_Tag
UI_IconFont(void) {
   return ui_state->icon_info.icon_font;
}

void UI_SetLabelFont(FNT_Tag font, FNT_RasterFlags raster_flags) {
   ui_state->label_font         = font;
   ui_state->label_raster_flags = raster_flags;
}

FNT_Tag UI_LabelFont(void) {
   return ui_state->label_font;
}

FNT_RasterFlags UI_LabelRasterFlags(void) {
   return ui_state->label_raster_flags;
}

String8
UI_IconStringFromKind(UI_IconKind icon_kind) {
   return ui_state->icon_info.icon_kind_text_map[icon_kind];
}

F32 UI_Dt(void) {
   return ui_state->animation_dt;
}

//- rjf: event pumping

B32 UI_NextEvent(UI_Event** ev) {
   UI_EventList* events = ui_state->events;
   UI_EventNode* start_node = events->first;
   if (ev[0] != 0) {
      start_node = CastFromMember(UI_EventNode, v, ev[0]);
      start_node = start_node->next;
      ev[0] = 0;
   }
   if (start_node != 0) {
      UI_PermissionFlags perms = UI_TopPermissionFlags();
      for (UI_EventNode* n = start_node; n != 0; n = n->next) {
         B32 good = 1;
         if (!(perms & UI_PermissionFlag_ClicksLeft) &&
             (n->v.kind == UI_EventKind_Press ||
              n->v.kind == UI_EventKind_Release) &&
             (n->v.key == WM_Key_LeftMouseButton)) {
            good = 0;
         }
         if (!(perms & UI_PermissionFlag_ClicksMiddle) &&
             (n->v.kind == UI_EventKind_Press ||
              n->v.kind == UI_EventKind_Release) &&
             (n->v.key == WM_Key_MiddleMouseButton)) {
            good = 0;
         }
         if (!(perms & UI_PermissionFlag_ClicksRight) &&
             (n->v.kind == UI_EventKind_Press ||
              n->v.kind == UI_EventKind_Release) &&
             (n->v.key == WM_Key_RightMouseButton)) {
            good = 0;
         }
         if (!(perms & UI_PermissionFlag_ScrollX) && (n->v.kind == UI_EventKind_Scroll) && (n->v.delta_2f32.x != 0 || n->v.modifiers == WM_Modifier_Shift)) {
            good = 0;
         }
         if (!(perms & UI_PermissionFlag_ScrollY) && (n->v.kind == UI_EventKind_Scroll) && n->v.delta_2f32.y != 0 && n->v.modifiers == 0) {
            good = 0;
         }
         if ((n->v.kind == UI_EventKind_Press ||
              n->v.kind == UI_EventKind_Release ||
              n->v.kind == UI_EventKind_Navigate ||
              n->v.kind == UI_EventKind_Edit) &&
             (n->v.key != WM_Key_LeftMouseButton &&
              n->v.key != WM_Key_MiddleMouseButton &&
              n->v.key != WM_Key_RightMouseButton)) {
            if ((perms & UI_PermissionFlag_Keyboard) == UI_PermissionFlag_KeyboardSecondary) {
               good = !!(n->v.flags & UI_EventFlag_Secondary);
            }
            else if (!(perms & UI_PermissionFlag_Keyboard)) {
               good = 0;
            }
         }
         else if (!(perms & UI_PermissionFlag_Text) && (n->v.kind == UI_EventKind_Text)) {
            good = 0;
         }
         if (good) {
            ev[0] = &n->v;
            break;
         }
      }
   }
   B32 result = !!ev[0];
   return result;
}

void UI_EatEvent(UI_Event* ev) {
   if (ev != 0) {
      UI_EventNode* n = CastFromMember(UI_EventNode, v, ev);
      UI_EatEventNode(ui_state->events, n);
   }
}

//- rjf: event consumption helpers

B32 UI_KeyPress(WM_Modifiers mods, WM_Key key) {
   B32 result = 0;
   for (UI_Event* evt = 0; UI_NextEvent(&evt);) {
      if (evt->kind == UI_EventKind_Press && evt->key == key && evt->modifiers == mods) {
         result = 1;
         UI_EatEvent(evt);
         break;
      }
   }
   return result;
}

B32 UI_KeyRelease(WM_Modifiers mods, WM_Key key) {
   B32 result = 0;
   for (UI_Event* evt = 0; UI_NextEvent(&evt);) {
      if (evt->kind == UI_EventKind_Release && evt->key == key && evt->modifiers == mods) {
         result = 1;
         UI_EatEvent(evt);
         break;
      }
   }
   return result;
}

B32 UI_Text(U32 character) {
   B32     result = 0;
   Temp    scratch = ScratchBegin(0, 0);
   String8 character_text = Str8From32(scratch.arena, Str32(&character, 1));
   for (UI_Event* evt = 0; UI_NextEvent(&evt);) {
      if (evt->kind == UI_EventKind_Text && Str8Match(character_text, evt->string, 0)) {
         result = 1;
         UI_EatEvent(evt);
         break;
      }
   }
   ScratchEnd(scratch);
   return result;
}

B32 UI_SlotPress(UI_EventActionSlot slot) {
   B32 result = 0;
   for (UI_Event* evt = 0; UI_NextEvent(&evt);) {
      if (evt->kind == UI_EventKind_Press && evt->slot == slot) {
         result = 1;
         UI_EatEvent(evt);
         break;
      }
   }
   return result;
}

//- rjf: autocomplete info

void UI_SetAutocompleteString(String8 string) {
   ui_state->autocomplete_string = PushStr8Copy(UI_BuildArena(), string);
}

String8
UI_AutocompleteString(void) {
   return ui_state->autocomplete_string;
}

String8
UI_Autocomplete(void) {
   String8 result = ui_state->autocomplete_string;
   MemoryZeroStruct(&ui_state->autocomplete_string);
   return result;
}

//- rjf: drag data

Vec2F32
UI_DragStartMouse(void) {
   return ui_state->drag_start_mouse;
}

Vec2F32
UI_DragDelta(void) {
   return Sub2F32(UI_Mouse(), ui_state->drag_start_mouse);
}

void UI_StoreDragData(String8 string) {
   ArenaClear(ui_state->drag_state_arena);
   ui_state->drag_state_data = PushStr8Copy(ui_state->drag_state_arena, string);
}

String8
UI_GetDragData(U64 min_required_size) {
   if (ui_state->drag_state_data.size < min_required_size) {
      Temp    scratch = ScratchBegin(0, 0);
      String8 str = {PushArray(scratch.arena, U8, min_required_size), min_required_size};
      UI_StoreDragData(str);
      ScratchEnd(scratch);
   }
   return ui_state->drag_state_data;
}

//- rjf: hovered string info

B32 UI_StringHoverActive(void) {
   return (ui_state->build_index > 0 && ui_state->string_hover_build_index >= ui_state->build_index - 1 &&
           NowTimeUS() >= ui_state->string_hover_begin_us + 500000);
}

U64 UI_StringHoverBeginTimeUs(void) {
   return ui_state->string_hover_begin_us;
}

DR_FStrList
UI_StringHoverFstrs(Arena* arena) {
   DR_FStrList result = Core_D_FStrs_Copy(arena, &ui_state->string_hover_fstrs);
   return result;
}

//- rjf: interaction keys

UI_Key
UI_HotKey(void) {
   return ui_state->hot_box_key;
}

UI_Key
UI_ActiveKey(UI_MouseButtonKind button_kind) {
   return ui_state->active_box_key[button_kind];
}

UI_Key
UI_DropHotKey(void) {
   return ui_state->drop_hot_box_key;
}

//- rjf: controls over interaction

void UI_KillAction(void) {
   for EachEnumVal(UI_MouseButtonKind, k) {
      ui_state->active_box_key[k] = UI_KeyZero();
   }
}

//- rjf: box cache lookup

UI_Box*
UI_BoxFromKey(UI_Key key) {
   ProfBeginFunction();
   UI_Box* result = &ui_nil_box;
   if (!UI_KeyMatch(key, UI_KeyZero())) {
      U64 slot = key.u64[0] % ui_state->box_table_size;
      for (UI_Box* b = ui_state->box_table[slot].hash_first; !UI_BoxIsNil(b); b = b->hash_next) {
         if (UI_KeyMatch(b->key, key)) {
            result = b;
            break;
         }
      }
   }
   ProfEnd();
   return result;
}

////////////////////////////////
//~ rjf: Top-Level Building API

void UI_BeginBuild(WM_Window window, UI_EventList* events, UI_IconInfo* icon_info, UI_Theme* theme, UI_AnimationInfo* animation_info, F32 real_dt, F32 animation_dt) {
   UnusedVariable(real_dt);
   //- rjf: reset per-build ui state
   {
      UI_InitStacks(ui_state);
      ui_state->root = &ui_nil_box;
      ui_state->ctx_menu_touched_this_frame = 0;
      ui_state->is_animating = 0;
      ui_state->clipboard_copy_key = UI_KeyZero();
      ui_state->last_build_box_count = ui_state->build_box_count;
      ui_state->build_box_count = 0;
      ui_state->tooltip_open = 0;
      ui_state->ctx_menu_changed = 0;
      ui_state->default_animation_rate = 1 - PowF32(2, (-60.f * ui_state->animation_dt));
      ui_state->tooltip_can_overflow_window = 0;
      ui_state->tooltip_anchor_key = UI_KeyZero();
      ui_state->tags_key_stack_top = ui_state->tags_key_stack_free = 0;
      ui_state->tags_cache_slots_count = 512;
      ui_state->tags_cache_slots = PushArray(UI_BuildArena(), UI_TagsCacheSlot, ui_state->tags_cache_slots_count);
      ui_state->autocomplete_string = Str8Zero();
   }

   //- rjf: prune unused animation nodes
   ProfScope("ui prune unused animation nodes") {
      for (UI_AnimNode *n = ui_state->lru_anim_node, *next = &ui_nil_anim_node; n != &ui_nil_anim_node && n != 0; n = next) {
         next = n->lru_next;
         if (n->last_touched_build_index + 2 < ui_state->build_index) {
            U64          slot_idx = n->key.u64[0] % ui_state->anim_slots_count;
            UI_AnimSlot* slot = &ui_state->anim_slots[slot_idx];
            DLLRemove_NPZ(&ui_nil_anim_node, slot->first, slot->last, n, slot_next, slot_prev);
            ;
            DLLRemove_NPZ(&ui_nil_anim_node, ui_state->lru_anim_node, ui_state->mru_anim_node, n, lru_next, lru_prev);
            SLLStackPush_N(ui_state->free_anim_node, n, slot_next);
         }
         else {
            break;
         }
      }
   }

   //- rjf: prune unused theme pattern cache nodes
   ProfScope("ui prune unused theme pattern cache") {
      for (UI_ThemePatternCacheNode *n = ui_state->lru_theme_pattern_cache_node, *next = 0; n != 0; n = next) {
         next = n->lru_next;
         if (n->last_build_index_accessed + 2 < ui_state->build_index) {
            U64                       slot_idx = n->key.u64[0] % ui_state->theme_pattern_cache_slots_count;
            UI_ThemePatternCacheSlot* slot = &ui_state->theme_pattern_cache_slots[slot_idx];
            DLLRemove_NP(slot->first, slot->last, n, slot_next, slot_prev);
            DLLRemove_NP(ui_state->lru_theme_pattern_cache_node, ui_state->mru_theme_pattern_cache_node, n, lru_next, lru_prev);
            SLLStackPush_N(ui_state->theme_pattern_cache_node_free, n, slot_next);
         }
         else {
            break;
         }
      }
   }

   //- rjf: detect mouse-moves
   for (UI_EventNode* n = events->first; n != 0; n = n->next) {
      if (n->v.kind == UI_EventKind_MouseMove) {
         ui_state->last_time_mousemoved_us = NowTimeUS();
      }
   }

   //- rjf: detect external press & holds
   for EachEnumVal(UI_MouseButtonKind, k) {
      if (UI_KeyMatch(ui_state->active_box_key[k], UI_KeyZero()) && Core_WM_KeyIsDown((WM_Key)(WM_Key_LeftMouseButton + (int)k))) {
         ui_state->active_box_key[k] = ui_state->external_key;
      }
      else if (UI_KeyMatch(ui_state->active_box_key[k], ui_state->external_key) && !Core_WM_KeyIsDown((WM_Key)(WM_Key_LeftMouseButton + (int)k))) {
         ui_state->active_box_key[k] = UI_KeyZero();
      }
   }

        //- rjf: fill build phase parameters
        {
           ui_state->theme = theme;
           ui_state->events = events;
           ui_state->window = window;
           ui_state->mouse = (Core_WM_WindowIsFocused(window) || ui_state->last_time_mousemoved_us + 500000 >= NowTimeUS()) ? Core_WM_MouseFromWindow(window) : V2F32(-100, -100);
           ui_state->animation_dt = animation_dt;
           MemoryZeroStruct(&ui_state->icon_info);
           ui_state->icon_info.icon_font = icon_info->icon_font;
           for (UI_IconKind icon_kind = UI_IconKind_Null;
                icon_kind < UI_IconKind_COUNT;
                icon_kind = (UI_IconKind)(icon_kind + 1)) {
              ui_state->icon_info.icon_kind_text_map[icon_kind] = PushStr8Copy(UI_BuildArena(), icon_info->icon_kind_text_map[icon_kind]);
           }
           MemoryCopyStruct(&ui_state->animation_info, animation_info);
        }

        //- rjf: do default navigation
        {
           Temp scratch = ScratchBegin(0, 0);
           if (!UI_KeyMatch(ui_state->default_nav_root_key, UI_KeyZero())) {
              UI_Box* nav_root = UI_BoxFromKey(ui_state->default_nav_root_key);
              if (!UI_BoxIsNil(nav_root)) {
                 //- rjf: no child has the active focus -> do navigation at this layer
                 if (UI_KeyMatch(UI_KeyZero(), nav_root->default_nav_focus_active_key)) {
                    for (;;) {
                       B32        moved = 0;
                       UI_Box*    focus_box = UI_BoxFromKey(nav_root->default_nav_focus_next_hot_key);
                       UI_BoxList next_focus_box_candidates = {0};

                       // rjf: gather & consume events & nav actions
                       B32   nav_next = 0;
                       B32   nav_prev = 0;
                       Axis2 axis_lock = Axis2_Invalid;
                       if (UI_KeyPress(0, WM_Key_Tab)) {
                          nav_next = 1;
                       }
                       if (UI_KeyPress(WM_Modifier_Shift, WM_Key_Tab)) {
                          nav_prev = 1;
                       }
                       for (UI_EventNode *node = events->first, *next = 0; node != 0; node = next) {
                          next = node->next;
                          B32 taken = 0;
                          if (node->v.delta_2s32.x == 0 && node->v.delta_2s32.y == 0) {
                             continue;
                          }
                          if (((node->v.delta_2s32.x > 0 && nav_root->flags & UI_BoxFlag_DefaultFocusNavX) || node->v.delta_2s32.x == 0) &&
                              ((node->v.delta_2s32.y > 0 && nav_root->flags & UI_BoxFlag_DefaultFocusNavY) || node->v.delta_2s32.y == 0)) {
                             taken = 1;
                             nav_next = 1;
                          }
                          if (((node->v.delta_2s32.x < 0 && nav_root->flags & UI_BoxFlag_DefaultFocusNavX) || node->v.delta_2s32.x == 0) &&
                              ((node->v.delta_2s32.y < 0 && nav_root->flags & UI_BoxFlag_DefaultFocusNavY) || node->v.delta_2s32.y == 0)) {
                             taken = 1;
                             nav_prev = 1;
                          }
                          if (node->v.flags & UI_EventFlag_ExplicitDirectional) {
                             axis_lock = node->v.delta_2s32.x != 0 ? Axis2_X : Axis2_Y;
                          }
                          if (taken) {
                             UI_EatEventNode(events, node);
                          }
                       }

                       // rjf: [+] directional movement
                       if (nav_next) {
                          UI_Box* search_start = UI_BoxIsNil(focus_box) ? nav_root : focus_box;
                          U64     moved_in_axis[Axis2_COUNT] = {0};
                          moved = 1;
                          for (UI_Box* box = search_start;;) {
                             if (box != search_start && !(box->flags & UI_BoxFlag_FocusNavSkip) && (box->flags & UI_BoxFlag_Clickable || UI_BoxIsNil(box)) && (axis_lock == Axis2_Invalid || moved_in_axis[axis_lock] > 0)) {
                                UI_BoxListPush(scratch.arena, &next_focus_box_candidates, box);
                                if (axis_lock == Axis2_Invalid || moved_in_axis[axis_lock] > 1) {
                                   break;
                                }
                             }
                             UI_Box* last_box = box;
                             if (!UI_BoxIsNil(box->first)) {
                                moved_in_axis[box->child_layout_axis] += 1;
                                box = box->first;
                             }
                             else
                                for (UI_Box* p = box; !UI_BoxIsNil(p) && p != nav_root; p = p->parent) {
                                   if (!UI_BoxIsNil(p->next)) {
                                      moved_in_axis[p->parent->child_layout_axis] += 1;
                                      box = p->next;
                                      break;
                                   }
                                }
                             if (last_box == box) {
                                UI_BoxListPush(scratch.arena, &next_focus_box_candidates, &ui_nil_box);
                                break;
                             }
                          }
                       }

                       // rjf: [-] directional movement
                       if (nav_prev) {
                          UI_Box* search_start = UI_BoxIsNil(focus_box) ? nav_root : focus_box;
                          U64     moved_in_axis[Axis2_COUNT] = {0};
                          moved = 1;
                          for (UI_Box* box = search_start;;) {
                             if (box != search_start && !(box->flags & UI_BoxFlag_FocusNavSkip) && (box->flags & UI_BoxFlag_Clickable || UI_BoxIsNil(box)) && (axis_lock == Axis2_Invalid || moved_in_axis[axis_lock] > 0)) {
                                UI_BoxListPush(scratch.arena, &next_focus_box_candidates, box);
                                if (axis_lock == Axis2_Invalid || moved_in_axis[axis_lock] > 1) {
                                   break;
                                }
                             }
                             UI_Box* last_box = box;
                             UI_Box* root_descendant = &ui_nil_box;
                             if (box == nav_root && box == search_start) {
                                for (UI_Box* d = box->last; !UI_BoxIsNil(d); d = d->last) {
                                   moved_in_axis[d->parent->child_layout_axis] += 1;
                                   root_descendant = d;
                                }
                             }
                             UI_Box* prev_descendant = &ui_nil_box;
                             for (UI_Box* d = box->prev; !UI_BoxIsNil(d); d = d->last) {
                                moved_in_axis[d->parent->child_layout_axis] += 1;
                                prev_descendant = d;
                             }
                             if (!UI_BoxIsNil(root_descendant)) {
                                box = root_descendant;
                             }
                             else if (!UI_BoxIsNil(prev_descendant)) {
                                box = prev_descendant;
                             }
                             else if (box->parent != nav_root) {
                                moved_in_axis[box->parent->child_layout_axis] += 1;
                                box = box->parent;
                             }
                             if (box == last_box) {
                                UI_BoxListPush(scratch.arena, &next_focus_box_candidates, &ui_nil_box);
                                break;
                             }
                          }
                       }

                       // rjf: scan candidates and grab next focus box
                       UI_Box* next_focus_box = focus_box;
                       F32     best_distance_from_start = 1000000;
                       for (UI_BoxNode* n = next_focus_box_candidates.first; n != 0; n = n->next) {
                          UI_Box* box = n->box;
                          F32     distance_from_start = 0;
                          if (axis_lock != Axis2_Invalid) {
                             distance_from_start = AbsF32(Center2F32(box->rect).v[Axis2Flip(axis_lock)] - Center2F32(focus_box->rect).v[Axis2Flip(axis_lock)]);
                          }
                          if (distance_from_start < best_distance_from_start && box != focus_box) {
                             next_focus_box = box;
                             best_distance_from_start = distance_from_start;
                          }
                       }

                       // rjf: commit next focus box
                       nav_root->default_nav_focus_next_hot_key = next_focus_box->key;

                       // rjf: no movement -> break
                       if (moved == 0) {
                          break;
                       }
                    }
                 }

                 //- rjf: some child has the active focus -> accept escape keys to pop from the active key stack
                 if (!UI_KeyMatch(UI_KeyZero(), nav_root->default_nav_focus_active_key)) {
                    for (; UI_SlotPress(UI_EventActionSlot_Cancel);) {
                       UI_Box* prev_focus_root = nav_root;
                       for (UI_Box* focus_root = UI_BoxFromKey(nav_root->default_nav_focus_active_key);
                            !UI_BoxIsNil(focus_root);) {
                          UI_Box* next_focus_root = UI_BoxFromKey(focus_root->default_nav_focus_active_key);
                          if (UI_BoxIsNil(next_focus_root)) {
                             prev_focus_root->default_nav_focus_next_active_key = UI_KeyZero();
                             break;
                          }
                          else {
                             prev_focus_root = focus_root;
                             focus_root = next_focus_root;
                          }
                       }
                    }
                 }
              }
           }
           ui_state->default_nav_root_key = UI_KeyZero();
           ScratchEnd(scratch);
        }

        //- rjf: next-default-nav-focus keys -> current-default-nav-focus-keys
        for (U64 slot_idx = 0; slot_idx < ui_state->box_table_size; slot_idx += 1) {
           for (UI_Box* box = ui_state->box_table[slot_idx].hash_first;
                !UI_BoxIsNil(box);
                box = box->hash_next) {
              box->default_nav_focus_hot_key = box->default_nav_focus_next_hot_key;
              box->default_nav_focus_active_key = box->default_nav_focus_next_active_key;
           }
        }

        //- rjf: build top-level root
        {
           Rng2F32 window_rect = Core_WM_ClientRectFromWindow(window);
           Vec2F32 window_rect_size = Dim2F32(window_rect);
           UI_SetNextFixedWidth(window_rect_size.x);
           UI_SetNextFixedHeight(window_rect_size.y);
           UI_SetNextChildLayoutAxis(Axis2_X);
           UI_Box* root = UI_BuildBoxFromStringf(0, "###%I64x", window.u64[0]);
           UI_PushParent(root);
           ui_state->root = root;
        }

        //- rjf: setup parent box for tooltip
        UI_FixedX(ui_state->mouse.x + 15.f) UI_FixedY(ui_state->mouse.y + 15.f) UI_PrefWidth(UI_ChildrenSum(1.f)) UI_PrefHeight(UI_ChildrenSum(1.f)) {
           UI_SetNextChildLayoutAxis(Axis2_Y);
           ui_state->tooltip_root = UI_BuildBoxFromStringf(0, "###tooltip_%I64x", window.u64[0]);
        }

        //- rjf: setup parent box for context menu
        ui_state->ctx_menu_open = ui_state->next_ctx_menu_open;
        ui_state->ctx_menu_anchor_key = ui_state->next_ctx_menu_anchor_key;
        {
           UI_Box* anchor_box = UI_BoxFromKey(ui_state->ctx_menu_anchor_key);
           if (!UI_BoxIsNil(anchor_box)) {
              ui_state->ctx_menu_anchor_box_last_pos = anchor_box->rect.p0;
           }
           Vec2F32 anchor = Add2F32(ui_state->ctx_menu_anchor_box_last_pos, ui_state->ctx_menu_anchor_off);
           UI_FixedX(anchor.x) UI_FixedY(anchor.y) UI_PrefWidth(UI_ChildrenSum(1.f)) UI_PrefHeight(UI_ChildrenSum(1.f))
               UI_Focus(UI_FocusKind_On)
                   UI_Squish(0.1f - ui_state->ctx_menu_open_t * 0.1f)
                       UI_Transparency(1 - ui_state->ctx_menu_open_t) {
              UI_SetNextChildLayoutAxis(Axis2_Y);
              ui_state->ctx_menu_root = UI_BuildBoxFromStringf(UI_BoxFlag_Clickable |
                                                                   UI_BoxFlag_SquishAnchored |
                                                                   UI_BoxFlag_DrawDropShadow |
                                                                   (ui_state->ctx_menu_open * UI_BoxFlag_DefaultFocusNavY),
                                                               "###ctx_menu_%I64x", window.u64[0]);
           }
        }

        //- rjf: reset hot if we don't have an active widget
        {
           B32 has_active = 0;
           for EachEnumVal(UI_MouseButtonKind, k) {
              if (!UI_KeyMatch(ui_state->active_box_key[k], UI_KeyZero())) {
                 has_active = 1;
              }
           }
                if (!has_active) {
                   ui_state->hot_box_key = UI_KeyZero();
                }
        }

        //- rjf: reset drop-hot key
        {
           ui_state->drop_hot_box_key = UI_KeyZero();
        }

        //- rjf: reset active if our active box is disabled
        for EachEnumVal(UI_MouseButtonKind, k) {
           if (!UI_KeyMatch(ui_state->active_box_key[k], UI_KeyZero())) {
              UI_Box* box = UI_BoxFromKey(ui_state->active_box_key[k]);
              if (!UI_BoxIsNil(box) && box->flags & UI_BoxFlag_Disabled) {
                 ui_state->active_box_key[k] = UI_KeyZero();
              }
           }
        }

        //- rjf: reset active keys if they have been pruned
        for EachEnumVal(UI_MouseButtonKind, k) {
           UI_Box* box = UI_BoxFromKey(ui_state->active_box_key[k]);
           if (UI_BoxIsNil(box)) {
              ui_state->active_box_key[k] = UI_KeyZero();
           }
        }

        //- rjf: escape -> close context menu
        if (UI_AnyCtxMenuIsOpen() && UI_SlotPress(UI_EventActionSlot_Cancel)) {
           UI_CtxMenuClose();
        }
}

void UI_EndBuild(void) {
   ProfBeginFunction();

   //- rjf: prune untouched or transient boxes in the cache
   ProfScope("ui prune unused boxes") {
      for (U64 slot_idx = 0; slot_idx < ui_state->box_table_size; slot_idx += 1) {
         for (UI_Box *box = ui_state->box_table[slot_idx].hash_first, *next = 0;
              !UI_BoxIsNil(box);
              box = next) {
            next = box->hash_next;
            if (box->last_touched_build_index < ui_state->build_index ||
                UI_KeyMatch(box->key, UI_KeyZero())) {
               DLLRemove_NPZ(&ui_nil_box, ui_state->box_table[slot_idx].hash_first, ui_state->box_table[slot_idx].hash_last, box, hash_next, hash_prev);
               SLLStackPush(ui_state->first_free_box, box);
            }
         }
      }
   }

   //- rjf: layout box tree
   ProfScope("ui box tree layout") {
      for (Axis2 axis = (Axis2)0; axis < Axis2_COUNT; axis = (Axis2)(axis + 1)) {
         UI_LayoutRoot(ui_state->root, axis);
      }
   }

   //- rjf: close ctx menu if untouched
   if (!ui_state->ctx_menu_touched_this_frame) {
      UI_CtxMenuClose();
   }

   //- rjf: stick ctx menu to anchor
   if (ui_state->ctx_menu_touched_this_frame && !ui_state->ctx_menu_changed) {
      UI_Box* anchor_box = UI_BoxFromKey(ui_state->ctx_menu_anchor_key);
      if (!UI_BoxIsNil(anchor_box)) {
         Rng2F32 root_rect = ui_state->ctx_menu_root->rect;
         Vec2F32 pos =
             {{
                 anchor_box->rect.x0 + ui_state->ctx_menu_anchor_off.x,
                 anchor_box->rect.y0 + ui_state->ctx_menu_anchor_off.y,
             }};
         Vec2F32 shift = Sub2F32(pos, root_rect.p0);
         Rng2F32 new_root_rect = Shift2F32(root_rect, shift);
         ui_state->ctx_menu_root->fixed_position = new_root_rect.p0;
         ui_state->ctx_menu_root->fixed_size = Dim2F32(new_root_rect);
         ui_state->ctx_menu_root->rect = new_root_rect;
      }
   }

   //- rjf: anchor tooltips
   if (!UI_KeyMatch(ui_state->tooltip_anchor_key, UI_KeyZero())) {
      UI_Box* anchor_box = UI_BoxFromKey(ui_state->tooltip_anchor_key);
      if (!UI_BoxIsNil(anchor_box)) {
         Vec2F32 dim = Dim2F32(ui_state->tooltip_root->rect);
         ui_state->tooltip_root->fixed_position.x = ui_state->tooltip_root->rect.x0 = anchor_box->rect.x0;
         ui_state->tooltip_root->fixed_position.y = ui_state->tooltip_root->rect.y0 = anchor_box->rect.y1 + anchor_box->font_size * 0.5f;
         ui_state->tooltip_root->rect.x1 = ui_state->tooltip_root->rect.x0 + dim.x;
         ui_state->tooltip_root->rect.y1 = ui_state->tooltip_root->rect.y0 + dim.y;
      }
      else {
         ui_state->tooltip_root->rect.x0 = 10000;
         ui_state->tooltip_root->rect.y0 = 10000;
      }
   }

   //- rjf: ensure special floating roots are within screen bounds
   UI_Box* floating_roots[] = {ui_state->tooltip_root, ui_state->ctx_menu_root};
   B32     force_contain[] =
       {
           !ui_state->tooltip_can_overflow_window,
           1,
       };
   for (U64 idx = 0; idx < ArrayCount(floating_roots); idx += 1) {
      UI_Box* root = floating_roots[idx];
      if (!UI_BoxIsNil(root)) {
         Rng2F32 window_rect = Core_WM_ClientRectFromWindow(UI_Window());
         Rng2F32 root_rect = root->rect;
         Vec2F32 shift_down =
             {{
                 -ClampBot(0, root_rect.x1 - window_rect.x1) * (force_contain[idx]),
                 -ClampBot(0, root_rect.y1 - window_rect.y1) * (force_contain[idx]),
             }};
         Rng2F32 new_root_rect = Shift2F32(root_rect, shift_down);
         Vec2F32 shift_up =
             {{
                 ClampBot(0, window_rect.x0 - new_root_rect.x0) * (force_contain[idx]),
                 ClampBot(0, window_rect.y0 - new_root_rect.y0) * (force_contain[idx]),
             }};
         new_root_rect = Shift2F32(new_root_rect, shift_up);
         root->fixed_position = new_root_rect.p0;
         root->fixed_size = Dim2F32(new_root_rect);
         root->rect = new_root_rect;
         for (Axis2 axis = (Axis2)0; axis < Axis2_COUNT; axis = (Axis2)(axis + 1)) {
            UI_CalcSizesStandaloneInPlace(root, axis);
            UI_CalcSizesUpwardsDependentInPlace(root, axis);
            UI_CalcSizesDownwardsDependentInPlace(root, axis);
            UI_LayoutEnforceConstraintsInPlace(root, axis);
            UI_LayoutPositionInPlace(root, axis);
         }
      }
   }

   //- rjf: enforce child-rounding
   {
      for (U64 slot_idx = 0; slot_idx < ui_state->box_table_size; slot_idx += 1) {
         for (UI_Box* box = ui_state->box_table[slot_idx].hash_first;
              !UI_BoxIsNil(box);
              box = box->hash_next) {
            if (box->flags & UI_BoxFlag_RoundChildrenByParent) {
               for (UI_Box* b = box; !UI_BoxIsNil(b); b = UI_BoxRecDfPre(b, box).next) {
                  if (FloorF32(b->rect.x0) <= FloorF32(box->rect.x0) &&
                      FloorF32(b->rect.y0) <= FloorF32(box->rect.y0)) {
                     b->corner_radii[Corner_00] = box->corner_radii[Corner_00];
                  }
                  if (FloorF32(b->rect.x1) >= FloorF32(box->rect.x1) &&
                      FloorF32(b->rect.y0) <= FloorF32(box->rect.y0)) {
                     b->corner_radii[Corner_10] = box->corner_radii[Corner_10];
                  }
                  if (FloorF32(b->rect.x0) <= FloorF32(box->rect.x0) &&
                      FloorF32(b->rect.y1) >= FloorF32(box->rect.y1)) {
                     b->corner_radii[Corner_01] = box->corner_radii[Corner_01];
                  }
                  if (FloorF32(b->rect.x1) >= FloorF32(box->rect.x1) &&
                      FloorF32(b->rect.y1) >= FloorF32(box->rect.y1)) {
                     b->corner_radii[Corner_11] = box->corner_radii[Corner_11];
                  }
               }
               box->first->corner_radii[Corner_00] = box->corner_radii[Corner_00];
               box->first->corner_radii[Corner_10] = box->corner_radii[Corner_10];
               box->last->corner_radii[Corner_01] = box->corner_radii[Corner_01];
               box->last->corner_radii[Corner_11] = box->corner_radii[Corner_11];
            }
         }
      }
   }

   //- rjf: animate
   ProfScope("animate") {
      for (U64 slot_idx = 0; slot_idx < ui_state->anim_slots_count; slot_idx += 1) {
         for (UI_AnimNode* n = ui_state->anim_slots[slot_idx].first;
              n != &ui_nil_anim_node && n != 0;
              n = n->slot_next) {
            n->current += (n->params.target - n->current) * n->params.rate;
            ui_state->is_animating = (ui_state->is_animating || AbsF32(n->params.target - n->current) > n->params.epsilon);
         }
      }
      F32 fast_rate = ui_state->default_animation_rate;
      F32 slow_rate = 1 - PowF32(2, (-30.f * ui_state->animation_dt));
      for (U64 slot_idx = 0; slot_idx < ui_state->theme_pattern_cache_slots_count; slot_idx += 1) {
         for (UI_ThemePatternCacheNode* n = ui_state->theme_pattern_cache_slots[slot_idx].first;
              n != 0;
              n = n->slot_next) {
            for EachIndex(idx, 4) {
               n->current_rgba.v[idx] += (n->target_rgba.v[idx] - n->current_rgba.v[idx]) * slow_rate;
               ui_state->is_animating = (ui_state->is_animating || AbsF32(n->target_rgba.v[idx] - n->current_rgba.v[idx]) > 0.001f);
            }
         }
      }
      ui_state->ctx_menu_open_t += ((F32) !!ui_state->ctx_menu_open - ui_state->ctx_menu_open_t) * ui_state->animation_info.menu_animation_rate;
      ui_state->is_animating = (ui_state->is_animating || AbsF32((F32) !!ui_state->ctx_menu_open - ui_state->ctx_menu_open_t) > 0.01f);
      if (ui_state->ctx_menu_open_t >= 0.99f && ui_state->ctx_menu_open) {
         ui_state->ctx_menu_open_t = 1.f;
      }
      ui_state->tooltip_open_t += ((F32) !!ui_state->tooltip_open - ui_state->tooltip_open_t) * ui_state->animation_info.tooltip_animation_rate;
      ui_state->is_animating = (ui_state->is_animating || AbsF32((F32) !!ui_state->tooltip_open - ui_state->tooltip_open_t) > 0.01f);
      if (ui_state->tooltip_open_t >= 0.99f && ui_state->tooltip_open) {
         ui_state->tooltip_open_t = 1.f;
      }
      for (U64 slot_idx = 0; slot_idx < ui_state->box_table_size; slot_idx += 1) {
         for (UI_Box* box = ui_state->box_table[slot_idx].hash_first;
              !UI_BoxIsNil(box);
              box = box->hash_next) {
            // rjf: grab states informing animation
            B32 is_hot = (UI_KeyMatch(box->key, ui_state->hot_box_key) ||
                          UI_KeyMatch(box->key, ui_state->drop_hot_box_key));
            B32 is_active = UI_KeyMatch(box->key, ui_state->active_box_key[UI_MouseButtonKind_Left]);
            B32 is_disabled = !!(box->flags & UI_BoxFlag_Disabled) && (box->first_disabled_build_index + 2 < ui_state->build_index ||
                                                                       box->first_touched_build_index == box->first_disabled_build_index);
            B32 is_focus_hot = !!(box->flags & UI_BoxFlag_FocusHot) && !(box->flags & UI_BoxFlag_FocusHotDisabled);
            B32 is_focus_active = !!(box->flags & UI_BoxFlag_FocusActive) && !(box->flags & UI_BoxFlag_FocusActiveDisabled);
            B32 is_focus_active_disabled = !!(box->flags & UI_BoxFlag_FocusActiveDisabled);

            // rjf: determine rates
            F32 hot_rate = ui_state->animation_info.hot_animation_rate;
            F32 active_rate = ui_state->animation_info.active_animation_rate;
            F32 disabled_rate = slow_rate;
            F32 focus_rate = ui_state->animation_info.focus_animation_rate;

            // rjf: determine animating status
            B32 box_is_animating = 0;
            box_is_animating = (box_is_animating || AbsF32((F32)is_hot - box->hot_t) > 0.01f);
            box_is_animating = (box_is_animating || AbsF32((F32)is_active - box->active_t) > 0.01f);
            box_is_animating = (box_is_animating || AbsF32((F32)is_disabled - box->disabled_t) > 0.01f);
            box_is_animating = (box_is_animating || AbsF32((F32)is_focus_hot - box->focus_hot_t) > 0.01f);
            box_is_animating = (box_is_animating || AbsF32((F32)is_focus_active - box->focus_active_t) > 0.01f);
            box_is_animating = (box_is_animating || AbsF32((F32)is_focus_active_disabled - box->focus_active_disabled_t) > 0.01f);
            box_is_animating = (box_is_animating || AbsF32(box->view_off_target.x - box->view_off.x) > 0.5f);
            box_is_animating = (box_is_animating || AbsF32(box->view_off_target.y - box->view_off.y) > 0.5f);
            if (box->flags & UI_BoxFlag_AnimatePosX) {
               box_is_animating = (box_is_animating || AbsF32(box->fixed_position_animated.x - box->fixed_position.x) > 0.5f);
            }
            if (box->flags & UI_BoxFlag_AnimatePosY) {
               box_is_animating = (box_is_animating || AbsF32(box->fixed_position_animated.y - box->fixed_position.y) > 0.5f);
            }
            ui_state->is_animating = (ui_state->is_animating || box_is_animating);
#if 0 // NOTE(rjf): enable to debug animation-causing-frames (or not)
				if(box_is_animating)
				{
					box->overlay_color = v4f32(1, 0, 0, 0.1f);
					box->flags |= UI_BoxFlag_DrawOverlay;
				}
#endif

            // rjf: animate interaction transition states
            box->hot_t += hot_rate * ((F32)is_hot - box->hot_t);
            box->active_t = is_active ? 1.f : box->active_t + (active_rate * ((F32)is_active - box->active_t));
            box->disabled_t += disabled_rate * ((F32)is_disabled - box->disabled_t);
            box->focus_hot_t += focus_rate * ((F32)is_focus_hot - box->focus_hot_t);
            box->focus_active_t += focus_rate * ((F32)is_focus_active - box->focus_active_t);
            box->focus_active_disabled_t += focus_rate * ((F32)is_focus_active_disabled - box->focus_active_disabled_t);

            // rjf: animate positions
            {
               box->fixed_position_animated.x += fast_rate * (box->fixed_position.x - box->fixed_position_animated.x);
               box->fixed_position_animated.y += fast_rate * (box->fixed_position.y - box->fixed_position_animated.y);
               if (AbsF32(box->fixed_position.x - box->fixed_position_animated.x) < 1) {
                  box->fixed_position_animated.x = box->fixed_position.x;
               }
               if (AbsF32(box->fixed_position.y - box->fixed_position_animated.y) < 1) {
                  box->fixed_position_animated.y = box->fixed_position.y;
               }
            }

            // rjf: clamp view
            if (box->flags & UI_BoxFlag_ViewClamp) {
               Vec2F32 max_view_off_target =
                   {{
                       ClampBot(0, box->view_bounds.x - box->fixed_size.x),
                       ClampBot(0, box->view_bounds.y - box->fixed_size.y),
                   }};
               if (box->flags & UI_BoxFlag_ViewClampX) {
                  box->view_off_target.x = Clamp(0, box->view_off_target.x, max_view_off_target.x);
               }
               if (box->flags & UI_BoxFlag_ViewClampY) {
                  box->view_off_target.y = Clamp(0, box->view_off_target.y, max_view_off_target.y);
               }
            }

            // rjf: animate view offset
            {
               box->view_off.x += ui_state->animation_info.scroll_animation_rate * (box->view_off_target.x - box->view_off.x);
               box->view_off.y += ui_state->animation_info.scroll_animation_rate * (box->view_off_target.y - box->view_off.y);
               if (AbsF32(box->view_off.x - box->view_off_target.x) < 2) {
                  box->view_off.x = box->view_off_target.x;
               }
               if (AbsF32(box->view_off.y - box->view_off_target.y) < 2) {
                  box->view_off.y = box->view_off_target.y;
               }
            }
         }
      }
   }

   //- rjf: use group keys for box animation data if possible
   for (UI_Box* b = ui_state->root; !UI_BoxIsNil(b); b = UI_BoxRecDfPre(b, ui_state->root).next) {
      if (UI_KeyMatch(b->key, UI_KeyZero()) && !UI_KeyMatch(b->group_key, UI_KeyZero())) {
         UI_Box* group_box = UI_BoxFromKey(b->group_key);
         b->hot_t = group_box->hot_t;
      }
   }

   //- rjf: fall-through interact with context menu
   if (ui_state->ctx_menu_open) {
      UI_SignalFromBox(ui_state->ctx_menu_root);
   }

   //- rjf: close ctx menu if unconsumed clicks
   {
      for (UI_Event* evt = 0; UI_NextEvent(&evt);) {
         if (evt->kind == UI_EventKind_Press &&
             (evt->key == WM_Key_LeftMouseButton || evt->key == WM_Key_RightMouseButton)) {
            UI_CtxMenuClose();
         }
      }
   }

   //- rjf: hover cursor
   if (!UI_KeyMatch(ui_state->active_box_key[UI_MouseButtonKind_Left], ui_state->external_key)) {
      UI_Box*       hot = UI_BoxFromKey(ui_state->hot_box_key);
      UI_Box*       active = UI_BoxFromKey(ui_state->active_box_key[UI_MouseButtonKind_Left]);
      UI_Box*       box = UI_BoxIsNil(active) ? hot : active;
      WM_Cursor cursor = box->hover_cursor;
      if (box->flags & UI_BoxFlag_Disabled && box->flags & UI_BoxFlag_Clickable) {
         cursor = WM_Cursor_Disabled;
      }
      // raddebugger's condition (ui_end_build): the hot box's cursor while
      // the window has focus, the active one's while dragging out of it.
      if (Core_WM_WindowIsFocused(ui_state->window) || !UI_BoxIsNil(active)) {
         Core_WM_SetCursor(cursor);
      }
   }

   //- rjf: clipboard commits
   {
      UI_Box* box = UI_BoxFromKey(ui_state->clipboard_copy_key);
      if (!UI_BoxIsNil(box)) {
         Temp        scratch = ScratchBegin(0, 0);
         String8List strs = {0};
         UI_BoxRec   rec = {0};
         for (UI_Box* b = box; !UI_BoxIsNil(b); rec = UI_BoxRecDfPre(b, box), b = rec.next) {
            if (b->flags & UI_BoxFlag_DrawText && b->flags & UI_BoxFlag_HasDisplayString && !Core_FNT_TagMatch(b->font, UI_IconFont())) {
               String8 display_string = UI_BoxDisplayString(b);
               Str8ListPush(scratch.arena, &strs, display_string);
            }
         }
         if (strs.node_count != 0) {
            StringJoin join = {0};
            join.sep = Str8Lit(" ");
            String8 string = Str8ListJoin(scratch.arena, &strs, &join);
            Core_WM_SetClipboardText(string);
         }
         ScratchEnd(scratch);
      }
   }

   //- rjf: hovering possibly-truncated drawn text -> store text
   // Jason ~ TODO: I'm if'ing this out for now until I look more into string runlists
#if 0
   {
      B32 inactive = 1;
                for EachEnumVal(UI_MouseButtonKind, k) {
                      if (!UI_KeyMatch(UI_KeyZero(), ui_state->active_box_key[k])) {
                         inactive = 0;
                         break;
                      }
                   }
                if (inactive) {
                   B32 found = 0;
                   for (UI_Box *box = ui_state->root, *next = 0; !UI_BoxIsNil(box); box = next) {
                      UI_BoxRec rec = UI_BoxRecDfPre(box, ui_state->root);
                      next = rec.next;
                      S32 pop_idx = 0;
                      for (UI_Box* b = box; !UI_BoxIsNil(b) && pop_idx <= rec.pop_count; b = b->parent, pop_idx += 1) {
                         if (b->flags & UI_BoxFlag_DrawText && !(b->flags & UI_BoxFlag_DisableTextTrunc)) {
                            Rng2F32 rect = b->rect;
                            for (UI_Box* p = b->parent; !UI_BoxIsNil(p); p = p->parent) {
                               if (p->flags & UI_BoxFlag_Clip) {
                                  rect = Intersect2F32(rect, p->rect);
                               }
                            }
                            String8 box_display_string = UI_BoxDisplayString(b);
                            Vec2F32 text_pos = UI_BoxTextPosition(b);
                            Vec2F32 drawn_text_dim = {0};
                            {
                               Temp        scratch = ScratchBegin(0, 0);
                               DR_FRunList fruns = Core_D_FStrs_Fruns(scratch.arena, b->tab_size, &b->display_fstrs);
                               drawn_text_dim = fruns.dim;
                               ScratchEnd(scratch);
                            }
                            B32 text_is_truncated = (drawn_text_dim.x + text_pos.x > rect.x1);
                            B32 mouse_is_hovering = Contains2F32(r2f32p(text_pos.x,
                                                                         rect.y0,
                                                                         Min(text_pos.x + drawn_text_dim.x, rect.x1),
                                                                         rect.y1),
                                                                  ui_state->mouse);
                            if (text_is_truncated && mouse_is_hovering && !(b->flags & UI_BoxFlag_DisableTruncatedHover)) {
                               if (!Str8Match(box_display_string, ui_state->string_hover_string, 0) || box->font_size != ui_state->string_hover_size) {
                                  ArenaClear(ui_state->string_hover_arena);
                                  ui_state->string_hover_string = PushStr8Copy(ui_state->string_hover_arena, box_display_string);
                                  ui_state->string_hover_size = box->font_size;
                                  ui_state->string_hover_fstrs = Core_D_FStrs_Copy(ui_state->string_hover_arena, &b->display_fstrs);
                                  ui_state->string_hover_begin_us = now_time_us();
                               }
                               ui_state->string_hover_build_index = ui_state->build_index;
                               found = 1;
                               goto break_all_hover_string;
                            }
                         }
                         if (b != box && UI_KeyMatch(b->key, UI_HotKey())) {
                            goto break_all_hover_string;
                         }
                         if (b != box && Contains2F32(b->rect, ui_state->mouse) && b->flags & UI_BoxFlag_DrawText) {
                            goto break_all_hover_string;
                         }
                      }
                   }
                break_all_hover_string:;
                   if (!found) {
                      ArenaClear(ui_state->string_hover_arena);
                      ui_state->string_hover_build_index = 0;
                      MemoryZeroStruct(&ui_state->string_hover_string);
                   }
                   if (found && !UI_StringHoverActive()) {
                      ui_state->is_animating = 1;
                   }
                }
   }
#endif

   ui_state->build_index += 1;
   ArenaClear(UI_BuildArena());
   ProfEnd();
}

void UI_CalcSizesStandaloneInPlace(UI_Box* root, Axis2 axis) {
   ProfBeginFunction();
   for (UI_Box* b = root; !UI_BoxIsNil(b); b = UI_BoxRecDfPre(b, root).next) {
      switch (b->pref_size[axis].kind) {
      default: {
      } break;
      case UI_SizeKind_Pixels: {
         b->fixed_size.v[axis] = b->pref_size[axis].value;
      } break;
      case UI_SizeKind_TextContent: {
         F32 padding = b->pref_size[axis].value;
         F32 text_size = b->display_fruns.dim.x;
         b->fixed_size.v[axis] = padding + text_size + b->text_padding * 2;
      } break;
      }
   }
   ProfEnd();
}

void UI_CalcSizesUpwardsDependentInPlace(UI_Box* root, Axis2 axis) {
   ProfBeginFunction();
   for (UI_Box* b = root; !UI_BoxIsNil(b); b = UI_BoxRecDfPre(b, root).next) {
      switch (b->pref_size[axis].kind) {
      default: {
      } break;
      case UI_SizeKind_ParentPct: {
         // rjf: find parent that has a fixed size
         UI_Box* fixed_parent = &ui_nil_box;
         for (UI_Box* p = b->parent; !UI_BoxIsNil(p); p = p->parent) {
            if (p->flags & (UI_BoxFlag_FixedWidth << axis) ||
                p->pref_size[axis].kind == UI_SizeKind_Pixels ||
                p->pref_size[axis].kind == UI_SizeKind_TextContent ||
                p->pref_size[axis].kind == UI_SizeKind_ParentPct) {
               fixed_parent = p;
               break;
            }
         }

         // rjf: figure out box's size on this axis
         F32 size = fixed_parent->fixed_size.v[axis] * b->pref_size[axis].value;

         // rjf: mutate box to have this size
         b->fixed_size.v[axis] = size;
      } break;
      }
   }
   ProfEnd();
}

void UI_CalcSizesDownwardsDependentInPlace(UI_Box* root, Axis2 axis) {
   ProfBeginFunction();
   UI_BoxRec rec = {0};
   for (UI_Box* box = root; !UI_BoxIsNil(box); box = rec.next) {
      rec = UI_BoxRecDfPre(box, root);
      S32 pop_idx = 0;
      for (UI_Box* b = box;
           !UI_BoxIsNil(b) && pop_idx <= rec.pop_count;
           b = b->parent, pop_idx += 1) {
         if (b->pref_size[axis].kind == UI_SizeKind_ChildrenSum) {
            F32 sum = 0;
            for (UI_Box* child = b->first; !UI_BoxIsNil(child); child = child->next) {
               if (!(child->flags & (UI_BoxFlag_FloatingX << axis))) {
                  if (axis == b->child_layout_axis) {
                     sum += child->fixed_size.v[axis];
                  }
                  else {
                     sum = Max(sum, child->fixed_size.v[axis]);
                  }
               }
            }
            b->fixed_size.v[axis] = sum;
         }
      }
   }
   ProfEnd();
}

void UI_LayoutEnforceConstraintsInPlace(UI_Box* root, Axis2 axis) {
   ProfBeginFunction();
   Temp scratch = ScratchBegin(0, 0);
   for (UI_Box* box = root; !UI_BoxIsNil(box); box = UI_BoxRecDfPre(box, root).next) {
      //- rjf: fixup children sizes (if we're solving along the *non-layout* axis)
      if (axis != box->child_layout_axis && !(box->flags & (UI_BoxFlag_AllowOverflowX << axis))) {
         F32 allowed_size = box->fixed_size.v[axis];
         for (UI_Box* child = box->first; !UI_BoxIsNil(child); child = child->next) {
            if (!(child->flags & (UI_BoxFlag_FloatingX << axis))) {
               F32 child_size = child->fixed_size.v[axis];
               F32 violation = child_size - allowed_size;
               F32 max_fixup = child_size;
               F32 fixup = Clamp(0, violation, max_fixup);
               if (fixup > 0) {
                  child->fixed_size.v[axis] -= fixup;
               }
            }
         }
      }

      //- rjf: fixup children sizes (in the direction of the layout axis)
      if (axis == box->child_layout_axis && !(box->flags & (UI_BoxFlag_AllowOverflowX << axis))) {
         // rjf: figure out total allowed size & total size
         F32 total_allowed_size = box->fixed_size.v[axis];
         F32 total_size = 0;
         F32 total_weighted_size = 0;
         for (UI_Box* child = box->first; !UI_BoxIsNil(child); child = child->next) {
            if (!(child->flags & (UI_BoxFlag_FloatingX << axis))) {
               total_size += child->fixed_size.v[axis];
               total_weighted_size += child->fixed_size.v[axis] * (1 - child->pref_size[axis].strictness);
            }
         }

         // rjf: if we have a violation, we need to subtract some amount from all children
         F32 violation = total_size - total_allowed_size;
         if (violation > 0 && total_weighted_size > 0) {
            Temp temp = TempBegin(scratch.arena);

            // rjf: figure out how much we can take in totality
            F32* child_fixups = PushArray(temp.arena, F32, box->child_count);
            {
               U64 child_idx = 0;
               for (UI_Box* child = box->first; !UI_BoxIsNil(child); child = child->next, child_idx += 1) {
                  if (!(child->flags & (UI_BoxFlag_FloatingX << axis))) {
                     F32 fixup_size_this_child = child->fixed_size.v[axis] * (1 - child->pref_size[axis].strictness);
                     fixup_size_this_child = ClampBot(0, fixup_size_this_child);
                     child_fixups[child_idx] = fixup_size_this_child;
                  }
               }
            }

            // rjf: fixup child sizes
            {
               U64 child_idx = 0;
               for (UI_Box* child = box->first; !UI_BoxIsNil(child); child = child->next, child_idx += 1) {
                  if (!(child->flags & (UI_BoxFlag_FloatingX << axis))) {
                     F32 fixup_pct = (violation / total_weighted_size);
                     fixup_pct = Clamp(0, fixup_pct, 1);
                     child->fixed_size.v[axis] -= child_fixups[child_idx] * fixup_pct;
                  }
               }
            }

            TempEnd(temp);
         }
      }

      //- rjf: fixup upwards-relative sizes
      if (box->flags & (UI_BoxFlag_AllowOverflowX << axis)) {
         for (UI_Box* child = box->first; !UI_BoxIsNil(child); child = child->next) {
            if (child->pref_size[axis].kind == UI_SizeKind_ParentPct) {
               child->fixed_size.v[axis] = box->fixed_size.v[axis] * child->pref_size[axis].value;
            }
         }
      }

      //- rjf: enforce clamps
      for (UI_Box* child = box->first; !UI_BoxIsNil(child); child = child->next) {
         child->fixed_size.v[axis] = Max(child->fixed_size.v[axis], child->min_size.v[axis]);
      }
   }
   ScratchEnd(scratch);
   ProfEnd();
}

void UI_LayoutPositionInPlace(UI_Box* root, Axis2 axis) {
   ProfBeginFunction();
   for (UI_Box* box = root; !UI_BoxIsNil(box); box = UI_BoxRecDfPre(box, root).next) {
      F32 layout_position = 0;

      //- rjf: lay out children
      F32 bounds = 0;
      for (UI_Box* child = box->first; !UI_BoxIsNil(child); child = child->next) {
         // rjf: grab original position
         F32 original_position = Min(child->rect.p0.v[axis], child->rect.p1.v[axis]);

         // rjf: calculate fixed position & size
         if (!(child->flags & (UI_BoxFlag_FloatingX << axis))) {
            child->fixed_position.v[axis] = layout_position;
            if (box->child_layout_axis == axis) {
               layout_position += child->fixed_size.v[axis];
               bounds += child->fixed_size.v[axis];
            }
            else {
               bounds = Max(bounds, child->fixed_size.v[axis]);
            }
         }

         // rjf: determine final rect for child, given fixed_position & size
         if (child->flags & (UI_BoxFlag_AnimatePosX << axis)) {
            if (child->first_touched_build_index == child->last_touched_build_index) {
               child->fixed_position_animated = child->fixed_position;
            }
            child->rect.p0.v[axis] = box->rect.p0.v[axis] + child->fixed_position_animated.v[axis] - !(child->flags & (UI_BoxFlag_SkipViewOffX << axis)) * FloorF32(box->view_off.v[axis]);
         }
         else {
            child->rect.p0.v[axis] = box->rect.p0.v[axis] + child->fixed_position.v[axis] - !(child->flags & (UI_BoxFlag_SkipViewOffX << axis)) * FloorF32(box->view_off.v[axis]);
         }
         child->rect.p1.v[axis] = child->rect.p0.v[axis] + child->fixed_size.v[axis];
         child->rect.p0.x = FloorF32(child->rect.p0.x);
         child->rect.p0.y = FloorF32(child->rect.p0.y);
         child->rect.p1.x = FloorF32(child->rect.p1.x);
         child->rect.p1.y = FloorF32(child->rect.p1.y);

         // rjf: grab new position
         F32 new_position = Min(child->rect.p0.v[axis], child->rect.p1.v[axis]);

         // rjf: store position delta
         child->position_delta.v[axis] = new_position - original_position;
      }

      //- rjf: store view bounds
      {
         box->view_bounds.v[axis] = bounds;
      }
   }
   ProfEnd();
}

void UI_LayoutRoot(UI_Box* root, Axis2 axis) {
   ProfBegin("ui layout pass (%s)", axis == Axis2_X ? "x" : "y");
   UI_CalcSizesStandaloneInPlace(root, axis);
   UI_CalcSizesUpwardsDependentInPlace(root, axis);
   UI_CalcSizesDownwardsDependentInPlace(root, axis);
   UI_LayoutEnforceConstraintsInPlace(root, axis);
   UI_LayoutPositionInPlace(root, axis);
   ProfEnd();
}

////////////////////////////////
//~ rjf: Box Building API

//- rjf: spacers

UI_Signal
UI_Spacer(UI_Size size) {
   UI_Box* parent = UI_TopParent();
   UI_SetNextPrefSize(parent->child_layout_axis, size);
   UI_Box*   box = UI_BuildBoxFromKey(0, UI_KeyZero());
   UI_Signal interact = UI_SignalFromBox(box);
   return interact;
}

//- rjf: tooltips

void UI_TooltipBeginBase(void) {
   ui_state->tooltip_open = 1;
   UI_PushParent(UI_RootFromState(ui_state));
   UI_PushParent(ui_state->tooltip_root);
   UI_PushFlags(0);
   UI_PushTextRasterFlags(UI_BottomTextRasterFlags());
   UI_PushFontSize(UI_BottomFontSize());
   UI_PushColorTag(Str8Lit("."));
   UI_PushColorTag(Str8Lit("floating"));
}

void UI_TooltipEndBase(void) {
   UI_PopColorTag();
   UI_PopColorTag();
   UI_PopFontSize();
   UI_PopTextRasterFlags();
   UI_PopFlags();
   UI_PopParent();
   UI_PopParent();
}

void UI_TooltipBegin(void) {
   UI_TooltipBeginBase();
   UI_SetNextSquish(0.1f - ui_state->tooltip_open_t * 0.1f);
   UI_SetNextTransparency(1 - ui_state->tooltip_open_t);
   UI_Flags(UI_BoxFlag_Floating | UI_BoxFlag_DrawBorder | UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawBackgroundBlur | UI_BoxFlag_DrawDropShadow | UI_BoxFlag_SquishAnchored)
       UI_PrefWidth(UI_ChildrenSum(1))
           UI_PrefHeight(UI_ChildrenSum(1))
               UI_CornerRadius(UI_TopFontSize() * 0.25f)
                   UI_ColumnBegin();
   UI_PrefWidth(UI_Px(0, 1)) UI_Spacer(UI_Em(1.f, 1.f));
   UI_PrefWidth(UI_ChildrenSum(1))
       UI_PrefHeight(UI_ChildrenSum(1))
           UI_RowBegin();
   UI_PrefHeight(UI_Px(0, 1)) UI_Spacer(UI_Em(1.f, 1.f));
   UI_PrefWidth(UI_ChildrenSum(1))
       UI_PrefHeight(UI_ChildrenSum(1))
           UI_ColumnBegin();
   UI_PushPrefWidth(UI_TextDim(10.f, 1.f));
   UI_PushPrefHeight(UI_Em(2.f, 1.f));
   UI_PushTextAlignment(UI_TextAlign_Center);
}

void UI_TooltipEnd(void) {
   UI_PopTextAlignment();
   UI_PopPrefWidth();
   UI_PopPrefHeight();
   UI_ColumnEnd();
   UI_PrefHeight(UI_Px(0, 1)) UI_Spacer(UI_Em(1.f, 1.f));
   UI_RowEnd();
   UI_PrefWidth(UI_Px(0, 1)) UI_Spacer(UI_Em(1.f, 1.f));
   UI_ColumnEnd();
   UI_TooltipEndBase();
}

//- rjf: context menus

void UI_CtxMenuOpen(UI_Key key, UI_Key anchor_box_key, Vec2F32 anchor_off) {
   anchor_off.x = (F32)(int)anchor_off.x;
   anchor_off.y = (F32)(int)anchor_off.y;
   ui_state->next_ctx_menu_open = 1;
   ui_state->ctx_menu_changed = 1;
   ui_state->ctx_menu_open_t = 0;
   ui_state->ctx_menu_key = key;
   ui_state->next_ctx_menu_anchor_key = anchor_box_key;
   ui_state->ctx_menu_anchor_off = anchor_off;
   ui_state->ctx_menu_touched_this_frame = 1;
   ui_state->ctx_menu_anchor_box_last_pos = V2F32(0, 0);
   ui_state->ctx_menu_root->default_nav_focus_active_key = UI_KeyZero();
   ui_state->ctx_menu_root->default_nav_focus_next_active_key = UI_KeyZero();
}

void UI_CtxMenuClose(void) {
   ui_state->next_ctx_menu_open = 0;
}

B32 UI_BeginCtxMenu(UI_Key key) {
   UI_PushParent(UI_RootFromState(ui_state));
   UI_PushParent(ui_state->ctx_menu_root);
   UI_PushPrefWidth(UI_BottomPrefWidth());
   UI_PushPrefHeight(UI_BottomPrefHeight());
   UI_PushFocusHot(UI_FocusKind_Root);
   UI_PushFocusActive(UI_FocusKind_Root);
   UI_PushColorTag(Str8Lit("."));
   B32 is_open = UI_KeyMatch(key, ui_state->ctx_menu_key) && ui_state->ctx_menu_open;
   if (is_open != 0)
      UI_ColorTagF("floating") {
         ui_state->ctx_menu_touched_this_frame = 1;
         ui_state->ctx_menu_root->flags |= UI_BoxFlag_RoundChildrenByParent;
         ui_state->ctx_menu_root->flags |= UI_BoxFlag_DrawBackgroundBlur;
         ui_state->ctx_menu_root->flags |= UI_BoxFlag_DrawBackground;
         ui_state->ctx_menu_root->flags |= UI_BoxFlag_DisableFocusOverlay;
         ui_state->ctx_menu_root->flags |= UI_BoxFlag_DrawBorder;
         ui_state->ctx_menu_root->flags |= UI_BoxFlag_Clip;
         ui_state->ctx_menu_root->flags |= UI_BoxFlag_Clickable;
         ui_state->ctx_menu_root->corner_radii[Corner_00] = ui_state->ctx_menu_root->corner_radii[Corner_01] = ui_state->ctx_menu_root->corner_radii[Corner_10] = ui_state->ctx_menu_root->corner_radii[Corner_11] = UI_TopFontSize() * 0.25f;
         ui_state->ctx_menu_root->tags_key = UI_TopTagsKey();
         ui_state->ctx_menu_root->blur_size = UI_TopBlurSize();
         ui_state->ctx_menu_root->text_color = UI_ColorFromSlot(UI_ColorSlot_Text);
         ui_state->ctx_menu_root->background_color = UI_ThemeColor(Str8Lit("menu"), UI_ColorSlot_Background);
         UI_Spacer(UI_Em(1.f, 1.f));
      }
   ui_state->is_in_open_ctx_menu = is_open;
   return is_open;
}

void UI_EndCtxMenu(void) {
   if (ui_state->is_in_open_ctx_menu) {
      ui_state->is_in_open_ctx_menu = 0;
      UI_Spacer(UI_Em(1.f, 1.f));
   }
   UI_PopColorTag();
   UI_PopFocusActive();
   UI_PopFocusHot();
   UI_PopPrefWidth();
   UI_PopPrefHeight();
   UI_PopParent();
   UI_PopParent();
}

B32 UI_CtxMenuIsOpen(UI_Key key) {
   return (ui_state->ctx_menu_open && UI_KeyMatch(key, ui_state->ctx_menu_key));
}

B32 UI_AnyCtxMenuIsOpen(void) {
   return ui_state->ctx_menu_open;
}

//- rjf: focus tree coloring

B32 UI_IsFocusHot(void) {
   B32 result = (ui_state->focus_hot_stack.top->v == UI_FocusKind_On);
   if (result) {
      for (UI_FocusHotNode* n = ui_state->focus_hot_stack.top; n != 0; n = n->next) {
         if (n->v == UI_FocusKind_Root) {
            break;
         }
         if (n->v == UI_FocusKind_Off) {
            result = 0;
            break;
         }
      }
   }
   return result;
}

B32 UI_IsFocusActive(void) {
   B32 result = (ui_state->focus_active_stack.top->v == UI_FocusKind_On);
   if (result) {
      for (UI_FocusActiveNode* n = ui_state->focus_active_stack.top; n != 0; n = n->next) {
         if (n->v == UI_FocusKind_Root) {
            break;
         }
         if (n->v == UI_FocusKind_Off) {
            result = 0;
            break;
         }
      }
   }
   return result;
}

//- rjf: implicit auto-managed tree-based focus state

B32 UI_IsKeyAutoFocusActive(UI_Key key) {
   B32 result = 0;
   if (!UI_KeyMatch(UI_KeyZero(), key)) {
      for (UI_Box* p = UI_TopParent(); !UI_BoxIsNil(p); p = p->parent) {
         if (p->flags & UI_BoxFlag_FocusActive && UI_KeyMatch(key, p->default_nav_focus_active_key)) {
            result = 1;
            break;
         }
      }
   }
   return result;
}

B32 UI_IsKeyAutoFocusHot(UI_Key key) {
   B32 result = 0;
   if (!UI_KeyMatch(UI_KeyZero(), key)) {
      for (UI_Box* p = UI_TopParent(); !UI_BoxIsNil(p); p = p->parent) {
         if (p->flags & UI_BoxFlag_FocusHot &&
             ((!(p->flags & UI_BoxFlag_FocusHotDisabled) &&
               UI_KeyMatch(key, p->default_nav_focus_hot_key)) ||
              UI_KeyMatch(key, p->default_nav_focus_active_key))) {
            result = 1;
            break;
         }
      }
   }
   return result;
}

void UI_SetAutoFocusActiveKey(UI_Key key) {
   for (UI_Box* p = UI_TopParent(); !UI_BoxIsNil(p); p = p->parent) {
      if (p->flags & UI_BoxFlag_DefaultFocusNav) {
         p->default_nav_focus_next_active_key = key;
         break;
      }
   }
}

void UI_SetAutoFocusHotKey(UI_Key key) {
   for (UI_Box* p = UI_TopParent(); !UI_BoxIsNil(p); p = p->parent) {
      if (p->flags & UI_BoxFlag_DefaultFocusNav) {
         p->default_nav_focus_next_hot_key = key;
         break;
      }
   }
}

//- rjf: current style tags key

UI_Key
UI_TopTagsKey(void) {
   UI_Key key = UI_KeyZero();
   if (ui_state->tags_key_stack_top != 0) {
      key = ui_state->tags_key_stack_top->key;
   }
   return key;
}

//- rjf: theme color lookups

// bgz: the next space separated word of *rest; empty when there are no more.
PRIVATE String8 _UI_NextWord(String8* rest) {
   U64 start = 0;
   for (; start < rest->size && rest->str[start] == ' '; start += 1) {
   }
   U64 end = start;
   for (; end < rest->size && rest->str[end] != ' '; end += 1) {
   }
   String8 word = Str8(rest->str + start, end - start);
   *rest        = Str8(rest->str + end, rest->size - end);
   return word;
}

// bgz: whether word is one of the pushed tags or a word of extras.
PRIVATE B32 _UI_TagIsPresent(String8 word, String8Array tags, String8 extras) {
   for EachIndex(idx, tags.count) {
      if (Str8Match(word, tags.v[idx], 0)) {
         return 1;
      }
   }
   for (String8 extra = _UI_NextWord(&extras); extra.size != 0; extra = _UI_NextWord(&extras)) {
      if (Str8Match(word, extra, 0)) {
         return 1;
      }
   }
   return 0;
}

// bgz: the rule in the Themes note in ui_core.h. 0 when the slot has no pattern here.
PRIVATE UI_ThemePattern* _UI_ThemePatternFromTags(UI_Theme* theme, String8Array tags, String8 extras, UI_ColorSlot slot) {
   UI_ThemePattern* pattern          = 0;
   U64              best_match_count = 0;

   for (U64 idx = 0; idx < theme->patterns_count; idx += 1) {
      UI_ThemePattern* p = &theme->patterns[idx];

      if (p->slot == slot) {
         String8 rest        = Str8CString(p->tags);
         U64     match_count = 0;
         B32     all_present = 1;

         for (String8 word = _UI_NextWord(&rest); word.size != 0; word = _UI_NextWord(&rest)) {
            if (_UI_TagIsPresent(word, tags, extras)) {
               match_count += 1;
            }
            else {
               all_present = 0;
            }
         }

         if (all_present && (pattern == 0 || match_count > best_match_count)) {
            pattern          = p;
            best_match_count = match_count;
         }
      }
   }

   return pattern;
}

Vec4F32
UI_ColorFromSlot(UI_ColorSlot slot) {
   Vec4F32 result = UI_ColorFromTagsKeySlot(UI_TopTagsKey(), slot);
   return result;
}

Vec4F32
UI_ThemeColor(String8 tags, UI_ColorSlot slot) {
   Vec4F32 result = UI_ColorFromTagsKeyExtras(UI_TopTagsKey(), tags, slot);
   return result;
}

Vec4F32
UI_ColorFromTagsKeyExtras(UI_Key key, String8 extras, UI_ColorSlot color_slot) {
   Vec4F32 result = {0};
   if (ui_state->theme_pattern_cache_slots_count) {
      //- rjf: compute final key, mixing (tags_key, extras, slot)
      UI_Key final_key = key;
      if (extras.size != 0) {
         final_key = UI_KeyFromString(final_key, extras);
      }
      final_key = UI_KeyFromString(final_key, Str8((U8*)&color_slot, sizeof(color_slot)));

                //- rjf: map to existing node
                U64                       slot_idx = final_key.u64[0] % ui_state->theme_pattern_cache_slots_count;
                UI_ThemePatternCacheSlot* slot = &ui_state->theme_pattern_cache_slots[slot_idx];
                UI_ThemePatternCacheNode* node = 0;
                for (UI_ThemePatternCacheNode* n = slot->first;
                     n != 0;
                     n = n->slot_next) {
                   if (UI_KeyMatch(n->key, final_key)) {
                      node = n;
                   }
                }

                //- rjf: no node, or this node is stale? create and/or update
                if (node == 0 || node->last_build_index_accessed < ui_state->build_index) {
                   // rjf: map tags_key (without name) -> full list of tags
                   String8Array tags = {0};
                   {
                      U64               tags_cache_slot_idx = key.u64[0] % ui_state->tags_cache_slots_count;
                      UI_TagsCacheSlot* tags_cache_slot = &ui_state->tags_cache_slots[tags_cache_slot_idx];
                      for (UI_TagsCacheNode* n = tags_cache_slot->first; n != 0; n = n->next) {
                         if (UI_KeyMatch(n->key, key)) {
                            tags = n->tags;
                            break;
                         }
                      }
                   }

                   // rjf: map tags to theme pattern
                   UI_ThemePattern* pattern = _UI_ThemePatternFromTags(ui_state->theme, tags, extras, color_slot);

                   // rjf: store in (key, slot) -> (pattern) cache
                   B32 node_is_new = 0;
                   if (node == 0) {
                      node_is_new = 1;
                      node = ui_state->theme_pattern_cache_node_free;
                      if (node != 0) {
                         SLLStackPop_N(ui_state->theme_pattern_cache_node_free, slot_next);
                      }
                      else {
                         node = PushArray(ui_state->arena, UI_ThemePatternCacheNode, 1);
                      }
                      DLLPushBack_NP(slot->first, slot->last, node, slot_next, slot_prev);
                      DLLPushBack_NP(ui_state->lru_theme_pattern_cache_node, ui_state->mru_theme_pattern_cache_node, node, lru_next, lru_prev);
                      node->key = final_key;
                   }

                   // rjf: update node's target color
                   if (pattern != 0) {
                      node->target_rgba = pattern->color;
                      if (node_is_new) {
                         node->current_rgba = node->target_rgba;
                      }
                   }
                }

                //- rjf: mark this node as most-recently-used
                if (node != 0 && node->last_build_index_accessed < ui_state->build_index) {
                   node->last_build_index_accessed = ui_state->build_index;
                   DLLRemove_NP(ui_state->lru_theme_pattern_cache_node, ui_state->mru_theme_pattern_cache_node, node, lru_next, lru_prev);
                   DLLPushBack_NP(ui_state->lru_theme_pattern_cache_node, ui_state->mru_theme_pattern_cache_node, node, lru_next, lru_prev);
                }

                //- rjf: grab resultant color
                if (node != 0) {
                   result = node->current_rgba;
                }
   }
   return result;
}

Vec4F32
UI_ColorFromTagsKeySlot(UI_Key key, UI_ColorSlot slot) {
   Vec4F32 result = UI_ColorFromTagsKeyExtras(key, Str8Lit(""), slot);
   return result;
}

//- rjf: box node construction

UI_Box*
UI_BuildBoxFromKey(UI_BoxFlags flags, UI_Key key) {
   ui_state->build_box_count += 1;

   //- rjf: grab active parent
   UI_Box* parent = UI_TopParent();

   //- rjf: try to get box
   UI_BoxFlags last_flags = 0;
   UI_Box*     box = UI_BoxFromKey(key);
   B32         box_first_frame = UI_BoxIsNil(box);
   last_flags = box->flags;

   //- rjf: zero key on duplicate
   if (!box_first_frame && box->last_touched_build_index == ui_state->build_index) {
      box = &ui_nil_box;
      key = UI_KeyZero();
      box_first_frame = 1;
   }

   //- rjf: gather info from box
   B32 box_is_transient = UI_KeyMatch(key, UI_KeyZero());

   //- rjf: allocate box if it doesn't yet exist
   if (box_first_frame) {
      box = !box_is_transient ? ui_state->first_free_box : 0;
      ui_state->is_animating = ui_state->is_animating || !box_is_transient;
      if (!UI_BoxIsNil(box)) {
         SLLStackPop(ui_state->first_free_box);
      }
      else {
         box = PushArrayNoZero(box_is_transient ? UI_BuildArena() : ui_state->arena, UI_Box, 1);
      }
      MemoryZeroStruct(box);
   }

   //- rjf: zero out per-frame state
   {
      box->first = box->last = box->next = box->prev = box->parent = &ui_nil_box;
      box->child_count = 0;
      box->flags = 0;
      box->hover_cursor = WM_Cursor_Pointer;
      MemoryZeroArray(box->pref_size);
      MemoryZeroStruct(&box->draw_bucket);
   }

   //- rjf: hook into persistent state table
   if (box_first_frame && !box_is_transient) {
      U64 slot = key.u64[0] % ui_state->box_table_size;
      DLLInsert_NPZ(&ui_nil_box, ui_state->box_table[slot].hash_first, ui_state->box_table[slot].hash_last, ui_state->box_table[slot].hash_last, box, hash_next, hash_prev);
   }

   //- rjf: hook into per-frame tree structure
   if (!UI_BoxIsNil(parent)) {
      DLLPushBack_NPZ(&ui_nil_box, parent->first, parent->last, box, next, prev);
      parent->child_count += 1;
      box->parent = parent;
   }

   //- rjf: fill box
   {
      box->key = key;
      box->flags = (flags | ui_state->flags_stack.top->v) & ~ui_state->omit_flags_stack.top->v;
      box->fastpath_codepoint = ui_state->fastpath_codepoint_stack.top->v;
      box->group_key = ui_state->group_key_stack.top->v;

      if (UI_IsFocusActive() && (box->flags & UI_BoxFlag_DefaultFocusNav) && UI_KeyMatch(ui_state->default_nav_root_key, UI_KeyZero())) {
         ui_state->default_nav_root_key = box->key;
      }

      if (box_first_frame) {
         box->first_touched_build_index = ui_state->build_index;
         box->disabled_t = (F32) !!(box->flags & UI_BoxFlag_Disabled);
      }
      box->last_touched_build_index = ui_state->build_index;

      if (box->flags & UI_BoxFlag_Disabled && (!(last_flags & UI_BoxFlag_Disabled) || box_first_frame)) {
         box->first_disabled_build_index = ui_state->build_index;
      }

      if (ui_state->fixed_x_stack.top != &ui_state->fixed_x_nil_stack_top) {
         box->flags |= UI_BoxFlag_FloatingX;
         box->fixed_position.x = ui_state->fixed_x_stack.top->v;
      }
      if (ui_state->fixed_y_stack.top != &ui_state->fixed_y_nil_stack_top) {
         box->flags |= UI_BoxFlag_FloatingY;
         box->fixed_position.y = ui_state->fixed_y_stack.top->v;
      }
      if (ui_state->fixed_width_stack.top != &ui_state->fixed_width_nil_stack_top) {
         box->flags |= UI_BoxFlag_FixedWidth;
         box->fixed_size.x = ui_state->fixed_width_stack.top->v;
      }
      else {
         box->pref_size[Axis2_X] = ui_state->pref_width_stack.top->v;
      }
      if (ui_state->fixed_height_stack.top != &ui_state->fixed_height_nil_stack_top) {
         box->flags |= UI_BoxFlag_FixedHeight;
         box->fixed_size.y = ui_state->fixed_height_stack.top->v;
      }
      else {
         box->pref_size[Axis2_Y] = ui_state->pref_height_stack.top->v;
      }
      box->min_size.v[Axis2_X] = ui_state->min_width_stack.top->v;
      box->min_size.v[Axis2_Y] = ui_state->min_height_stack.top->v;

      B32 is_auto_focus_active = UI_IsKeyAutoFocusActive(key);
      B32 is_auto_focus_hot = UI_IsKeyAutoFocusHot(key);
      if (is_auto_focus_active) {
         UI_SetNextFocusActive(UI_FocusKind_On);
      }
      if (is_auto_focus_hot) {
         UI_SetNextFocusHot(UI_FocusKind_On);
      }
      box->flags |= UI_BoxFlag_FocusHot * (ui_state->focus_hot_stack.top->v == UI_FocusKind_On);
      box->flags |= UI_BoxFlag_FocusActive * (ui_state->focus_active_stack.top->v == UI_FocusKind_On);
      if (box->flags & UI_BoxFlag_FocusHot && !UI_IsFocusHot()) {
         box->flags |= UI_BoxFlag_FocusHotDisabled;
      }
      if (box->flags & UI_BoxFlag_FocusActive && !UI_IsFocusActive()) {
         box->flags |= UI_BoxFlag_FocusActiveDisabled;
      }

      box->text_align = ui_state->text_alignment_stack.top->v;
      box->child_layout_axis = ui_state->child_layout_axis_stack.top->v;
      box->font = ui_state->font_stack.top->v;
      box->font_size = ui_state->font_size_stack.top->v;
      box->tab_size = ui_state->tab_size_stack.top->v;
      box->text_raster_flags = ui_state->text_raster_flags_stack.top->v;
      box->corner_radii[Corner_00] = ui_state->corner_radius_00_stack.top->v;
      box->corner_radii[Corner_01] = ui_state->corner_radius_01_stack.top->v;
      box->corner_radii[Corner_10] = ui_state->corner_radius_10_stack.top->v;
      box->corner_radii[Corner_11] = ui_state->corner_radius_11_stack.top->v;
      box->blur_size = ui_state->blur_size_stack.top->v;
      box->transparency = ui_state->transparency_stack.top->v;
      box->squish = ui_state->squish_stack.top->v;
      box->text_padding = ui_state->text_padding_stack.top->v;
      box->hover_cursor = ui_state->hover_cursor_stack.top->v;
      box->custom_draw = 0;
      box->tags_key = UI_KeyZero();
      if (ui_state->tags_key_stack_top != 0) {
         box->tags_key = ui_state->tags_key_stack_top->key;
      }
      if (box->flags & UI_BoxFlag_DrawBackground) {
         if (ui_state->background_color_stack.top != &ui_state->background_color_nil_stack_top) {
            box->background_color = ui_state->background_color_stack.top->v;
         }
         else {
            box->background_color = UI_ColorFromSlot(UI_ColorSlot_Background);
         }
      }
      if (box->flags & UI_BoxFlag_DrawText) {
         if (ui_state->text_color_stack.top != &ui_state->text_color_nil_stack_top) {
            box->text_color = ui_state->text_color_stack.top->v;
         }
         else {
            box->text_color = UI_ColorFromSlot(UI_ColorSlot_Text);
         }
      }
      if (box->flags & (UI_BoxFlag_DrawBorder |
                        UI_BoxFlag_DrawSideRight |
                        UI_BoxFlag_DrawSideLeft |
                        UI_BoxFlag_DrawSideTop |
                        UI_BoxFlag_DrawSideBottom)) {
         if (ui_state->border_color_stack.top != &ui_state->border_color_nil_stack_top) {
            box->border_color = ui_state->border_color_stack.top->v;
         }
         else {
            box->border_color = UI_ColorFromSlot(UI_ColorSlot_Border);
         }
      }
   }

   //- rjf: auto-pop all stacks
   {
      UI_AutoPopStacks(ui_state);
   }

   //- rjf: return
   return box;
}

UI_Key
UI_ActiveSeedKey(void) {
   UI_Box* keyed_ancestor = &ui_nil_box;
   {
      for (UI_Box* p = UI_TopParent(); !UI_BoxIsNil(p); p = p->parent) {
         if (!UI_KeyMatch(UI_KeyZero(), p->key)) {
            keyed_ancestor = p;
            break;
         }
      }
   }
   return keyed_ancestor->key;
}

UI_Box*
UI_BuildBoxFromString(UI_BoxFlags flags, String8 string) {
   //- rjf: figure out key
   UI_Key key = UI_KeyFromString(UI_ActiveSeedKey(), string);

   //- rjf: build box from key, equip passed string
   UI_Box* box = UI_BuildBoxFromKey(flags, key);
   if (flags & UI_BoxFlag_DrawText) {
      UI_BoxEquipDisplayString(box, string);
   }

   //- rjf: return
   return box;
}

UI_Box*
UI_BuildBoxFromStringf(UI_BoxFlags flags, char* fmt, ...) {
   Temp    scratch = ScratchBegin(0, 0);
   va_list args;
   va_start(args, fmt);
   String8 string = PushStr8FV(scratch.arena, fmt, args);
   va_end(args);
   UI_Box* box = UI_BuildBoxFromString(flags, string);
   ScratchEnd(scratch);
   return box;
}

//- rjf: box node equipment

void UI_BoxEquipDisplayString(UI_Box* box, String8 string) {
   box->string = PushStr8Copy(UI_BuildArena(), string);
   box->flags |= UI_BoxFlag_HasDisplayString;
   Vec4F32 text_color = box->text_color;
   if (box->flags & UI_BoxFlag_DrawText && (box->fastpath_codepoint == 0 || !(box->flags & UI_BoxFlag_DrawTextFastpathCodepoint))) {
      String8     display_string = UI_BoxDisplayString(box);
      DR_FStrNode fstr_n = {0, {display_string, {box->font, box->text_raster_flags, text_color, box->font_size, 0, 0}}};
      DR_FStrList fstrs = {&fstr_n, &fstr_n, 1};
      box->display_fstrs = Core_D_FStrs_Copy(UI_BuildArena(), &fstrs);
      box->display_fruns = Core_D_FStrs_Fruns(UI_BuildArena(), box->tab_size, &box->display_fstrs);
   }
   else if (box->flags & UI_BoxFlag_DrawText && box->flags & UI_BoxFlag_DrawTextFastpathCodepoint && box->fastpath_codepoint != 0) {
      Temp     scratch = ScratchBegin(0, 0);
      String8  display_string = UI_BoxDisplayString(box);
      String32 fpcp32 = Str32(&box->fastpath_codepoint, 1);
      String8  fpcp = Str8From32(scratch.arena, fpcp32);
      U64      fpcp_pos = Str8FindNeedle(display_string, 0, fpcp, StringMatchFlag_CaseInsensitive);
      if (fpcp_pos < display_string.size) {
         DR_FStrNode pst_fstr_n = {0, {Str8Skip(display_string, fpcp_pos + fpcp.size), {box->font, box->text_raster_flags, text_color, box->font_size, 0, 0}}};
         DR_FStrNode cdp_fstr_n = {&pst_fstr_n, {Str8Substr(display_string, R1U64(fpcp_pos, fpcp_pos + fpcp.size)), {box->font, box->text_raster_flags, text_color, box->font_size, 3.f, 0}}};
         DR_FStrNode pre_fstr_n = {&cdp_fstr_n, {Str8Prefix(display_string, fpcp_pos), {box->font, box->text_raster_flags, text_color, box->font_size, 0, 0}}};
         DR_FStrList fstrs = {&pre_fstr_n, &pst_fstr_n, 3};
         box->display_fstrs = Core_D_FStrs_Copy(UI_BuildArena(), &fstrs);
         box->display_fruns = Core_D_FStrs_Fruns(UI_BuildArena(), box->tab_size, &box->display_fstrs);
      }
      else {
         DR_FStrNode fstr_n = {0, {display_string, {box->font, box->text_raster_flags, text_color, box->font_size, 0, 0}}};
         DR_FStrList fstrs = {&fstr_n, &fstr_n, 1};
         box->display_fstrs = Core_D_FStrs_Copy(UI_BuildArena(), &fstrs);
         box->display_fruns = Core_D_FStrs_Fruns(UI_BuildArena(), box->tab_size, &box->display_fstrs);
      }
      ScratchEnd(scratch);
   }
}

void UI_BoxEquipDisplayFstrs(UI_Box* box, DR_FStrList* strings) {
   box->flags |= UI_BoxFlag_HasDisplayString;
   box->string = Core_D_FStrs_String(UI_BuildArena(), strings);
   box->display_fstrs = Core_D_FStrs_Copy(UI_BuildArena(), strings);
   box->display_fruns = Core_D_FStrs_Fruns(UI_BuildArena(), box->tab_size, &box->display_fstrs);
}

inline void UI_BoxEquipFuzzyMatchRanges(UI_Box* box, FuzzyMatchRangeList* matches) {
   box->flags |= UI_BoxFlag_HasFuzzyMatchRanges;
   box->fuzzy_match_ranges = FuzzyMatchRangeListCopy(UI_BuildArena(), matches);
}

void UI_BoxEquipDrawBucket(UI_Box* box, D_Bucket* bucket) {
   box->flags |= UI_BoxFlag_DrawBucket;
   box->draw_bucket = bucket;
}

void UI_BoxEquipCustomDraw(UI_Box* box, UI_BoxCustomDrawFunctionType* custom_draw, void* user_data) {
   box->custom_draw = custom_draw;
   box->custom_draw_user_data = user_data;
}

//- rjf: box accessors / queries

String8
UI_BoxDisplayString(UI_Box* box) {
   String8 result = box->string;
   if (!(box->flags & UI_BoxFlag_DisableIDString)) {
      result = UI_DisplayPartFromKeyString(result);
   }
   return result;
}

Vec2F32
UI_BoxTextPosition(UI_Box* box) {
   Vec2F32     result = {0};
   FNT_Tag     font = box->font;
   F32         font_size = box->font_size;
   FNT_Metrics font_metrics = Core_FNT_MetricsFromTagSize(font, font_size);
   result.y = FloorF32((box->rect.p0.y + box->rect.p1.y) / 2.f + font_metrics.ascent / 2.f - font_metrics.descent / 2.f);
   switch (box->text_align) {
   default:
   case UI_TextAlign_Left: {
      result.x = box->rect.p0.x + box->text_padding;
   } break;
   case UI_TextAlign_Center: {
      Vec2F32 text_dim = box->display_fruns.dim;
      result.x = RoundF32((box->rect.p0.x + box->rect.p1.x) / 2 - text_dim.x / 2);
      result.x = ClampBot(result.x, box->rect.x0);
   } break;
   case UI_TextAlign_Right: {
      Vec2F32 text_dim = box->display_fruns.dim;
      result.x = RoundF32((box->rect.p1.x) - text_dim.x - box->text_padding);
      result.x = ClampBot(result.x, box->rect.x0);
   } break;
   }
   result.x = FloorF32(result.x);
   return result;
}

U64 UI_BoxCharPosFromXy(UI_Box* box, Vec2F32 xy) {
   FNT_Tag font = box->font;
   F32     font_size = box->font_size;
   String8 line = UI_BoxDisplayString(box);
   U64     result = Core_FNT_CharPosFromTagSizeStringP(font, font_size, 0, box->tab_size, line, xy.x - UI_BoxTextPosition(box).x);
   return result;
}

////////////////////////////////
//~ rjf: Box Interaction

UI_Signal
UI_SignalFromBox(UI_Box* box) {
   B32       is_focus_hot = box->flags & UI_BoxFlag_FocusHot && !(box->flags & UI_BoxFlag_FocusHotDisabled);
   UI_Signal sig = {box};
   sig.event_flags |= Core_WM_GetModifiers();

   //////////////////////////////
   //- rjf: calculate possibly-clipped box rectangle
   //
   Rng2F32 rect = box->rect;
   for (UI_Box* b = box->parent; !UI_BoxIsNil(b); b = b->parent) {
      if (b->flags & UI_BoxFlag_Clip) {
         rect = Intersect2F32(rect, b->rect);
      }
   }

   //////////////////////////////
   //- rjf: determine if we're under the context menu or not
   //
   B32 ctx_menu_is_ancestor = 0;
   {
      for (UI_Box* parent = box; !UI_BoxIsNil(parent); parent = parent->parent) {
         if (parent == ui_state->ctx_menu_root) {
            ctx_menu_is_ancestor = 1;
            break;
         }
      }
   }

   //////////////////////////////
   //- rjf: calculate blacklist rectangles
   //
   Rng2F32 blacklist_rect = {0};
   if (!ctx_menu_is_ancestor && ui_state->ctx_menu_open) {
      blacklist_rect = ui_state->ctx_menu_root->rect;
   }

   //////////////////////////////
   //- rjf: process events related to this box
   //
   B32 view_scrolled = 0;
   for (UI_Event* evt = 0; UI_NextEvent(&evt);) {
      B32 taken = 0;

      //- rjf: unpack event
      Vec2F32            evt_mouse = evt->pos;
      B32                evt_mouse_in_bounds = !Contains2F32(blacklist_rect, evt_mouse) && Contains2F32(rect, evt_mouse);
      UI_MouseButtonKind evt_mouse_button_kind = (evt->key == WM_Key_LeftMouseButton ? UI_MouseButtonKind_Left : evt->key == WM_Key_MiddleMouseButton ? UI_MouseButtonKind_Middle
                                                                                                       : evt->key == WM_Key_RightMouseButton    ? UI_MouseButtonKind_Right
                                                                                                                                          : UI_MouseButtonKind_Left);
      B32                evt_key_is_mouse = (evt->key == WM_Key_LeftMouseButton ||
                              evt->key == WM_Key_MiddleMouseButton ||
                              evt->key == WM_Key_RightMouseButton);
      sig.event_flags |= evt->modifiers;

      //- rjf: mouse presses in box -> set hot/active; mark signal accordingly
      if (box->flags & UI_BoxFlag_MouseClickable &&
          evt->kind == UI_EventKind_Press &&
          evt_mouse_in_bounds &&
          evt_key_is_mouse) {
         ui_state->hot_box_key = box->key;
         ui_state->active_box_key[evt_mouse_button_kind] = box->key;
         sig.f |= (UI_SignalFlag_LeftPressed << evt_mouse_button_kind);
         ui_state->drag_start_mouse = evt->pos;
         if (UI_KeyMatch(box->key, ui_state->press_key_history[evt_mouse_button_kind][0]) &&
             evt->timestamp_us - ui_state->press_timestamp_history_us[evt_mouse_button_kind][0] <= 1000000 * Core_WM_GetSystemInfo()->double_click_time) {
            sig.f |= (UI_SignalFlag_LeftDoubleClicked << evt_mouse_button_kind);
         }
         if (UI_KeyMatch(box->key, ui_state->press_key_history[evt_mouse_button_kind][0]) &&
             UI_KeyMatch(box->key, ui_state->press_key_history[evt_mouse_button_kind][1]) &&
             evt->timestamp_us - ui_state->press_timestamp_history_us[evt_mouse_button_kind][0] <= 1000000 * Core_WM_GetSystemInfo()->double_click_time &&
             ui_state->press_timestamp_history_us[evt_mouse_button_kind][0] - ui_state->press_timestamp_history_us[evt_mouse_button_kind][1] <= 1000000 * Core_WM_GetSystemInfo()->double_click_time) {
            sig.f |= (UI_SignalFlag_LeftTripleClicked << evt_mouse_button_kind);
         }
         // @Changed(asan_overlap): MemoryMove, not MemoryCopy - each history
         // shifts down one slot within itself, so source and destination overlap.
         MemoryCopy(&ui_state->press_timestamp_history_us[evt_mouse_button_kind][1], &ui_state->press_timestamp_history_us[evt_mouse_button_kind][0],
                    sizeof(ui_state->press_timestamp_history_us[evt_mouse_button_kind][0]) * (ArrayCount(ui_state->press_timestamp_history_us[evt_mouse_button_kind]) - 1));
         MemoryCopy(&ui_state->press_key_history[evt_mouse_button_kind][1], &ui_state->press_key_history[evt_mouse_button_kind][0],
                    sizeof(ui_state->press_key_history[evt_mouse_button_kind][0]) * (ArrayCount(ui_state->press_key_history[evt_mouse_button_kind]) - 1));
         MemoryCopy(&ui_state->press_pos_history[evt_mouse_button_kind][1], &ui_state->press_pos_history[evt_mouse_button_kind][0],
                    sizeof(ui_state->press_pos_history[evt_mouse_button_kind][0]) * (ArrayCount(ui_state->press_pos_history[evt_mouse_button_kind]) - 1));
         ui_state->press_timestamp_history_us[evt_mouse_button_kind][0] = evt->timestamp_us;
         ui_state->press_key_history[evt_mouse_button_kind][0] = box->key;
         ui_state->press_pos_history[evt_mouse_button_kind][0] = evt_mouse;
         taken = 1;
      }

      //- rjf: mouse releases in active box -> unset active; mark signal accordingly
      if (box->flags & UI_BoxFlag_MouseClickable &&
          evt->kind == UI_EventKind_Release &&
          UI_KeyMatch(ui_state->active_box_key[evt_mouse_button_kind], box->key) &&
          evt_mouse_in_bounds &&
          evt_key_is_mouse) {
         ui_state->active_box_key[evt_mouse_button_kind] = UI_KeyZero();
         sig.f |= (UI_SignalFlag_LeftReleased << evt_mouse_button_kind);
         sig.f |= (UI_SignalFlag_LeftClicked << evt_mouse_button_kind);
         taken = 1;
      }

      //- rjf: mouse releases outside active box -> unset hot/active
      if (box->flags & UI_BoxFlag_MouseClickable &&
          evt->kind == UI_EventKind_Release &&
          UI_KeyMatch(ui_state->active_box_key[evt_mouse_button_kind], box->key) &&
          !evt_mouse_in_bounds &&
          evt_key_is_mouse) {
         ui_state->hot_box_key = UI_KeyZero();
         ui_state->active_box_key[evt_mouse_button_kind] = UI_KeyZero();
         sig.f |= (UI_SignalFlag_LeftReleased << evt_mouse_button_kind);
         taken = 1;
      }

      //- rjf: focus is hot & keyboard click -> mark signal
      if (box->flags & UI_BoxFlag_KeyboardClickable &&
          is_focus_hot &&
          evt->kind == UI_EventKind_Press &&
          evt->slot == UI_EventActionSlot_Accept) {
         sig.f |= UI_SignalFlag_KeyboardPressed;
         taken = 1;
      }

      //- rjf: focus is hot & copy event -> remember to copy this box tree's text content
      if (is_focus_hot &&
          evt->flags & UI_EventFlag_Copy &&
          !(evt->flags & UI_EventFlag_Delete) &&
          !UI_KeyMatch(UI_KeyZero(), box->key)) {
         ui_state->clipboard_copy_key = box->key;
         taken = 1;
      }

      //- rjf: ancestor is focused & fastpath codepoint pressed -> press
      if (box->flags & UI_BoxFlag_Clickable && box->fastpath_codepoint != 0 && evt->string.size != 0) {
         B32 ancestor_is_focused = 0;
         for (UI_Box* parent = box->parent; !UI_BoxIsNil(parent); parent = parent->parent) {
            if (parent->flags & UI_BoxFlag_FocusActive) {
               ancestor_is_focused = 1;
               if (parent->flags & UI_BoxFlag_FocusActiveDisabled ||
                   !UI_KeyMatch(parent->default_nav_focus_active_key, UI_KeyZero())) {
                  ancestor_is_focused = 0;
                  break;
               }
            }
         }
         if (ancestor_is_focused) {
            Temp     scratch = ScratchBegin(0, 0);
            String32 insertion32 = Str32From8(scratch.arena, evt->string);
            if (insertion32.size == 1 && insertion32.str[0] == box->fastpath_codepoint) {
               taken = 1;
               sig.f |= UI_SignalFlag_Clicked | UI_SignalFlag_Pressed;
            }
            ScratchEnd(scratch);
         }
      }

      //- rjf: scrolling
      if (box->flags & UI_BoxFlag_Scroll &&
          evt->kind == UI_EventKind_Scroll &&
          (evt->modifiers == 0 || evt->modifiers == WM_Modifier_Shift) &&
          evt_mouse_in_bounds) {
         Vec2F32 delta = evt->delta_2f32;
         if (evt->modifiers & WM_Modifier_Shift) {
            Swap(F32, delta.x, delta.y);
         }
         // bgz: a wheel notch is 120; it scrolls scroll_rows_per_notch rows.
         S64     rows_per_notch = ui_state->animation_info.scroll_rows_per_notch > 0 ? ui_state->animation_info.scroll_rows_per_notch : 4;
         F32     rows_per_unit  = (F32)rows_per_notch / 120.f;
         Vec2S16 delta16        = V2S16((S16)(delta.x * rows_per_unit), (S16)(delta.y * rows_per_unit));
         if (delta.x > 0 && delta16.x == 0) {
            delta16.x = +1;
         }
         if (delta.x < 0 && delta16.x == 0) {
            delta16.x = -1;
         }
         if (delta.y > 0 && delta16.y == 0) {
            delta16.y = +1;
         }
         if (delta.y < 0 && delta16.y == 0) {
            delta16.y = -1;
         }
         sig.scroll.x += delta16.x;
         sig.scroll.y += delta16.y;
         taken = 1;
      }

      //- rjf: view scrolling
      if (box->flags & UI_BoxFlag_ViewScroll && box->first_touched_build_index != box->last_touched_build_index &&
          evt->kind == UI_EventKind_Scroll &&
          (evt->modifiers == 0 || evt->modifiers == WM_Modifier_Shift) &&
          evt_mouse_in_bounds) {
         Vec2F32 delta = evt->delta_2f32;
         if (evt->modifiers & WM_Modifier_Shift) {
            Swap(F32, delta.x, delta.y);
         }
         if (!(box->flags & UI_BoxFlag_ViewScrollX)) {
            if (delta.y == 0) {
               delta.y = delta.x;
            }
            delta.x = 0;
         }
         if (!(box->flags & UI_BoxFlag_ViewScrollY)) {
            if (delta.x == 0) {
               delta.x = delta.y;
            }
            delta.y = 0;
         }
         box->view_off_target.x += delta.x;
         box->view_off_target.y += delta.y;
         view_scrolled = 1;
         taken = 1;
      }

      //- rjf: taken -> eat event
      if (taken) {
         UI_EatEvent(evt);
      }
   }

   //////////////////////////////
   //- rjf: clamp view scrolling
   //
   if (view_scrolled && box->flags & UI_BoxFlag_ViewClamp) {
      Vec2F32 max_view_off_target =
          {{
              ClampBot(0, box->view_bounds.x - box->fixed_size.x),
              ClampBot(0, box->view_bounds.y - box->fixed_size.y),
          }};
      if (box->flags & UI_BoxFlag_ViewClampX) {
         box->view_off_target.x = Clamp(0, box->view_off_target.x, max_view_off_target.x);
      }
      if (box->flags & UI_BoxFlag_ViewClampY) {
         box->view_off_target.y = Clamp(0, box->view_off_target.y, max_view_off_target.y);
      }
   }

   //////////////////////////////
   //- rjf: active -> dragging
   //
   if (box->flags & UI_BoxFlag_MouseClickable) {
      for EachEnumVal(UI_MouseButtonKind, k) {
         if (UI_KeyMatch(ui_state->active_box_key[k], box->key) ||
             sig.f & (UI_SignalFlag_LeftPressed << k)) {
            sig.f |= (UI_SignalFlag_LeftDragging << k);
         }
      }
   }

   //////////////////////////////
   //- rjf: dragging started via double-click -> double-dragging
   //
   if (box->flags & UI_BoxFlag_MouseClickable) {
      for EachEnumVal(UI_MouseButtonKind, k) {
         if (sig.f & (UI_SignalFlag_LeftDragging << k) &&
             UI_KeyMatch(ui_state->press_key_history[k][0], box->key) &&
             UI_KeyMatch(ui_state->press_key_history[k][1], box->key) &&
             ui_state->press_timestamp_history_us[k][0] - ui_state->press_timestamp_history_us[k][1] <= 1000000 * Core_WM_GetSystemInfo()->double_click_time &&
             Length2F32(Sub2F32(ui_state->press_pos_history[k][0], ui_state->press_pos_history[k][1])) < 10.f) {
            sig.f |= (UI_SignalFlag_LeftDoubleDragging << k);
         }
      }
   }

   //////////////////////////////
   //- rjf: dragging started via triple-click -> triple-dragging
   //
   if (box->flags & UI_BoxFlag_MouseClickable) {
      for EachEnumVal(UI_MouseButtonKind, k) {
         if (sig.f & (UI_SignalFlag_LeftDragging << k) &&
             UI_KeyMatch(ui_state->press_key_history[k][0], box->key) &&
             UI_KeyMatch(ui_state->press_key_history[k][1], box->key) &&
             UI_KeyMatch(ui_state->press_key_history[k][2], box->key) &&
             ui_state->press_timestamp_history_us[k][0] - ui_state->press_timestamp_history_us[k][1] <= 1000000 * Core_WM_GetSystemInfo()->double_click_time &&
             ui_state->press_timestamp_history_us[k][1] - ui_state->press_timestamp_history_us[k][2] <= 1000000 * Core_WM_GetSystemInfo()->double_click_time &&
             Length2F32(Sub2F32(ui_state->press_pos_history[k][0], ui_state->press_pos_history[k][1])) < 10.f &&
             Length2F32(Sub2F32(ui_state->press_pos_history[k][1], ui_state->press_pos_history[k][2])) < 10.f) {
            sig.f |= (UI_SignalFlag_LeftTripleDragging << k);
         }
      }
   }

   //////////////////////////////
   //- rjf: mouse is over this box's rect -> always mark mouse-over
   //
   {
      if (Contains2F32(rect, ui_state->mouse) &&
          !Contains2F32(blacklist_rect, ui_state->mouse)) {
         sig.f |= UI_SignalFlag_MouseOver;
      }
   }

   //////////////////////////////
   //- rjf: mouse is over this box's rect, no other hot key? -> set hot key, mark hovering
   //
   {
      if (box->flags & UI_BoxFlag_MouseClickable &&
          Contains2F32(rect, ui_state->mouse) &&
          !Contains2F32(blacklist_rect, ui_state->mouse) &&
          (UI_KeyMatch(ui_state->hot_box_key, UI_KeyZero()) || UI_KeyMatch(ui_state->hot_box_key, box->key)) &&
          (UI_KeyMatch(ui_state->active_box_key[UI_MouseButtonKind_Left], UI_KeyZero()) || UI_KeyMatch(ui_state->active_box_key[UI_MouseButtonKind_Left], box->key)) &&
          (UI_KeyMatch(ui_state->active_box_key[UI_MouseButtonKind_Middle], UI_KeyZero()) || UI_KeyMatch(ui_state->active_box_key[UI_MouseButtonKind_Middle], box->key)) &&
          (UI_KeyMatch(ui_state->active_box_key[UI_MouseButtonKind_Right], UI_KeyZero()) || UI_KeyMatch(ui_state->active_box_key[UI_MouseButtonKind_Right], box->key))) {
         ui_state->hot_box_key = box->key;
         sig.f |= UI_SignalFlag_Hovering;
      }
   }

   //////////////////////////////
   //- rjf: mouse is over this box's rect, currently-active-key has the same group key? -> set hot/active key
   //
   if (box->flags & UI_BoxFlag_MouseClickable &&
       Contains2F32(rect, ui_state->mouse) &&
       !Contains2F32(blacklist_rect, ui_state->mouse) &&
       !UI_KeyMatch(UI_KeyZero(), box->group_key)) {
      for EachEnumVal(UI_MouseButtonKind, k) {
         UI_Box* active_box = UI_BoxFromKey(ui_state->active_box_key[k]);
         if (UI_KeyMatch(box->group_key, active_box->group_key)) {
            ui_state->hot_box_key       = box->key;
            ui_state->active_box_key[k] = box->key;
            sig.f |= UI_SignalFlag_Hovering | (UI_SignalFlag_Dragging << k);
         }
      }
   }

   //////////////////////////////
   //- rjf: mouse is over this box's rect, drop site, no other drop hot key? -> set drop hot key
   //
   {
      if (box->flags & UI_BoxFlag_DropSite &&
          Contains2F32(rect, ui_state->mouse) &&
          !Contains2F32(blacklist_rect, ui_state->mouse) &&
          (UI_KeyMatch(ui_state->drop_hot_box_key, UI_KeyZero()) || UI_KeyMatch(ui_state->drop_hot_box_key, box->key))) {
         ui_state->drop_hot_box_key = box->key;
      }
   }

   //////////////////////////////
   //- rjf: mouse is not over this box's rect, but this is the drop hot key? -> zero drop hot key
   //
   {
      if (box->flags & UI_BoxFlag_DropSite &&
          (!Contains2F32(rect, ui_state->mouse) ||
           Contains2F32(blacklist_rect, ui_state->mouse)) &&
          UI_KeyMatch(ui_state->drop_hot_box_key, box->key)) {
         ui_state->drop_hot_box_key = UI_KeyZero();
      }
   }

   //////////////////////////////
   //- rjf: clicking on something outside the context menu kills the context menu
   //
   if (!ctx_menu_is_ancestor && sig.f & (UI_SignalFlag_LeftPressed | UI_SignalFlag_RightPressed | UI_SignalFlag_MiddlePressed)) {
      UI_CtxMenuClose();
   }

   //////////////////////////////
   //- rjf: get default nav ancestor
   //
   UI_Box* default_nav_parent = &ui_nil_box;
   for (UI_Box* p = UI_TopParent(); !UI_BoxIsNil(p); p = p->parent) {
      if (p->flags & UI_BoxFlag_DefaultFocusNav) {
         default_nav_parent = p;
         break;
      }
   }

   //////////////////////////////
   //- rjf: clicking in default nav -> set navigation state to this box
   //
   if (box->flags & UI_BoxFlag_ClickToFocus && sig.f & UI_SignalFlag_Pressed && !UI_BoxIsNil(default_nav_parent)) {
      default_nav_parent->default_nav_focus_next_hot_key = box->key;
      if (!UI_KeyMatch(default_nav_parent->default_nav_focus_active_key, box->key)) {
         default_nav_parent->default_nav_focus_next_active_key = UI_KeyZero();
      }
   }

   return sig;
}

////////////////////////////////
//~ rjf: Animation Cache Interaction API

F32 UI_Anim_(UI_Key key, UI_AnimParams* params) {
   // rjf: get animation cache node
   UI_AnimNode* node = &ui_nil_anim_node;
   if (ui_state != 0) {
      U64          slot_idx = key.u64[0] % ui_state->anim_slots_count;
      UI_AnimSlot* slot = &ui_state->anim_slots[slot_idx];
      for (UI_AnimNode* n = slot->first; n != &ui_nil_anim_node && n != 0; n = n->slot_next) {
         if (UI_KeyMatch(n->key, key)) {
            node = n;
            break;
         }
      }
      if (node == &ui_nil_anim_node) {
         node = ui_state->free_anim_node;
         if (node != 0) {
            SLLStackPop_N(ui_state->free_anim_node, slot_next);
         }
         else {
            node = PushArray(ui_state->arena, UI_AnimNode, 1);
         }
         node->first_touched_build_index = ui_state->build_index;
         node->key = key;
         MemoryCopyStruct(&node->params, params);
         node->current = params->initial;
         DLLPushBack_NPZ(&ui_nil_anim_node, slot->first, slot->last, node, slot_next, slot_prev);
      }
      else {
         DLLRemove_NPZ(&ui_nil_anim_node, ui_state->lru_anim_node, ui_state->mru_anim_node, node, lru_next, lru_prev);
      }
   }

   // rjf: touch node & update parameters - grab current
   if (node != &ui_nil_anim_node) {
      node->last_touched_build_index = ui_state->build_index;
      DLLPushBack_NPZ(&ui_nil_anim_node, ui_state->lru_anim_node, ui_state->mru_anim_node, node, lru_next, lru_prev);
      if (params->reset) {
         node->current = params->initial;
      }
      MemoryCopyStruct(&node->params, params);
      if (node->params.epsilon == 0) {
         node->params.epsilon = 0.005f;
      }
      if (node->params.rate == 1 || AbsF32(node->current - node->params.target) < AbsF32(node->params.epsilon)) {
         node->current = node->params.target;
      }
   }
   return node->current;
}

////////////////////////////////
//~ rjf: Stacks

#define UI_StackTopImpl(state, name_upper, name_lower) \
   return state->name_lower##_stack.top->v;

#define UI_StackBottomImpl(state, name_upper, name_lower) \
   return state->name_lower##_stack.bottom_val;

#define UI_StackPushImpl(state, name_upper, name_lower, type, new_value) \
   UI_##name_upper##Node* node = state->name_lower##_stack.free;         \
   if (node != 0) {                                                      \
      SLLStackPop(state->name_lower##_stack.free);                          \
   }                                                                     \
   else {                                                                \
      node = PushArray(UI_BuildArena(), UI_##name_upper##Node, 1);       \
   }                                                                     \
   type old_value = state->name_lower##_stack.top->v;                    \
   node->v = new_value;                                                  \
   SLLStackPush(state->name_lower##_stack.top, node);                       \
   if (node->next == &state->name_lower##_nil_stack_top) {               \
      state->name_lower##_stack.bottom_val = (node->v);                  \
   }                                                                     \
   state->name_lower##_stack.auto_pop = 0;                               \
   state->name_lower##_stack.gen += 1;                                   \
   return old_value;

#define UI_StackPopImpl(state, name_upper, name_lower)            \
   UI_##name_upper##Node* popped = state->name_lower##_stack.top; \
   if (popped != &state->name_lower##_nil_stack_top) {            \
      SLLStackPop(state->name_lower##_stack.top);                    \
      SLLStackPush(state->name_lower##_stack.free, popped);          \
      state->name_lower##_stack.auto_pop = 0;                     \
      state->name_lower##_stack.gen += 1;                         \
   }                                                              \
   return popped->v;

#define UI_StackSetNextImpl(state, name_upper, name_lower, type, new_value) \
   UI_##name_upper##Node* node = state->name_lower##_stack.free;            \
   if (node != 0) {                                                         \
      SLLStackPop(state->name_lower##_stack.free);                             \
   }                                                                        \
   else {                                                                   \
      node = PushArray(UI_BuildArena(), UI_##name_upper##Node, 1);          \
   }                                                                        \
   type old_value = state->name_lower##_stack.top->v;                       \
   node->v = new_value;                                                     \
   SLLStackPush(state->name_lower##_stack.top, node);                          \
   state->name_lower##_stack.auto_pop = 1;                                  \
   state->name_lower##_stack.gen += 1;                                      \
   return old_value;

void UI_PushTagsKeyFromAppendedString(String8 string) {
   B32 is_new_root = Str8Match(Str8Lit("."), string, 0);

   // rjf: generate new key, by combining hash of this new string with the top
   // of the tags key stack
   UI_Key seed_key = {0};
   if (!is_new_root && ui_state->tags_key_stack_top != 0) {
      seed_key = ui_state->tags_key_stack_top->key;
   }
   UI_Key key = seed_key;
   if (!is_new_root && string.size > 0) {
      key = UI_KeyFromString(seed_key, string);
   }

   // rjf: push this new key onto the stack
   {
      UI_TagsKeyStackNode* node = ui_state->tags_key_stack_free;
      if (node != 0) {
         SLLStackPop(ui_state->tags_key_stack_free);
      }
      else {
         node = PushArray(UI_BuildArena(), UI_TagsKeyStackNode, 1);
      }
      SLLStackPush(ui_state->tags_key_stack_top, node);
      node->key = key;
   }

   // rjf: store in tags cache
   if (!is_new_root) {
      U64               slot_idx = key.u64[0] % ui_state->tags_cache_slots_count;
      UI_TagsCacheSlot* slot = &ui_state->tags_cache_slots[slot_idx];
      UI_TagsCacheNode* node = 0;
      for (UI_TagsCacheNode* n = slot->first; n != 0; n = n->next) {
         if (UI_KeyMatch(n->key, key)) {
            node = n;
            break;
         }
      }
      if (node == 0) {
         Temp        scratch = ScratchBegin(0, 0);
         String8List tags = {0};
         if (string.size != 0) {
            Str8ListPush(scratch.arena, &tags, PushStr8Copy(UI_BuildArena(), string));
         }
         for (UI_TagNode* n = ui_state->tag_stack.top; n != 0; n = n->next) {
            if (n->v.size == 1 && n->v.str[0] == '.') {
               break;
            }
            if (n->v.size != 0) {
               Str8ListPush(scratch.arena, &tags, PushStr8Copy(UI_BuildArena(), n->v));
            }
         }
         node = PushArray(UI_BuildArena(), UI_TagsCacheNode, 1);
         SLLQueuePush(slot->first, slot->last, node);
         node->key = key;
         node->tags = Str8ArrayFromList(UI_BuildArena(), &tags);
         ScratchEnd(scratch);
      }
   }
}

void UI_PopTagsKey(void) {
   if (ui_state->tags_key_stack_top != 0) {
      UI_TagsKeyStackNode* popped = ui_state->tags_key_stack_top;
      SLLStackPop(ui_state->tags_key_stack_top);
      SLLStackPush(ui_state->tags_key_stack_free, popped);
   }
}

//- rjf: manual implementations

String8
UI_TopColorTag(void){
    UI_StackTopImpl(ui_state, Tag, tag)}

String8
    UI_BottomColorTag(void){
        UI_StackBottomImpl(ui_state, Tag, tag)}

String8
    UI_PushColorTag(String8 v) {
   UI_PushTagsKeyFromAppendedString(v);
   UI_StackPushImpl(ui_state, Tag, tag, String8, PushStr8Copy(UI_BuildArena(), v))
}

String8
UI_PopColorTag(void) {
   UI_PopTagsKey();
   UI_StackPopImpl(ui_state, Tag, tag)
}

String8
UI_SetNextColorTag(String8 v) {
   UI_PushTagsKeyFromAppendedString(v);
   UI_StackSetNextImpl(ui_state, Tag, tag, String8, PushStr8Copy(UI_BuildArena(), v))
}

//- rjf: helpers

Rng2F32
UI_PushRect(Rng2F32 rect) {
   Rng2F32 replaced = {0};
   Vec2F32 size = Dim2F32(rect);
   replaced.x0 = UI_PushFixedX(rect.x0);
   replaced.y0 = UI_PushFixedY(rect.y0);
   replaced.x1 = replaced.x0 + UI_PushFixedWidth(size.x);
   replaced.y1 = replaced.y0 + UI_PushFixedHeight(size.y);
   return replaced;
}

Rng2F32
UI_PopRect(void) {
   Rng2F32 popped = {0};
   popped.x0 = UI_PopFixedX();
   popped.y0 = UI_PopFixedY();
   popped.x1 = popped.x0 + UI_PopFixedWidth();
   popped.y1 = popped.y0 + UI_PopFixedHeight();
   return popped;
}

void UI_SetNextRect(Rng2F32 rect) {
   Vec2F32 size = Dim2F32(rect);
   UI_SetNextFixedX(rect.x0);
   UI_SetNextFixedY(rect.y0);
   UI_SetNextFixedWidth(size.x);
   UI_SetNextFixedHeight(size.y);
}

UI_Size
UI_PushPrefSize(Axis2 axis, UI_Size v) {
   UI_Size result = zero_struct;
   switch (axis) {
   default:
      break;
   case Axis2_X: {
      result = UI_PushPrefWidth(v);
   } break;
   case Axis2_Y: {
      result = UI_PushPrefHeight(v);
   } break;
   }
   return result;
}

UI_Size
UI_PopPrefSize(Axis2 axis) {
   UI_Size result = zero_struct;
   switch (axis) {
   default:
      break;
   case Axis2_X: {
      result = UI_PopPrefWidth();
   } break;
   case Axis2_Y: {
      result = UI_PopPrefHeight();
   } break;
   }
   return result;
}

UI_Size
UI_SetNextPrefSize(Axis2 axis, UI_Size v) {
   return (axis == Axis2_X ? UI_SetNextPrefWidth : UI_SetNextPrefHeight)(v);
}

void UI_PushCornerRadius(F32 v) {
   UI_PushCornerRadius00(v);
   UI_PushCornerRadius01(v);
   UI_PushCornerRadius10(v);
   UI_PushCornerRadius11(v);
}

void UI_PopCornerRadius(void) {
   UI_PopCornerRadius00();
   UI_PopCornerRadius01();
   UI_PopCornerRadius10();
   UI_PopCornerRadius11();
}

void UI_PushColorTagf(char* fmt, ...) {
   Temp    scratch = ScratchBegin(0, 0);
   va_list args;
   va_start(args, fmt);
   String8 string = PushStr8FV(scratch.arena, fmt, args);
   UI_PushColorTag(string);
   va_end(args);
   ScratchEnd(scratch);
}

F32 UI_TopPxHeight(void) {
   F32 result = UI_TopFontSize();
   for (UI_PrefHeightNode* n = ui_state->pref_height_stack.top; n != 0; n = n->next) {
      if (n->v.kind == UI_SizeKind_Pixels) {
         result = n->v.value;
         break;
      }
   }
   return result;
}

////////////////////////////////
//~ rjf: Generated Code

#include "generated/ui.meta.c"
