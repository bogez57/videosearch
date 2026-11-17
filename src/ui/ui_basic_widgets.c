// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

////////////////////////////////
//~ rjf: Basic Widgets

void UI_Divider(UI_Size size) {
   UI_Box* parent = UI_TopParent();
   UI_SetNextPrefSize(parent->child_layout_axis, size);
   UI_SetNextChildLayoutAxis(parent->child_layout_axis);
   UI_Box* box = UI_BuildBoxFromKey(0, UI_KeyZero());
   UI_Parent(box) UI_Padding(UI_Pct(0.5f, 0.5f))
       UI_PrefSize(parent->child_layout_axis, UI_Em(0.1f, 1.f))
           UI_BackgroundColor(UI_ColorFromSlot(UI_ColorSlot_Border)) {
      UI_SetNextChildLayoutAxis(Axis2Flip(parent->child_layout_axis));
      UI_Box* spacing_parent = UI_BuildBoxFromKey(0, UI_KeyZero());
      UI_Parent(spacing_parent) UI_Padding(UI_Em(1.f, 1.f)) UI_PrefSize(Axis2Flip(parent->child_layout_axis), UI_Pct(1, 0)) {
         UI_BuildBoxFromKey(UI_BoxFlag_DrawBackground, UI_KeyZero());
      }
   }
}

UI_Signal
UI_Label(String8 string) {
   UI_Box* box = UI_BuildBoxFromString(UI_BoxFlag_DrawText, Str8Zero());
   UI_BoxEquipDisplayString(box, string);
   UI_Signal interact = UI_SignalFromBox(box);
   return interact;
}

UI_Signal
UI_LabelF(char* fmt, ...) {
   Temp    scratch = ScratchBegin(0, 0);
   va_list args;
   va_start(args, fmt);
   String8 string = PushStr8FV(scratch.arena, fmt, args);
   va_end(args);
   UI_Signal result = UI_Label(string);
   ScratchEnd(scratch);
   return result;
}

UI_Signal
UI_Button(String8 string) {
   UI_Box*   box = UI_BuildBoxFromString(UI_BoxFlag_Clickable |
                                             UI_BoxFlag_DrawBackground |
                                             UI_BoxFlag_DrawBorder |
                                             UI_BoxFlag_DrawText |
                                             UI_BoxFlag_DrawHotEffects |
                                             UI_BoxFlag_DrawActiveEffects,
                                         string);
   UI_Signal interact = UI_SignalFromBox(box);
   return interact;
}

UI_Signal
UI_ButtonF(char* fmt, ...) {
   Temp    scratch = ScratchBegin(0, 0);
   va_list args;
   va_start(args, fmt);
   String8 string = PushStr8FV(scratch.arena, fmt, args);
   va_end(args);
   UI_Signal result = UI_Button(string);
   ScratchEnd(scratch);
   return result;
}

UI_Signal
UI_HoverLabel(String8 string) {
   UI_Box*   box = UI_BuildBoxFromString(UI_BoxFlag_Clickable | UI_BoxFlag_DrawText, string);
   UI_Signal interact = UI_SignalFromBox(box);
   if (UI_Hovering(interact)) {
      box->flags |= UI_BoxFlag_DrawBorder;
   }
   return interact;
}

UI_Signal
UI_HoverLabelF(char* fmt, ...) {
   Temp    scratch = ScratchBegin(0, 0);
   va_list args;
   va_start(args, fmt);
   String8 string = PushStr8FV(scratch.arena, fmt, args);
   va_end(args);
   UI_Signal sig = UI_HoverLabel(string);
   ScratchEnd(scratch);
   return sig;
}

typedef struct UI_LineEditDrawData UI_LineEditDrawData;
struct UI_LineEditDrawData {
   String8 edited_string;
   TxtPt   cursor;
   TxtPt   mark;
   B32     trail;
};

UI_BOX_CUSTOM_DRAW(UI_LineEditDraw) {
   UI_LineEditDrawData* draw_data = (UI_LineEditDrawData*)user_data;
   FNT_Tag              font = box->font;
   F32                  font_size = box->font_size;
   F32                  tab_size = box->tab_size;
   Vec4F32              cursor_color = UI_ColorFromTagsKeySlot(box->tags_key, UI_ColorSlot_Cursor);
   cursor_color.w *= box->parent->parent->focus_active_t;
   Vec4F32 select_color = UI_ColorFromTagsKeySlot(box->tags_key, UI_ColorSlot_Selection);
   select_color.w *= (box->parent->parent->focus_active_t * 0.2f + 0.8f);
   Vec4F32 trail_color = cursor_color;
   trail_color.w *= 0.25f;
   Vec2F32 text_position = UI_BoxTextPosition(box);
   String8 edited_string = draw_data->edited_string;
   TxtPt   cursor = draw_data->cursor;
   TxtPt   mark = draw_data->mark;
   F32     cursor_pixel_off = Core_FNT_DimFromTagSizeString(font, font_size, 0, tab_size, Str8Prefix(edited_string, cursor.column - 1)).x;
   F32     mark_pixel_off = Core_FNT_DimFromTagSizeString(font, font_size, 0, tab_size, Str8Prefix(edited_string, mark.column - 1)).x;
   F32     cursor_thickness = ClampBot(1.f, FloorF32(font_size / 10.f));
   Rng2F32 cursor_rect = R2F32P(text_position.x + cursor_pixel_off,
                                box->parent->parent->rect.y0 + UI_TopFontSize() * 0.5f,
                                text_position.x + cursor_pixel_off + cursor_thickness,
                                box->parent->parent->rect.y1 - UI_TopFontSize() * 0.5f);
   Rng2F32 mark_rect = R2F32P(text_position.x + mark_pixel_off - cursor_thickness,
                              box->parent->parent->rect.y0 + UI_TopFontSize() * 0.5f,
                              text_position.x + mark_pixel_off + cursor_thickness,
                              box->parent->parent->rect.y1 - UI_TopFontSize() * 0.5f);
   Rng2F32 select_rect = Union2F32(cursor_rect, mark_rect);
   Core_D_Rect(select_rect, R2F32(V2F32(0, 0), V2F32(1, 1)), select_color);
   Core_D_Rect(cursor_rect, R2F32(V2F32(0, 0), V2F32(1, 1)), cursor_color);
}

UI_Signal
UI_LineEdit(TxtPt* cursor, TxtPt* mark, U8* edit_buffer, U64 edit_buffer_size, U64* edit_string_size_out, String8 pre_edit_value, String8 string) {
   //- rjf: make key
   UI_Key key = UI_KeyFromString(UI_ActiveSeedKey(), string);

   //- rjf: calculate focus
   B32 is_auto_focus_hot = UI_IsKeyAutoFocusHot(key);
   B32 is_auto_focus_active = UI_IsKeyAutoFocusActive(key);
   UI_PushFocusHot(is_auto_focus_hot ? UI_FocusKind_On : UI_FocusKind_Null);
   UI_PushFocusActive(is_auto_focus_active ? UI_FocusKind_On : UI_FocusKind_Null);
   B32 is_focus_active = UI_IsFocusActive();
   B32 is_focus_active_disabled = (!is_focus_active && UI_TopFocusActive() == UI_FocusKind_On);

   //- rjf: build top-level box
   UI_SetNextHoverCursor(is_focus_active ? WM_Cursor_IBar : WM_Cursor_Pointer);
   UI_Box* box = UI_BuildBoxFromKey(UI_BoxFlag_DrawBackground |
                                        UI_BoxFlag_DrawBorder |
                                        UI_BoxFlag_MouseClickable |
                                        UI_BoxFlag_ClickToFocus |
                                        ((is_auto_focus_hot || is_auto_focus_active) * UI_BoxFlag_KeyboardClickable) |
                                        UI_BoxFlag_DrawHotEffects |
                                        (is_focus_active || is_focus_active_disabled) * (UI_BoxFlag_Clip | UI_BoxFlag_AllowOverflowX | UI_BoxFlag_ViewClamp),
                                    key);

   //- rjf: take navigation actions for editing
   if (is_focus_active) {
      Temp scratch = ScratchBegin(0, 0);
      for (UI_Event* evt = 0; UI_NextEvent(&evt);) {
         String8 edit_string = Str8(edit_buffer, edit_string_size_out[0]);

         // rjf: do not consume anything that doesn't fit a single-line's operations
         if ((evt->kind != UI_EventKind_Edit && evt->kind != UI_EventKind_Navigate && evt->kind != UI_EventKind_Text) || evt->delta_2s32.y != 0) {
            continue;
         }

         // rjf: map this action to an op
         UI_TxtOp op = UI_SingleLineTxtOpFromEvent(scratch.arena, evt, edit_string, *cursor, *mark);

         // rjf: perform replace range
         if (!TxtPtMatch(op.range.min, op.range.max) || op.replace.size != 0) {
            String8 new_string = UI_PushStringReplaceRange(scratch.arena, edit_string, R1S64(op.range.min.column, op.range.max.column), op.replace);
            new_string.size = Min(edit_buffer_size, new_string.size);
            MemoryCopy(edit_buffer, new_string.str, new_string.size);
            edit_string_size_out[0] = new_string.size;
         }

         // rjf: perform copy
         if (op.flags & UI_TxtOpFlag_Copy) {
            Core_WM_SetClipboardText(op.copy);
         }

         // rjf: commit op's changed cursor & mark to caller-provided state
         *cursor = op.cursor;
         *mark = op.mark;

         // rjf: consume event
         UI_EatEvent(evt);
      }
      ScratchEnd(scratch);
   }

   //- rjf: build contents
   TxtPt mouse_pt = {0};
   F32   cursor_off = 0;
   UI_Parent(box) {
      String8 edit_string = Str8(edit_buffer, edit_string_size_out[0]);
      if (!is_focus_active && !is_focus_active_disabled) {
         String8 display_string = UI_DisplayPartFromKeyString(string);
         if (pre_edit_value.size != 0) {
            display_string = pre_edit_value;
         }
         UI_Label(display_string);
      } else {
         F32 total_text_width = Core_FNT_DimFromTagSizeString(UI_TopFont(), UI_TopFontSize(), 0, UI_TopTabSize(), edit_string).x;
         UI_SetNextPrefWidth(UI_Px(total_text_width + UI_TopFontSize() * 5, 1.f));
         UI_Box*              editstr_box = UI_BuildBoxFromStringf(UI_BoxFlag_DrawText | UI_BoxFlag_DisableTextTrunc, "###editstr");
         UI_LineEditDrawData* draw_data = PushArray(UI_BuildArena(), UI_LineEditDrawData, 1);
         draw_data->edited_string = PushStr8Copy(UI_BuildArena(), edit_string);
         draw_data->cursor = *cursor;
         draw_data->mark = *mark;
         draw_data->trail = 1;
         UI_BoxEquipDisplayString(editstr_box, edit_string);
         UI_BoxEquipCustomDraw(editstr_box, UI_LineEditDraw, draw_data);
         mouse_pt = TxtPtMake(1, 1 + UI_BoxCharPosFromXy(editstr_box, UI_Mouse()));
         cursor_off = Core_FNT_DimFromTagSizeString(UI_TopFont(), UI_TopFontSize(), 0, UI_TopTabSize(), Str8Prefix(edit_string, cursor->column - 1)).x;
      }
   }

   //- rjf: interact
   UI_Signal sig = UI_SignalFromBox(box);
   if (!is_focus_active && sig.f & (UI_SignalFlag_DoubleClicked | UI_SignalFlag_KeyboardPressed)) {
      String8 edit_string = pre_edit_value;
      edit_string.size = Min(edit_buffer_size, pre_edit_value.size);
      MemoryCopy(edit_buffer, edit_string.str, edit_string.size);
      edit_string_size_out[0] = edit_string.size;
      UI_SetAutoFocusActiveKey(key);
      UI_KillAction();
      *cursor = TxtPtMake(1, edit_string.size + 1);
      *mark = TxtPtMake(1, 1);
   }
   if (is_focus_active && sig.f & UI_SignalFlag_KeyboardPressed) {
      UI_SetAutoFocusActiveKey(UI_KeyZero());
      sig.f |= UI_SignalFlag_Commit;
   }
   if (is_focus_active && UI_Dragging(sig)) {
      if (UI_Pressed(sig)) {
         *mark = mouse_pt;
      }
      *cursor = mouse_pt;
   }

   //- rjf: focus cursor
   {
      Rng1F32 cursor_range_px = R1F32(cursor_off - UI_TopFontSize() * 2.f, cursor_off + UI_TopFontSize() * 2.f);
      Rng1F32 visible_range_px = R1F32(box->view_off_target.x, box->view_off_target.x + Dim2F32(box->rect).x);
      cursor_range_px.min = ClampBot(0, cursor_range_px.min);
      cursor_range_px.max = ClampBot(0, cursor_range_px.max);
      F32 min_delta = cursor_range_px.min - visible_range_px.min;
      F32 max_delta = cursor_range_px.max - visible_range_px.max;
      min_delta = Min(min_delta, 0);
      max_delta = Max(max_delta, 0);
      box->view_off_target.x += min_delta;
      box->view_off_target.x += max_delta;
   }

   //- rjf: pop focus
   UI_PopFocusHot();
   UI_PopFocusActive();

   return sig;
}

UI_Signal
UI_LineEditF(TxtPt* cursor, TxtPt* mark, U8* edit_buffer, U64 edit_buffer_size, U64* edit_string_size_out, String8 pre_edit_value, char* fmt, ...) {
   Temp    scratch = ScratchBegin(0, 0);
   va_list args;
   va_start(args, fmt);
   String8 string = PushStr8FV(scratch.arena, fmt, args);
   va_end(args);
   UI_Signal result = UI_LineEdit(cursor, mark, edit_buffer, edit_buffer_size, edit_string_size_out, pre_edit_value, string);
   ScratchEnd(scratch);
   return result;
}

////////////////////////////////
//~ rjf: Images

typedef struct UI_ImageDrawData UI_ImageDrawData;
struct UI_ImageDrawData {
   R_Handle          texture;
   R_Tex2DSampleKind sample_kind;
   Rng2F32           region;
   Vec4F32           tint;
   F32               blur;
};

UI_BOX_CUSTOM_DRAW(UI_ImageDraw) {
   UI_ImageDrawData* draw_data = (UI_ImageDrawData*)user_data;
   if (Core_R_HandleMatch(draw_data->texture, Core_R_HandleZero())) {
      R_Rect2DInst* inst = Core_D_Rect(box->rect, R2F32(V2F32(0, 0), V2F32(1, 1)), V4F32(0, 0, 0, 0));
      MemoryCopyArray(inst->corner_radii, box->corner_radii);
   } else {
      R_Rect2DInst* inst = Core_D_Img(box->rect, draw_data->region, draw_data->texture, draw_data->tint);
      MemoryCopyArray(inst->corner_radii, box->corner_radii);
   }
   if (draw_data->blur > 0.01f) {
      Rng2F32 clip = box->rect;
      for (UI_Box* b = box->parent; !UI_BoxIsNil(b); b = b->parent) {
         if (b->flags & UI_BoxFlag_Clip) {
            clip = Intersect2F32(b->rect, clip);
         }
      }
   }
}

UI_Signal
UI_Image(R_Handle texture, R_Tex2DSampleKind sample_kind, Rng2F32 region, Vec4F32 tint, F32 blur, String8 string) {
   UI_Box*           box = UI_BuildBoxFromString(0, string);
   UI_ImageDrawData* draw_data = PushArray(UI_BuildArena(), UI_ImageDrawData, 1);
   draw_data->texture = texture;
   draw_data->sample_kind = sample_kind;
   draw_data->region = region;
   draw_data->tint = tint;
   draw_data->blur = blur;
   UI_BoxEquipCustomDraw(box, UI_ImageDraw, draw_data);
   UI_Signal sig = UI_SignalFromBox(box);
   return sig;
}

UI_Signal
UI_ImageF(R_Handle texture, R_Tex2DSampleKind sample_kind, Rng2F32 region, Vec4F32 tint, F32 blur, char* fmt, ...) {
   Temp    scratch = ScratchBegin(0, 0);
   va_list args;
   va_start(args, fmt);
   String8 string = PushStr8FV(scratch.arena, fmt, args);
   va_end(args);
   UI_Signal result = UI_Image(texture, sample_kind, region, tint, blur, string);
   ScratchEnd(scratch);
   return result;
}

////////////////////////////////
//~ rjf: Special Buttons

UI_Signal
UI_Expander(B32 is_expanded, String8 string) {
   UI_SetNextHoverCursor(WM_Cursor_HandPoint);
   UI_SetNextTextAlignment(UI_TextAlign_Center);
   UI_SetNextFont(UI_IconFont());
   UI_Box* box = UI_BuildBoxFromString(UI_BoxFlag_Clickable | UI_BoxFlag_DrawText, string);
   UI_BoxEquipDisplayString(box, is_expanded ? Str8Lit("v") : Str8Lit(">"));
   UI_Signal sig = UI_SignalFromBox(box);
   return sig;
}

UI_Signal
UI_ExpanderF(B32 is_expanded, char* fmt, ...) {
   Temp    scratch = ScratchBegin(0, 0);
   va_list args;
   va_start(args, fmt);
   String8 string = PushStr8FV(scratch.arena, fmt, args);
   va_end(args);
   UI_Signal sig = UI_Expander(is_expanded, string);
   ScratchEnd(scratch);
   return sig;
}

UI_Signal
UI_SortHeader(B32 sorting, B32 ascending, String8 string) {
   UI_SetNextChildLayoutAxis(Axis2_X);
   UI_SetNextHoverCursor(WM_Cursor_HandPoint);
   UI_Box* box = UI_BuildBoxFromString(UI_BoxFlag_Clickable | UI_BoxFlag_DrawBackground | UI_BoxFlag_DrawActiveEffects, string);
   UI_PushParent(box);

   // rjf: make icon
   if (sorting) {
      UI_SetNextPrefWidth(UI_Em(1.8f, 1.f));
      UI_SetNextTextAlignment(UI_TextAlign_Center);
      UI_SetNextFont(UI_IconFont());
      UI_Box* icon = UI_BuildBoxFromString(UI_BoxFlag_DrawText, Str8Lit(""));
      UI_BoxEquipDisplayString(icon, ascending ? Str8Lit("^") : Str8Lit("v"));
   }

   // rjf: make text
   {
      UI_Label(string);
   }

   UI_PopParent();
   UI_Signal interact = UI_SignalFromBox(box);
   return interact;
}

UI_Signal
UI_SortHeaderF(B32 sorting, B32 ascending, char* fmt, ...) {
   Temp    scratch = ScratchBegin(0, 0);
   va_list args;
   va_start(args, fmt);
   String8 string = PushStr8FV(scratch.arena, fmt, args);
   va_end(args);
   UI_Signal sig = UI_SortHeader(sorting, ascending, string);
   ScratchEnd(scratch);
   return sig;
}

////////////////////////////////
//~ rjf: Simple Layout Widgets

UI_Box*   UI_RowBegin(void) { return UI_NamedRowBegin(Str8Lit("")); }
UI_Signal UI_RowEnd(void) { return UI_NamedRowEnd(); }
UI_Box*   UI_ColumnBegin(void) { return UI_NamedColumnBegin(Str8Lit("")); }
UI_Signal UI_ColumnEnd(void) { return UI_NamedColumnEnd(); }

UI_Box*
UI_NamedRowBegin(String8 string) {
   UI_SetNextChildLayoutAxis(Axis2_X);
   UI_Box* box = UI_BuildBoxFromString(0, string);
   UI_PushParent(box);
   return box;
}

UI_Signal
UI_NamedRowEnd(void) {
   UI_Box*   box = UI_PopParent();
   UI_Signal sig = UI_SignalFromBox(box);
   return sig;
}

UI_Box*
UI_NamedColumnBegin(String8 string) {
   UI_SetNextChildLayoutAxis(Axis2_Y);
   UI_Box* box = UI_BuildBoxFromString(0, string);
   UI_PushParent(box);
   return box;
}

UI_Signal
UI_NamedColumnEnd(void) {
   UI_Box*   box = UI_PopParent();
   UI_Signal sig = UI_SignalFromBox(box);
   return sig;
}

////////////////////////////////
//~ rjf: Floating Panes

UI_Box*
UI_ListRow(F32 row_h, B32 selected, UI_BoxFlags flags, char* fmt, ...) {
   Temp    scratch = ScratchBegin(0, 0);
   va_list args;
   va_start(args, fmt);
   String8 string = PushStr8FV(scratch.arena, fmt, args);
   va_end(args);
   UI_SetNextChildLayoutAxis(Axis2_X);
   UI_SetNextPrefWidth(UI_Pct(1, 0));
   UI_SetNextPrefHeight(UI_Px(row_h, 1));
   UI_SetNextColorTag(selected ? Str8Lit("selected") : Str8Lit(""));
   UI_Box* box = UI_BuildBoxFromString(flags, string);
   ScratchEnd(scratch);
   return box;
}

UI_Box*
UI_PaneBegin(Rng2F32 rect, String8 string) {
   UI_PushRect(rect);
   UI_SetNextChildLayoutAxis(Axis2_Y);
   UI_Box* box = UI_BuildBoxFromString(UI_BoxFlag_Clickable | UI_BoxFlag_Clip | UI_BoxFlag_DrawBorder | UI_BoxFlag_DrawBackground, string);
   UI_PopRect();
   UI_PushParent(box);
   UI_PushPrefWidth(UI_Pct(1, 0));
   return box;
}

UI_Box*
UI_PaneBeginF(Rng2F32 rect, char* fmt, ...) {
   Temp    scratch = ScratchBegin(0, 0);
   va_list args;
   va_start(args, fmt);
   String8 string = PushStr8FV(scratch.arena, fmt, args);
   va_end(args);
   UI_Box* box = UI_PaneBegin(rect, string);
   ScratchEnd(scratch);
   return box;
}

UI_Signal
UI_PaneEnd(void) {
   UI_PopPrefWidth();
   UI_Box*   box = UI_PopParent();
   UI_Signal sig = UI_SignalFromBox(box);
   return sig;
}

////////////////////////////////
//~ rjf: Tables

thread_static U64  ui_ts_col_pct_count = 0;
thread_static F32* ui_ts_col_pcts_stable = 0;
thread_static U64  ui_ts_vector_idx = 0;
thread_static U64  ui_ts_cell_idx = 0;

void UI_TableBegin(U64 column_pct_count, F32** column_pcts, String8 string) {
   //- rjf: store off persistent, user-provided column info
   ui_ts_col_pct_count = column_pct_count;

   //- rjf: build main table parent
   UI_SetNextPrefHeight(UI_ChildrenSum(1));
   UI_SetNextChildLayoutAxis(Axis2_Y);
   UI_Box* table = UI_BuildBoxFromString(0, string);
   UI_PushParent(table);

   //- rjf: build column boundaries
   F32 x_off = (ui_ts_col_pct_count > 0 ? *column_pcts[0] : 0) * Dim2F32(table->rect).x;
   for (U64 column_idx = 1; column_idx < ui_ts_col_pct_count; column_idx += 1) {
      // rjf: build base rectangle
      Rng2F32 rect = {0};
      {
         rect.x0 = x_off - 3.f;
         rect.y0 = 0;
         rect.x1 = x_off + 3.f;
         rect.y1 = Dim2F32(table->rect).y;
         x_off += *column_pcts[column_idx] * Dim2F32(table->rect).x;
      }

      // rjf: make column boundary widget
      UI_Rect(rect) {
         UI_SetNextHoverCursor(WM_Cursor_LeftRight);
         UI_Box* box = UI_BuildBoxFromStringf(UI_BoxFlag_Clickable, "###%S_boundary_%I64u", table->string, column_idx);

         F32* left_pct_ptr = column_idx < ui_ts_col_pct_count ? column_pcts[column_idx - 1] : 0;
         F32* right_pct_ptr = column_idx < ui_ts_col_pct_count ? column_pcts[column_idx] : 0;

         // rjf: boundary dragging
         UI_Signal interact = UI_SignalFromBox(box);
         if (UI_Dragging(interact)) {
            if (UI_Pressed(interact)) {
               Vec2F32 v = V2F32(*left_pct_ptr, *right_pct_ptr);
               UI_StoreDragStruct(&v);
            }

            // rjf: calculate how much space we're dividing amongst the columns that
            // the user can resize
            F32 adjustable_table_dim = 0;
            if (table->child_layout_axis == Axis2_Y) {
               adjustable_table_dim = Dim2F32(table->rect).x;
            } else {
               U64 child_idx = 0;
               for (UI_Box* v = table->first; !UI_BoxIsNil(v); v = v->next, child_idx += 1) {
                  U64 column_idx = (child_idx + 1);
                  if (column_idx < ui_ts_col_pct_count) {
                     adjustable_table_dim += Dim2F32(v->rect).x;
                  } else {
                     break;
                  }
               }
            }

            // rjf: calculate diff
            F32 min_size = 30.f;
            F32 left_pct__before = UI_GetDragStruct(Vec2F32)->x;
            F32 left_pixels__before = left_pct__before * adjustable_table_dim;
            F32 left_pixels__after = left_pixels__before + UI_DragDelta().x;

            // rjf: clamp left side
            if (left_pixels__after < min_size) {
               left_pixels__after = min_size;
            }

            // rjf: calculate right side
            F32 left_pct__after = left_pixels__after / adjustable_table_dim;
            F32 pct_delta = left_pct__after - left_pct__before;
            F32 right_pct__before = UI_GetDragStruct(Vec2F32)->y;
            F32 right_pct__after = right_pct__before - pct_delta;
            F32 right_pixels__after = right_pct__after * adjustable_table_dim;

            // rjf: clamp right side & back-solve
            if (right_pixels__after < min_size) {
               right_pixels__after = min_size;
               right_pct__after = right_pixels__after / adjustable_table_dim;
               pct_delta = -(right_pct__after - right_pct__before);
               left_pct__after = left_pct__before + pct_delta;
            }

            // rjf: commit new percentages
            *left_pct_ptr = left_pct__after;
            *right_pct_ptr = right_pct__after;
         }
      }
   }

   //- rjf: form stable pcts
   ui_ts_col_pcts_stable = PushArray(UI_BuildArena(), F32, ui_ts_col_pct_count);
   for (U64 idx = 0; idx < column_pct_count; idx += 1) {
      ui_ts_col_pcts_stable[idx] = *column_pcts[idx];
   }

   ui_ts_vector_idx = 0;
}

void UI_TableBeginF(U64 column_pct_count, F32** column_pcts, char* fmt, ...) {
   Temp    scratch = ScratchBegin(0, 0);
   va_list args;
   va_start(args, fmt);
   String8 string = PushStr8FV(scratch.arena, fmt, args);
   va_end(args);
   UI_TableBegin(column_pct_count, column_pcts, string);
   ScratchEnd(scratch);
}

void UI_TableEnd(void) {
   UI_PopParent();
}

UI_Box*
UI_NamedTableVectorBegin(String8 string) {
   UI_SetNextPrefWidth(UI_Pct(1, 0));
   UI_SetNextChildLayoutAxis(Axis2_X);
   UI_Box* vector = UI_BuildBoxFromString(UI_BoxFlag_DrawSideBottom, string);
   ui_ts_vector_idx += 1;
   ui_ts_cell_idx = 0;
   UI_PushParent(vector);
   return vector;
}

UI_Box*
UI_NamedTableVectorBeginF(char* fmt, ...) {
   Temp    scratch = ScratchBegin(0, 0);
   va_list args;
   va_start(args, fmt);
   String8 string = PushStr8FV(scratch.arena, fmt, args);
   va_end(args);
   UI_Box* vector = UI_NamedTableVectorBegin(string);
   ScratchEnd(scratch);
   return vector;
}

UI_Box*
UI_TableVectorBegin(void) {
   UI_Box* table = UI_TopParent();
   UI_Box* vector = UI_NamedTableVectorBeginF("###tbl_vec_%p_%I64u", table, ui_ts_vector_idx);
   return vector;
}

UI_Signal
UI_TableVectorEnd(void) {
   UI_Box* box = UI_PopParent();
   return UI_SignalFromBox(box);
}

UI_Box*
UI_TableCellBegin(void) {
   U64 column_idx = ui_ts_cell_idx;
   F32 width_pct = column_idx < ui_ts_col_pct_count ? ui_ts_col_pcts_stable[column_idx] : 1.f;
   return UI_TableCellSizedBegin(UI_Pct(width_pct, 0));
}

UI_Signal
UI_TableCellEnd(void) {
   UI_Box* cell = UI_PopParent();
   return UI_SignalFromBox(cell);
}

UI_Box*
UI_TableCellSizedBegin(UI_Size size) {
   UI_Box* vector = UI_TopParent();
   U64     column_idx = ui_ts_cell_idx;
   ui_ts_cell_idx += 1;
   UI_SetNextPrefWidth(size);
   UI_SetNextChildLayoutAxis(Axis2_X);
   UI_Box* cell = UI_BuildBoxFromStringf((column_idx > 0 ? UI_BoxFlag_DrawSideLeft : 0), "###tbl_cell_%p_%I64u", vector, ui_ts_cell_idx);
   UI_PushParent(cell);
   return cell;
}

////////////////////////////////
//~ rjf: Scroll Regions

void UI_ScrollListRowBlockChunkListPush(Arena* arena, UI_ScrollListRowBlockChunkList* list, U64 cap, UI_ScrollListRowBlock* block) {
   UI_ScrollListRowBlockChunkNode* n = list->last;
   if (n == 0 || n->count >= n->cap) {
      n = PushArray(arena, UI_ScrollListRowBlockChunkNode, 1);
      n->cap = cap;
      n->v = PushArrayNoZero(arena, UI_ScrollListRowBlock, n->cap);
      SLLQueuePush(list->first, list->last, n);
      list->chunk_count += 1;
   }
   MemoryCopyStruct(&n->v[n->count], block);
   n->count += 1;
   list->total_count += 1;
}

UI_ScrollListRowBlockArray
UI_ScrollListRowBlockArrayFromChunkList(Arena* arena, UI_ScrollListRowBlockChunkList* list) {
   UI_ScrollListRowBlockArray array = {0};
   array.count = list->total_count;
   array.v = PushArrayNoZero(arena, UI_ScrollListRowBlock, array.count);
   U64 idx = 0;
   for (UI_ScrollListRowBlockChunkNode* n = list->first; n != 0; n = n->next) {
      MemoryCopy(array.v + idx, n->v, sizeof(n->v[0]) * n->count);
      idx += n->count;
   }
   return array;
}

U64 UI_ScrollListRowFromItem(UI_ScrollListRowBlockArray* blocks, U64 item) {
   U64 result = 0;
   {
      U64 row_idx = 0;
      U64 item_idx = 0;
      for (U64 block_idx = 0; block_idx < blocks->count; block_idx += 1) {
         UI_ScrollListRowBlock* block = &blocks->v[block_idx];
         U64                    next_row_idx = row_idx + block->row_count;
         U64                    next_item_idx = item_idx + block->item_count;
         if (item_idx <= item && item < next_item_idx) {
            U64 item_off_rows = (item - item_idx) * (block->row_count / block->item_count);
            result = row_idx + item_off_rows;
            break;
         }
         row_idx = next_row_idx;
         item_idx = next_item_idx;
      }
   }
   return result;
}

U64 UI_ScrollListItemFromRow(UI_ScrollListRowBlockArray* blocks, U64 row) {
   U64 result = 0;
   {
      U64 row_idx = 0;
      U64 item_idx = 0;
      for (U64 block_idx = 0; block_idx < blocks->count; block_idx += 1) {
         UI_ScrollListRowBlock* block = &blocks->v[block_idx];
         U64                    next_row_idx = row_idx + block->row_count;
         U64                    next_item_idx = item_idx + block->item_count;
         if (row_idx <= row && row < next_row_idx) {
            result = item_idx;
            break;
         }
         row_idx = next_row_idx;
         item_idx = next_item_idx;
      }
   }
   return result;
}

// bgz: a scroll bar's thumb: a rounded bar against the right edge, thin, and
// the bar's full width under the mouse or while dragged. Up and down bars
// only, the only ones the editor builds. Its colour is the box's background.
PRIVATE void _UI_DrawScrollThumb(UI_Box* box, void* user_data) {
   UnusedVariable(user_data);
   Rng2F32 rect   = box->rect;
   F32     full_w = rect.x1 - rect.x0;
   F32     w      = Max(2.0f, (4.0f + (full_w - 4.0f) * Max(box->hot_t, box->active_t)) - 2.0f);
   F32     x1     = rect.x1 - 1.0f;

   R_Rect2DInst* inst = Core_D_Rect(R2F32P(x1 - w, rect.y0 + 2.0f, x1, rect.y1 - 2.0f), R2F32(V2F32(0, 0), V2F32(1, 1)), box->background_color);

   for EachEnumVal(Corner, corner) {
      inst->corner_radii[corner] = w * 0.5f;
   }
}

UI_ScrollPt
UI_ScrollBar(Axis2 axis, UI_Size off_axis_size, UI_ScrollPt pt, Rng1S64 idx_range, S64 view_num_indices) {
   UI_PushColorTag(Str8Lit("scroll_bar"));
   UI_PushFontSize(UI_BottomFontSize() * 0.65f);

   //- rjf: unpack
   S64 idx_range_dim = Max(Dim1S64(idx_range), 1);

   //- rjf: produce extra flags for cases in which scrolling is disabled
   UI_BoxFlags disabled_flags = 0;
   if (idx_range.min == idx_range.max) {
      disabled_flags |= UI_BoxFlag_Disabled;
   }

   //- rjf: build main container
   UI_SetNextPrefSize(Axis2Flip(axis), off_axis_size);
   UI_SetNextChildLayoutAxis(axis);
   UI_Box* container_box = UI_BuildBoxFromKey(UI_BoxFlag_DrawBorder, UI_KeyZero());

   // bgz: no arrow buttons - the wheel and the track do their work.
   UI_Signal min_scroll_sig = {0};

   //- rjf: main scroller area
   UI_Signal space_before_sig = {0};
   UI_Signal space_after_sig = {0};
   UI_Signal scroller_sig = {0};
   UI_Box*   scroll_area_box = &ui_nil_box;
   UI_Box*   scroller_box = &ui_nil_box;
   UI_Parent(container_box) {
      UI_SetNextPrefSize(axis, UI_Pct(1, 0));
      UI_SetNextChildLayoutAxis(axis);
      scroll_area_box = UI_BuildBoxFromStringf(0, "##_scroll_area_%i", axis);
      UI_Parent(scroll_area_box) {
         // rjf: space before
         if (idx_range.max != idx_range.min) {
            UI_SetNextPrefSize(axis, UI_Pct((F32)((F64)(pt.idx - idx_range.min) / (F64)idx_range_dim), 0));
            UI_Box* space_before_box = UI_BuildBoxFromStringf(UI_BoxFlag_Clickable, "##scroll_area_before");
            space_before_sig = UI_SignalFromBox(space_before_box);
         }

         // rjf: scroller
         // bgz: drawn as a thin rounded bar that widens under the mouse (_UI_DrawScrollThumb).
         UI_Flags(disabled_flags) UI_PrefSize(axis, UI_Pct(Clamp(0.05f, (F32)((F64)Max(view_num_indices, 1) / (F64)idx_range_dim), 1.f), 0.f)) {
            scroller_box = UI_BuildBoxFromStringf(UI_BoxFlag_Clickable, "##_scroller_%i", axis);
            scroller_sig = UI_SignalFromBox(scroller_box);
            UI_BoxEquipCustomDraw(scroller_box, _UI_DrawScrollThumb, 0);
            // bgz (slideshow): the thumb is drawn in its background colour, and
            // a box without DrawBackground is never given one - it was clear.
            scroller_box->background_color = UI_TopBackgroundColor();
         }

         // rjf: space after
         if (idx_range.max != idx_range.min) {
            UI_SetNextPrefSize(axis, UI_Pct(1.f - (F32)((F64)(pt.idx - idx_range.min) / (F64)idx_range_dim), 0));
            UI_Box* space_after_box = UI_BuildBoxFromStringf(UI_BoxFlag_Clickable, "##scroll_area_after");
            space_after_sig = UI_SignalFromBox(space_after_box);
         }
      }
   }

   UI_Signal max_scroll_sig = {0};

   //- rjf: pt * signals -> new pt
   UI_ScrollPt new_pt = pt;
   {
      typedef struct UI_ScrollBarDragData UI_ScrollBarDragData;
      struct UI_ScrollBarDragData {
         UI_ScrollPt start_pt;
         F32         scroll_space_px;
      };
      if (UI_Dragging(scroller_sig)) {
         if (UI_Pressed(scroller_sig)) {
            UI_ScrollBarDragData drag_data = {pt, (FloorF32(Dim2F32(scroll_area_box->rect).v[axis]) - FloorF32(Dim2F32(scroller_box->rect).v[axis]))};
            UI_StoreDragStruct(&drag_data);
         }
         UI_ScrollBarDragData* drag_data = UI_GetDragStruct(UI_ScrollBarDragData);
         UI_ScrollPt           original_pt = drag_data->start_pt;
         F32                   drag_delta = UI_DragDelta().v[axis];
         F32                   drag_pct = drag_delta / drag_data->scroll_space_px;
         S64                   new_idx = original_pt.idx + (S64)(drag_pct * idx_range_dim);
         new_idx = Clamp(idx_range.min, new_idx, idx_range.max);
         UI_ScrollPtTargetIdx(&new_pt, new_idx);
         new_pt.off = 0;
      }
      if (UI_Dragging(min_scroll_sig) || UI_Dragging(space_before_sig)) {
         S64 new_idx = new_pt.idx - 1;
         new_idx = Clamp(idx_range.min, new_idx, idx_range.max);
         UI_ScrollPtTargetIdx(&new_pt, new_idx);
      }
      if (UI_Dragging(max_scroll_sig) || UI_Dragging(space_after_sig)) {
         S64 new_idx = new_pt.idx + 1;
         new_idx = Clamp(idx_range.min, new_idx, idx_range.max);
         UI_ScrollPtTargetIdx(&new_pt, new_idx);
      }
   }

   UI_PopFontSize();
   UI_PopColorTag();
   return new_pt;
}

thread_static UI_ScrollPt* ui_scroll_list_scroll_pt_ptr = 0;
thread_static F32          ui_scroll_list_scroll_bar_dim_px = 0;
thread_static Vec2F32      ui_scroll_list_dim_px = {0};
thread_static Rng1S64      ui_scroll_list_scroll_idx_rng = {0};

void UI_ScrollListBegin(UI_ScrollListParams* params, UI_ScrollPt* scroll_pt, Vec2S64* cursor_out, Vec2S64* mark_out, Rng1S64* visible_row_range_out, UI_ScrollListSignal* signal_out) {
   //- rjf: unpack arguments
   S64     num_possible_visible_rows = (S64)(params->dim_px.y / params->row_height_px);
   Rng1S64 scroll_row_idx_range = R1S64(params->item_range.min, ClampBot(params->item_range.min, params->item_range.max - 1));

   //- rjf: do keyboard navigation
   B32 moved = 0;
   if (params->flags & UI_ScrollListFlag_Nav && cursor_out != 0 && UI_IsFocusActive()) {
      Vec2S64 cursor = *cursor_out;
      Vec2S64 mark = mark_out ? *mark_out : cursor;
      for (UI_Event* evt = 0; UI_NextEvent(&evt);) {
         if ((evt->delta_2s32.x == 0 && evt->delta_2s32.y == 0) ||
             evt->flags & UI_EventFlag_Delete) {
            continue;
         }
         UI_EatEvent(evt);
         moved = 1;
         switch (evt->delta_unit) {
         default: {
            moved = 0;
         } break;
         case UI_EventDeltaUnit_Char: {
            for (Axis2 axis = (Axis2)0; axis < Axis2_COUNT; axis = (Axis2)(axis + 1)) {
               cursor.v[axis] += evt->delta_2s32.v[axis];
               if (cursor.v[axis] < params->cursor_range.min.v[axis]) {
                  cursor.v[axis] = params->cursor_range.max.v[axis];
               }
               if (cursor.v[axis] > params->cursor_range.max.v[axis]) {
                  cursor.v[axis] = params->cursor_range.min.v[axis];
               }
               cursor.v[axis] = Clamp1S64(R1S64(params->cursor_range.min.v[axis], params->cursor_range.max.v[axis]), cursor.v[axis]);
            }
         } break;
         case UI_EventDeltaUnit_Word:
         case UI_EventDeltaUnit_Line:
         case UI_EventDeltaUnit_Page: {
            cursor.x = (evt->delta_2s32.x > 0 ? params->cursor_range.max.x : evt->delta_2s32.x < 0 ? params->cursor_range.min.x + !!params->cursor_min_is_empty_selection[Axis2_X]
                                                                                                   : cursor.x);
            cursor.y += ((evt->delta_2s32.y > 0 ? +(num_possible_visible_rows - 3) : evt->delta_2s32.y < 0 ? -(num_possible_visible_rows - 3)
                                                                                                           : 0));
            cursor.y = Clamp1S64(R1S64(params->cursor_range.min.y + !!params->cursor_min_is_empty_selection[Axis2_Y], params->cursor_range.max.y), cursor.y);
         } break;
         case UI_EventDeltaUnit_Whole: {
            for (Axis2 axis = (Axis2)0; axis < Axis2_COUNT; axis = (Axis2)(axis + 1)) {
               cursor.v[axis] = (evt->delta_2s32.v[axis] > 0 ? params->cursor_range.max.v[axis] : evt->delta_2s32.v[axis] < 0 ? params->cursor_range.min.v[axis] + !!params->cursor_min_is_empty_selection[axis]
                                                                                                                              : cursor.v[axis]);
            }
         } break;
         }
         if (!(evt->flags & UI_EventFlag_KeepMark)) {
            mark = cursor;
         }
      }
      if (moved) {
         *cursor_out = cursor;
         if (mark_out) {
            *mark_out = mark;
         }
      }
   }

   //- rjf: moved -> snap
   if (params->flags & UI_ScrollListFlag_Snap && moved) {
      S64 cursor_item_idx = cursor_out->y - 1;
      if (params->item_range.min <= cursor_item_idx && cursor_item_idx <= params->item_range.max) {
         //- rjf: compute visible row range
         Rng1S64 visible_row_range = R1S64(scroll_pt->idx + 0 - !!(scroll_pt->off < 0),
                                           scroll_pt->idx + 0 + num_possible_visible_rows + 1);

         //- rjf: compute cursor row range from cursor item
         Rng1S64 cursor_visibility_row_range = {0};
         if (params->row_blocks.count == 0) {
            cursor_visibility_row_range = R1S64(cursor_item_idx - 1, cursor_item_idx + 3);
         } else {
            cursor_visibility_row_range.min = (S64)UI_ScrollListRowFromItem(&params->row_blocks, (U64)cursor_item_idx);
            cursor_visibility_row_range.max = cursor_visibility_row_range.min + 4;
         }

         //- rjf: compute deltas & apply
         S64 min_delta = Min(0, cursor_visibility_row_range.min - visible_row_range.min);
         S64 max_delta = Max(0, cursor_visibility_row_range.max - visible_row_range.max);
         S64 new_idx = scroll_pt->idx + min_delta + max_delta;
         new_idx = Clamp1S64(scroll_row_idx_range, new_idx);
         UI_ScrollPtTargetIdx(scroll_pt, new_idx);
      }
   }

   //- rjf: output signal
   if (signal_out != 0) {
      signal_out->cursor_moved = moved;
   }

   //- rjf: determine ranges & limits
   Rng1S64 visible_row_range = R1S64(scroll_pt->idx + (S64)(scroll_pt->off) + 0 - !!(scroll_pt->off < 0),
                                     scroll_pt->idx + (S64)(scroll_pt->off) + 0 + num_possible_visible_rows + 1);
   visible_row_range.min = Clamp1S64(params->item_range, visible_row_range.min);
   visible_row_range.max = Clamp1S64(params->item_range, visible_row_range.max);
   *visible_row_range_out = visible_row_range;

   //- rjf: store thread-locals
   ui_scroll_list_scroll_bar_dim_px = (params->flags & UI_ScrollListFlag_NoScrollBar) ? 0.f : UI_BottomFontSize() * 1.5f; // @Added(NoScrollBar)
   ui_scroll_list_scroll_pt_ptr = scroll_pt;
   ui_scroll_list_dim_px = params->dim_px;
   ui_scroll_list_scroll_idx_rng = scroll_row_idx_range;

   //- rjf: build top-level container
   UI_Box* container_box = &ui_nil_box;
   UI_FixedWidth(params->dim_px.x) UI_FixedHeight(params->dim_px.y) UI_ChildLayoutAxis(Axis2_X) {
      container_box = UI_BuildBoxFromKey(0, UI_KeyZero());
   }

   //- rjf: build scrollable container
   UI_Box* scrollable_container_box = &ui_nil_box;
   UI_Parent(container_box) UI_ChildLayoutAxis(Axis2_Y) UI_FixedWidth(params->dim_px.x - ui_scroll_list_scroll_bar_dim_px) UI_FixedHeight(params->dim_px.y) {
      UI_BoxFlags fade_flags = 0;
      if (scroll_pt->idx > params->item_range.min) {
         fade_flags |= UI_BoxFlag_DrawFadeTop;
      }
      if (scroll_pt->idx + num_possible_visible_rows < params->item_range.max) {
         fade_flags |= UI_BoxFlag_DrawFadeBottom;
      }
      scrollable_container_box = UI_BuildBoxFromStringf(fade_flags | UI_BoxFlag_Clip | UI_BoxFlag_AllowOverflowY | UI_BoxFlag_Scroll, "###sp");
      scrollable_container_box->view_off.y = scrollable_container_box->view_off_target.y = params->row_height_px * ModF32(scroll_pt->off, 1.f) + params->row_height_px * (scroll_pt->off < 0) - params->row_height_px * (scroll_pt->off == -1.f && scroll_pt->idx == 1);
   }

   //- rjf: build vertical scroll bar
   if (!(params->flags & UI_ScrollListFlag_NoScrollBar)) { // @Added(NoScrollBar)
      UI_Parent(container_box) UI_Focus(UI_FocusKind_Null) {
         UI_SetNextFixedWidth(ui_scroll_list_scroll_bar_dim_px);
         UI_SetNextFixedHeight(ui_scroll_list_dim_px.y);
         *ui_scroll_list_scroll_pt_ptr = UI_ScrollBar(Axis2_Y,
                                                      UI_Px(ui_scroll_list_scroll_bar_dim_px, 1.f),
                                                      *ui_scroll_list_scroll_pt_ptr,
                                                      scroll_row_idx_range,
                                                      num_possible_visible_rows);
      }
   }

   //- rjf: begin scrollable region
   UI_PushParent(container_box);
   UI_PushParent(scrollable_container_box);
   UI_PushPrefHeight(UI_Px(params->row_height_px, 1.f));
}

void UI_ScrollListEnd(void) {
   UI_PopPrefHeight();
   UI_Box* scrollable_container_box = UI_PopParent();
   UI_PopParent(); // the container

   //- rjf: scroll
   {
      UI_Signal sig = UI_SignalFromBox(scrollable_container_box);
      if (sig.scroll.y != 0) {
         S64 new_idx = ui_scroll_list_scroll_pt_ptr->idx + sig.scroll.y;
         new_idx = Clamp1S64(ui_scroll_list_scroll_idx_rng, new_idx);
         UI_ScrollPtTargetIdx(ui_scroll_list_scroll_pt_ptr, new_idx);
      }
      UI_ScrollPtClampIdx(ui_scroll_list_scroll_pt_ptr, ui_scroll_list_scroll_idx_rng);
   }
}

////////////////////////////////
//~ bgz: Scroll Rows

UI_ScrollRowsFrame
UI_ScrollRowsBegin(UI_ScrollRows* rows, Vec2F32 size, S64 count, F32 row_h, S64 header_rows, UI_ScrollRowsFlags flags, char* fmt, ...) {
   Temp    scratch = ScratchBegin(0, 0);
   va_list args;
   va_start(args, fmt);
   String8 string = PushStr8FV(scratch.arena, fmt, args);
   va_end(args);

   UI_ScrollRowsFrame frame = {0};
   frame.count              = count;
   frame.header_rows        = header_rows;
   frame.row_h              = row_h;
   frame.size               = size;
   frame.visible            = Max(1, (S64)(size.y / row_h) - header_rows);

   // The bar only when there is somewhere to scroll.
   frame.bar_w = count > frame.visible ? 10.0f : 0.0f;

   UI_SetNextChildLayoutAxis(Axis2_X);
   UI_SetNextPrefWidth(UI_Px(size.x, 1));
   UI_SetNextPrefHeight(UI_Px(size.y, 1));
   frame.view_box = UI_BuildBoxFromString(UI_BoxFlag_Clip | UI_BoxFlag_Scroll | ((flags & UI_ScrollRowsFlag_DrawBackground) ? UI_BoxFlag_DrawBackground : 0), string);

   // Clickable on its own, not as part of the view, so a click on the bar is the bar's.
   UI_Parent(frame.view_box) {
      UI_SetNextChildLayoutAxis(Axis2_Y);
      UI_SetNextPrefWidth(UI_Px(Max(0, size.x - frame.bar_w), 1));
      UI_SetNextPrefHeight(UI_Px(size.y, 1));
      frame.list_box = UI_BuildBoxFromStringf(UI_BoxFlag_Clip | ((flags & UI_ScrollRowsFlag_RowClicks) ? 0 : UI_BoxFlag_MouseClickable), "###list");
   }

   if (!(flags & UI_ScrollRowsFlag_RowClicks)) {
      frame.list_signal = UI_SignalFromBox(frame.list_box);
   }

   UI_Signal wheel_signal = UI_SignalFromBox(frame.view_box);
   S64       max_first    = Max(0, count - frame.visible);

   if (wheel_signal.scroll.y != 0) {
      rows->first_row     = Clamp(0, rows->first_row + wheel_signal.scroll.y, max_first);
      rows->scrolled_away = rows->first_row < max_first;
   }

   // By where the rows are drawn, mid-glide too. The rect is last frame's: what was on screen.
   frame.mouse_row = (S64)FloorF32(rows->shown_row + (UI_Mouse().y - frame.list_box->rect.y0) / row_h) - header_rows;

   ScratchEnd(scratch);
   return frame;
}

void
UI_ScrollRowsEnd(UI_ScrollRows* rows, UI_ScrollRowsFrame* frame, S64 picked, UI_ScrollRowsFlags flags) {
   S64 visible   = frame->visible;
   S64 max_first = Max(0, frame->count - visible);
   F32 row_h     = frame->row_h;
   F32 bar_w     = frame->bar_w;

   // Just far enough to show the pick - so a drag past the top or the bottom scrolls.
   if (rows->show_picked) {
      rows->show_picked   = 0;
      rows->scrolled_away = 0;

      if (picked < rows->first_row) {
         rows->first_row = picked;
      }

      if (picked >= rows->first_row + visible) {
         rows->first_row = picked - visible + 1;
      }
   }
   else if ((flags & UI_ScrollRowsFlag_FollowEnd) && !rows->scrolled_away) {
      rows->first_row = max_first;
   }

   rows->first_row = Clamp(0, rows->first_row, max_first);

   // The bar's thumb takes the pushed background, and the track is a box behind it.
   if (bar_w > 0) {
      UI_Parent(frame->view_box) UI_Focus(UI_FocusKind_Null) {
         UI_SetNextFixedWidth(bar_w);
         UI_SetNextFixedHeight(frame->size.y);
         UI_SetNextBackgroundColor(UI_ThemeColor(Str8Lit("scroll track"), UI_ColorSlot_Background));
         UI_Box* track_box = UI_BuildBoxFromKey(UI_BoxFlag_DrawBackground, UI_KeyZero());

         UI_Parent(track_box) UI_BackgroundColor(UI_ThemeColor(Str8Lit("scroll thumb"), UI_ColorSlot_Background)) {
            UI_SetNextFixedWidth(bar_w);
            UI_SetNextFixedHeight(frame->size.y);
            UI_ScrollPt bar = UI_ScrollBar(Axis2_Y, UI_Px(bar_w, 1), (UI_ScrollPt){rows->first_row, 0}, R1S64(0, max_first), visible);

            if (bar.idx != rows->first_row) {
               rows->first_row     = Clamp(0, bar.idx, max_first);
               rows->scrolled_away = rows->first_row < max_first;
            }
         }
      }
   }

   // Back at the end, new rows are followed again.
   if (rows->first_row >= max_first) {
      rows->scrolled_away = 0;
   }

   F32 target      = (F32)rows->first_row;
   rows->shown_row = Clamp(0.0f, rows->shown_row, (F32)max_first);
   rows->shown_row += (target - rows->shown_row) * (1 - PowF32(2, -30.f * UI_Dt()));

   // Within half a pixel: there.
   if ((flags & UI_ScrollRowsFlag_NoGlide) || AbsF32(target - rows->shown_row) * row_h < 0.5f) {
      rows->shown_row = target;
   }

   frame->first_row = (flags & UI_ScrollRowsFlag_AllRows) ? 0 : (S64)rows->shown_row;
   frame->last_row  = (flags & UI_ScrollRowsFlag_AllRows) ? frame->count : Min(frame->count, frame->first_row + visible + 2);

   // The rows' own box, under any header rows, so only they slide by the part of a row scrolled past.
   UI_Parent(frame->list_box) {
      UI_SetNextChildLayoutAxis(Axis2_Y);
      UI_SetNextPrefWidth(UI_Pct(1, 0));
      UI_SetNextPrefHeight(UI_Px(Max(0, frame->size.y - (F32)frame->header_rows * row_h), 1));
      frame->rows_box                     = UI_BuildBoxFromStringf(UI_BoxFlag_Clip, "###rows");
      frame->rows_box->view_off.y        = (rows->shown_row - (F32)frame->first_row) * row_h;
      frame->rows_box->view_off_target.y = frame->rows_box->view_off.y;
   }
}

////////////////////////////////
//~ bgz: Spans

void UI_SpanListPush(Arena* arena, UI_SpanList* list, F32 x0, F32 x1, Vec4F32 color) {
   UI_SpanNode* node = PushArray(arena, UI_SpanNode, 1);
   node->x0          = x0;
   node->x1          = x1;
   node->color       = color;
   SLLQueuePush(list->first, list->last, node);
}

void UI_DrawSpans(UI_Box* box, void* user_data) {
   UI_SpanList* spans = (UI_SpanList*)user_data;

   for (UI_SpanNode* node = spans->first; node != 0; node = node->next) {
      Rng2F32 rect = R2F32(V2F32(box->rect.x0 + node->x0, box->rect.y0), V2F32(box->rect.x0 + node->x1, box->rect.y1));
      Core_D_Rect(rect, R2F32(V2F32(0, 0), V2F32(1, 1)), node->color);
   }
}

////////////////////////////////
//~ bgz: Line Text

PRIVATE S64 _UI_LineTextClamp(const UI_LineText* text, S64 pos) {
   return Clamp(0, pos, (S64)text->size);
}

// rad's text ops count in TxtPt, a line and a column from 1. They stay inside
// this section: UI_LineText's own positions are byte offsets from 0.
PRIVATE TxtPt _UI_LineTextPtFromPos(S64 pos) {
   return TxtPtMake(1, pos + 1);
}

String8 UI_LineTextString(const UI_LineText* text) {
   return Str8((U8*)text->str, text->size);
}

void UI_LineTextSet(UI_LineText* text, String8 string) {
   text->size = Min(string.size, (U64)sizeof(text->str));

   if (text->size > 0) {
      MemoryCopy(text->str, string.str, text->size);
   }

   text->cursor = (S64)text->size;
   text->mark   = text->cursor;
}

void UI_LineTextSelectAll(UI_LineText* text) {
   text->mark   = 0;
   text->cursor = (S64)text->size;
}

UI_LineTextEdit UI_LineTextEditFromEvent(Arena* arena, WM_Event* event) {
   UI_LineTextEdit edit  = {0};
   B32             ctrl  = (event->modifiers & WM_Modifier_Ctrl) != 0;
   B32             shift = (event->modifiers & WM_Modifier_Shift) != 0;
   B32             alt   = (event->modifiers & WM_Modifier_Alt) != 0;

   if (event->kind == WM_EventKind_Text) {
      if (!ctrl && !alt && 32 <= event->character && event->character <= 126) {
         U8 c        = (U8)event->character;
         edit.kind   = UI_LineTextEditKind_Insert;
         edit.insert = PushStr8Copy(arena, Str8(&c, 1));
      }
   }
   else if (event->kind == WM_EventKind_Press && !alt) {
      switch (event->key) {
      case WM_Key_Left:
      case WM_Key_Right: {
         edit.kind   = UI_LineTextEditKind_Move;
         edit.delta  = event->key == WM_Key_Left ? -1 : 1;
         edit.unit   = ctrl ? UI_EventDeltaUnit_Word : UI_EventDeltaUnit_Char;
         edit.select = shift;
      } break;

      case WM_Key_Home:
      case WM_Key_End: {
         edit.kind   = UI_LineTextEditKind_Move;
         edit.delta  = event->key == WM_Key_Home ? -1 : 1;
         edit.unit   = UI_EventDeltaUnit_Line;
         edit.select = shift;
      } break;

      case WM_Key_Backspace:
      case WM_Key_Delete: {
         edit.kind  = UI_LineTextEditKind_Delete;
         edit.delta = event->key == WM_Key_Backspace ? -1 : 1;
         edit.unit  = ctrl ? UI_EventDeltaUnit_Word : UI_EventDeltaUnit_Char;
      } break;

      case WM_Key_A: {
         if (ctrl) {
            edit.kind = UI_LineTextEditKind_SelectAll;
         }
      } break;

      case WM_Key_V: {
         // The clipboard's first line: this is one line of text.
         if (ctrl) {
            String8 text = Core_WM_GetClipboardText(arena);

            for (U64 i = 0; i < text.size; i += 1) {
               if (text.str[i] == '\n' || text.str[i] == '\r') {
                  text.size = i;
                  break;
               }
            }

            if (text.size > 0) {
               edit.kind   = UI_LineTextEditKind_Insert;
               edit.insert = text;
            }
         }
      } break;

      default: {
      } break;
      }
   }

   return edit;
}

void UI_LineTextApply(UI_LineText* text, UI_LineTextEdit edit) {
   text->cursor = _UI_LineTextClamp(text, text->cursor);
   text->mark   = _UI_LineTextClamp(text, text->mark);

   if (edit.kind == UI_LineTextEditKind_SelectAll) {
      UI_LineTextSelectAll(text);
   }
   else if (edit.kind != UI_LineTextEditKind_Null) {
      // As the event rad's line edit takes, so the text rules are its.
      UI_Event event = {0};

      switch (edit.kind) {
      case UI_LineTextEditKind_Insert: {
         event.kind   = UI_EventKind_Text;
         event.string = edit.insert;
      } break;

      case UI_LineTextEditKind_Delete: {
         event.kind         = UI_EventKind_Edit;
         event.flags        = UI_EventFlag_Delete | UI_EventFlag_ZeroDeltaOnSelect;
         event.delta_unit   = edit.unit;
         event.delta_2s32.x = edit.delta;
      } break;

      case UI_LineTextEditKind_Move: {
         event.kind         = UI_EventKind_Navigate;
         event.delta_unit   = edit.unit;
         event.delta_2s32.x = edit.delta;

         if (edit.select) {
            event.flags = UI_EventFlag_KeepMark;
         }
         else if (edit.unit == UI_EventDeltaUnit_Char) {
            // With text selected, Left and Right go to that end of it.
            event.flags = UI_EventFlag_PickSelectSide | UI_EventFlag_ZeroDeltaOnSelect;
         }
      } break;

      default: {
      } break;
      }

      Temp     scratch = ScratchBegin(0, 0);
      String8  string  = UI_LineTextString(text);
      UI_TxtOp op      = UI_SingleLineTxtOpFromEvent(scratch.arena, &event, string, _UI_LineTextPtFromPos(text->cursor), _UI_LineTextPtFromPos(text->mark));

      // Invalid: the key would leave the text, like Left at its start. rad
      // also marks an insert at the end Invalid (the cursor goes past the old
      // text's end), and its own line edit ignores the flag - so inserts do too.
      if (!(op.flags & UI_TxtOpFlag_Invalid) || edit.kind == UI_LineTextEditKind_Insert) {
         if (!TxtPtMatch(op.range.min, op.range.max) || op.replace.size != 0) {
            String8 changed = UI_PushStringReplaceRange(scratch.arena, string, R1S64(op.range.min.column, op.range.max.column), op.replace);

            text->size = Min(changed.size, (U64)sizeof(text->str));
            MemoryCopy(text->str, changed.str, text->size);
         }

         text->cursor = _UI_LineTextClamp(text, op.cursor.column - 1);
         text->mark   = _UI_LineTextClamp(text, op.mark.column - 1);
      }

      ScratchEnd(scratch);
   }
}

typedef struct UI_LineTextDrawData UI_LineTextDrawData;
struct UI_LineTextDrawData {
   String8 string;
   S64     cursor;
   S64     mark;
};

UI_BOX_CUSTOM_DRAW(UI_LineTextDraw) {
   UI_LineTextDrawData* draw_data    = (UI_LineTextDrawData*)user_data;
   Vec4F32              cursor_color = UI_ColorFromTagsKeySlot(box->tags_key, UI_ColorSlot_Cursor);
   Vec4F32              select_color = UI_ColorFromTagsKeySlot(box->tags_key, UI_ColorSlot_Selection);

   // Drawn over the text, so see-through.
   select_color.w *= 0.5f;

   Vec2F32 text_position = UI_BoxTextPosition(box);
   F32     cursor_x      = text_position.x + Core_FNT_DimFromTagSizeString(box->font, box->font_size, 0, box->tab_size, Str8Prefix(draw_data->string, (U64)draw_data->cursor)).x;
   F32     mark_x        = text_position.x + Core_FNT_DimFromTagSizeString(box->font, box->font_size, 0, box->tab_size, Str8Prefix(draw_data->string, (U64)draw_data->mark)).x;
   F32     thickness     = ClampBot(1.f, FloorF32(box->font_size / 8.f));
   F32     pad           = FloorF32(Dim2F32(box->rect).y * 0.15f);
   F32     y0            = box->rect.y0 + pad;
   F32     y1            = box->rect.y1 - pad;

   if (draw_data->cursor != draw_data->mark) {
      Rng2F32 select_rect = R2F32(V2F32(Min(cursor_x, mark_x), y0), V2F32(Max(cursor_x, mark_x), y1));
      Core_D_Rect(select_rect, R2F32(V2F32(0, 0), V2F32(1, 1)), select_color);
   }

   Rng2F32 cursor_rect = R2F32(V2F32(cursor_x, y0), V2F32(cursor_x + thickness, y1));
   Core_D_Rect(cursor_rect, R2F32(V2F32(0, 0), V2F32(1, 1)), cursor_color);
}

UI_Box* UI_LineTextBox(const UI_LineText* text, char* fmt, ...) {
   Temp    scratch = ScratchBegin(0, 0);
   va_list args;
   va_start(args, fmt);
   String8 string = PushStr8FV(scratch.arena, fmt, args);
   va_end(args);

   String8 shown  = PushStr8Copy(UI_BuildArena(), UI_LineTextString(text));
   F32     text_w = Core_FNT_DimFromTagSizeString(UI_TopFont(), UI_TopFontSize(), 0, UI_TopTabSize(), shown).x;

   // Room past the text for the cursor at its end.
   UI_SetNextPrefWidth(UI_Px(text_w + UI_TopFontSize(), 1.f));
   UI_Box* box = UI_BuildBoxFromString(UI_BoxFlag_DrawText | UI_BoxFlag_DisableTextTrunc, string);
   UI_BoxEquipDisplayString(box, shown);

   UI_LineTextDrawData* draw_data = PushArray(UI_BuildArena(), UI_LineTextDrawData, 1);
   draw_data->string              = shown;
   draw_data->cursor              = _UI_LineTextClamp(text, text->cursor);
   draw_data->mark                = _UI_LineTextClamp(text, text->mark);
   UI_BoxEquipCustomDraw(box, UI_LineTextDraw, draw_data);

   ScratchEnd(scratch);
   return box;
}
