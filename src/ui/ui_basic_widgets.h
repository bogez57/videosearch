// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef UI_BASIC_WIDGETS_H
#define UI_BASIC_WIDGETS_H

////////////////////////////////
//~ rjf: Scroll List Types

typedef U32 UI_ScrollListFlags;
enum {
   UI_ScrollListFlag_Nav = (1 << 0),
   UI_ScrollListFlag_Snap = (1 << 1),
   UI_ScrollListFlag_NoScrollBar = (1 << 2), // @Added(NoScrollBar) contents get the full width; scroll by wheel/keys only
   UI_ScrollListFlag_All = 0xffffffff,
};

typedef struct UI_ScrollListRowBlock UI_ScrollListRowBlock;
struct UI_ScrollListRowBlock {
   U64 row_count;
   U64 item_count;
};

typedef struct UI_ScrollListRowBlockChunkNode UI_ScrollListRowBlockChunkNode;
struct UI_ScrollListRowBlockChunkNode {
   UI_ScrollListRowBlockChunkNode* next;
   UI_ScrollListRowBlock*          v;
   U64                             count;
   U64                             cap;
};

typedef struct UI_ScrollListRowBlockChunkList UI_ScrollListRowBlockChunkList;
struct UI_ScrollListRowBlockChunkList {
   UI_ScrollListRowBlockChunkNode* first;
   UI_ScrollListRowBlockChunkNode* last;
   U64                             chunk_count;
   U64                             total_count;
};

typedef struct UI_ScrollListRowBlockArray UI_ScrollListRowBlockArray;
struct UI_ScrollListRowBlockArray {
   UI_ScrollListRowBlock* v;
   U64                    count;
};

typedef struct UI_ScrollListParams UI_ScrollListParams;
struct UI_ScrollListParams {
   UI_ScrollListFlags         flags;
   Vec2F32                    dim_px;
   F32                        row_height_px;
   UI_ScrollListRowBlockArray row_blocks;
   Rng2S64                    cursor_range;
   Rng1S64                    item_range;
   B32                        cursor_min_is_empty_selection[Axis2_COUNT];
};

typedef struct UI_ScrollListSignal UI_ScrollListSignal;
struct UI_ScrollListSignal {
   B32 cursor_moved;
};

////////////////////////////////
//~ rjf: Basic Widgets

exported void      UI_Divider(UI_Size size);
exported UI_Signal UI_Label(String8 string);
exported UI_Signal UI_LabelF(char* fmt, ...);
exported UI_Signal UI_Button(String8 string);
exported UI_Signal UI_ButtonF(char* fmt, ...);
exported UI_Signal UI_HoverLabel(String8 string);
exported UI_Signal UI_HoverLabelF(char* fmt, ...);
exported UI_Signal UI_LineEdit(TxtPt* cursor, TxtPt* mark, U8* edit_buffer, U64 edit_buffer_size, U64* edit_string_size_out, String8 pre_edit_value, String8 string);
exported UI_Signal UI_LineEditF(TxtPt* cursor, TxtPt* mark, U8* edit_buffer, U64 edit_buffer_size, U64* edit_string_size_out, String8 pre_edit_value, char* fmt, ...);

////////////////////////////////
//~ rjf: Images

exported UI_Signal UI_Image(R_Handle texture, R_Tex2DSampleKind sample_kind, Rng2F32 region, Vec4F32 tint, F32 blur, String8 string);
exported UI_Signal UI_ImageF(R_Handle texture, R_Tex2DSampleKind sample_kind, Rng2F32 region, Vec4F32 tint, F32 blur, char* fmt, ...);

////////////////////////////////
//~ rjf: Special Buttons

exported UI_Signal UI_Expander(B32 is_expanded, String8 string);
exported UI_Signal UI_ExpanderF(B32 is_expanded, char* fmt, ...);
exported UI_Signal UI_SortHeader(B32 sorting, B32 ascending, String8 string);
exported UI_Signal UI_SortHeaderF(B32 sorting, B32 ascending, char* fmt, ...);

////////////////////////////////
//~ rjf: Simple Layout Widgets

exported UI_Box*   UI_RowBegin(void);
exported UI_Signal UI_RowEnd(void);
exported UI_Box*   UI_ColumnBegin(void);
exported UI_Signal UI_ColumnEnd(void);
exported UI_Box*   UI_NamedRowBegin(String8 string);
exported UI_Signal UI_NamedRowEnd(void);
exported UI_Box*   UI_NamedColumnBegin(String8 string);
exported UI_Signal UI_NamedColumnEnd(void);

// bgz: a list's row: full width, row_h tall, children left to right, and the
// selected colours when selected. flags say what it draws and takes.
exported UI_Box*   UI_ListRow(F32 row_h, B32 selected, UI_BoxFlags flags, char* fmt, ...);

////////////////////////////////
//~ rjf: Floating Panes

exported UI_Box*   UI_PaneBegin(Rng2F32 rect, String8 string);
exported UI_Box*   UI_PaneBeginF(Rng2F32 rect, char* fmt, ...);
exported UI_Signal UI_PaneEnd(void);

////////////////////////////////
//~ rjf: Tables

exported void      UI_TableBegin(U64 column_pct_count, F32** column_pcts, String8 string);
exported void      UI_TableBeginF(U64 column_pct_count, F32** column_pcts, char* fmt, ...);
exported void      UI_TableEnd(void);
exported UI_Box*   UI_NamedTableVectorBegin(String8 string);
exported UI_Box*   UI_NamedTableVectorBeginF(char* fmt, ...);
exported UI_Box*   UI_TableVectorBegin(void);
exported UI_Signal UI_TableVectorEnd(void);
exported UI_Box*   UI_TableCellBegin(void);
exported UI_Signal UI_TableCellEnd(void);
exported UI_Box*   UI_TableCellSizedBegin(UI_Size size);

////////////////////////////////
//~ rjf: Scroll Regions

exported void                       UI_ScrollListRowBlockChunkListPush(Arena* arena, UI_ScrollListRowBlockChunkList* list, U64 cap, UI_ScrollListRowBlock* block);
exported UI_ScrollListRowBlockArray UI_ScrollListRowBlockArrayFromChunkList(Arena* arena, UI_ScrollListRowBlockChunkList* list);
exported U64                        UI_ScrollListRowFromItem(UI_ScrollListRowBlockArray* blocks, U64 item);
exported U64                        UI_ScrollListItemFromRow(UI_ScrollListRowBlockArray* blocks, U64 row);
exported UI_ScrollPt                UI_ScrollBar(Axis2 axis, UI_Size off_axis_size, UI_ScrollPt pt, Rng1S64 idx_range, S64 view_num_indices);
exported void                       UI_ScrollListBegin(UI_ScrollListParams* params, UI_ScrollPt* scroll_pt_out, Vec2S64* cursor_out, Vec2S64* mark_out, Rng1S64* visible_row_range_out, UI_ScrollListSignal* signal_out);
exported void                       UI_ScrollListEnd(void);

////////////////////////////////
//~ bgz: Scroll Rows
//
// A list of rows of one height that scrolls by the wheel and a scroll bar,
// gliding to where it is going. The list keeps a UI_ScrollRows in its own
// state and builds its rows itself, between two calls:
//
//    UI_SetNextRect(rect); // and a colour tag: these go to frame.view_box
//    UI_ScrollRowsFrame frame = UI_ScrollRowsBegin(&rows, size, count, row_h, 0, flags, "file tree %llu", id);
//    ... clicks on frame.list_signal, at frame.mouse_row; a header row into frame.list_box ...
//    UI_ScrollRowsEnd(&rows, &frame, picked, flags);
//    UI_Parent(frame.rows_box) for (S64 row = frame.first_row; row < frame.last_row; row += 1) { ... }
//
// The wheel and the bar never move the pick. A key or a drag that moves it
// sets show_picked, and End scrolls just far enough to show it, once.

typedef U32 UI_ScrollRowsFlags;
enum {
   UI_ScrollRowsFlag_FollowEnd = (1 << 0), // End shows the last row, unless the wheel or the bar left the end
   UI_ScrollRowsFlag_RowClicks = (1 << 1), // the rows take their own clicks: list_box isn't clickable
   UI_ScrollRowsFlag_DrawBackground = (1 << 2),
   UI_ScrollRowsFlag_NoGlide        = (1 << 3), // jump straight to where it is going
   UI_ScrollRowsFlag_AllRows        = (1 << 4), // the caller builds every row, not first_row to last_row: rows_box scrolls by all of it
};

typedef struct UI_ScrollRows UI_ScrollRows;
struct UI_ScrollRows {
   S64 first_row;     // the top row, where the list is going
   F32 shown_row;     // the top row as drawn, in rows, gliding to first_row - a list whose rows move moves it too
   B32 show_picked;   // the list's keys set it: End scrolls the pick into sight and clears it
   B32 scrolled_away; // FollowEnd: the wheel or the bar left the end, so new rows don't move the list
};

typedef struct UI_ScrollRowsFrame UI_ScrollRowsFrame;
struct UI_ScrollRowsFrame {
   UI_Box*   view_box;    // the whole list, the wheel's
   UI_Box*   list_box;    // left of the bar: header rows, then rows_box
   UI_Signal list_signal; // clicks and drags anywhere on the rows - empty with RowClicks
   S64       mouse_row;   // the row under the mouse, by last frame's layout; can be outside [0, count)
   S64       visible;     // whole rows that fit, under the header rows

   // Written by End.
   UI_Box* rows_box;  // the rows go in here, first_row first
   S64     first_row;
   S64     last_row;  // one past the last to build: the ones partly in sight while it glides too

   S64     count;
   S64     header_rows;
   F32     row_h;
   F32     bar_w;
   Vec2F32 size;
};

exported UI_ScrollRowsFrame UI_ScrollRowsBegin(UI_ScrollRows* rows, Vec2F32 size, S64 count, F32 row_h, S64 header_rows, UI_ScrollRowsFlags flags, char* fmt, ...);
exported void               UI_ScrollRowsEnd(UI_ScrollRows* rows, UI_ScrollRowsFrame* frame, S64 picked, UI_ScrollRowsFlags flags);

////////////////////////////////
//~ bgz: Spans
//
// Coloured stretches drawn behind a box's text: a selection, find hits, the
// symbol under the cursor, a search's matches. x0 and x1 are from the box's
// left. The caller pushes them on its frame arena and gives the list to the
// box that holds the text:
//
//    UI_SpanList* spans = PushArray(arena, UI_SpanList, 1);
//    UI_SpanListPush(arena, spans, x0, x1, color);
//    UI_BoxEquipCustomDraw(box, UI_DrawSpans, spans);

typedef struct UI_SpanNode UI_SpanNode;
struct UI_SpanNode {
   UI_SpanNode* next;
   F32          x0;
   F32          x1;
   Vec4F32      color;
};

typedef struct UI_SpanList UI_SpanList;
struct UI_SpanList {
   UI_SpanNode* first;
   UI_SpanNode* last;
};

exported void UI_SpanListPush(Arena* arena, UI_SpanList* list, F32 x0, F32 x1, Vec4F32 color);
// A UI_BoxEquipCustomDraw callback; user_data is the UI_SpanList.
exported void UI_DrawSpans(UI_Box* box, void* user_data);

////////////////////////////////
//~ bgz: Line Text
//
// One line of text someone types into: the palette's search, a query's answer.
// The owner keeps a UI_LineText in its own state. Nothing here changes it while
// the UI builds - keys are turned into edits where the owner handles input, and
// applied where the owner changes its state (an op, for App_State):
//
//    UI_LineTextEdit edit = UI_LineTextEditFromEvent(ops->arena, event);
//    if (edit.kind != UI_LineTextEditKind_Null) { push an op carrying edit }
//    ...in that op:  UI_LineTextApply(&state->query.text, params->edit);
//    ...in the UI:   UI_LineTextBox(&state->query.text, "###query text");
//
// An edit is what the key means (insert this, delete a word left), not where:
// Apply works out the where against the text as it is by then, so two keys in
// one frame land in order.

typedef struct UI_LineText UI_LineText;
struct UI_LineText {
   U8  str[256];
   U64 size;
   S64 cursor; // a byte offset from 0, as a document's byte_pos: 0 is before the first byte, size after the last
   S64 mark;   // the selection's other end; equal to cursor when nothing is selected
};

typedef enum UI_LineTextEditKind {
   UI_LineTextEditKind_Null, // the event is not a text key
   UI_LineTextEditKind_Insert,
   UI_LineTextEditKind_Delete,
   UI_LineTextEditKind_Move,
   UI_LineTextEditKind_SelectAll,
} UI_LineTextEditKind;

typedef struct UI_LineTextEdit UI_LineTextEdit;
struct UI_LineTextEdit {
   UI_LineTextEditKind kind;
   String8             insert; // Insert: on the arena given to UI_LineTextEditFromEvent
   S32                 delta;  // Delete, Move: -1 left, 1 right
   UI_EventDeltaUnit   unit;   // Delete, Move: Char, Word, or Line for Home and End
   B32                 select; // Move: Shift is held, so the mark stays
};

exported String8 UI_LineTextString(const UI_LineText* text);
// The cursor ends up after the text. An empty string clears.
exported void UI_LineTextSet(UI_LineText* text, String8 string);
exported void UI_LineTextSelectAll(UI_LineText* text);
// Characters, Backspace and Delete (Ctrl: a word), Left, Right, Home and End
// (Shift selects, Ctrl+Left/Right: a word), Ctrl+A, Ctrl+V. Up, Down, Enter
// and Esc are the owner's.
exported UI_LineTextEdit UI_LineTextEditFromEvent(Arena* arena, WM_Event* event);
exported void            UI_LineTextApply(UI_LineText* text, UI_LineTextEdit edit);
// Draws the text with its cursor and selection, as wide as the text. Changes nothing.
exported UI_Box* UI_LineTextBox(const UI_LineText* text, char* fmt, ...);

////////////////////////////////
//~ rjf: Macro Loop Wrappers

#define UI_Row DeferLoop(UI_RowBegin(), UI_RowEnd())
#define UI_Column DeferLoop(UI_ColumnBegin(), UI_ColumnEnd())
#define UI_NamedRow(s) DeferLoop(UI_NamedRowBegin(s), UI_NamedRowEnd())
#define UI_NamedColumn(s) DeferLoop(UI_NamedColumnBegin(s), UI_NamedColumnEnd())
#define UI_Pane(r, s) DeferLoop(UI_PaneBegin(r, s), UI_PaneEnd())
#define UI_PaneF(r, ...) DeferLoop(UI_PaneBeginF(r, __VA_ARGS__), UI_PaneEnd())
#define UI_Padding(size) DeferLoop(UI_Spacer(size), UI_Spacer(size))
#define UI_Center UI_Padding(UI_Pct(1, 0))

#define UI_Table(col_pct_count, col_pcts, s) DeferLoop(UI_TableBegin(col_pct_count, col_pcts, s), UI_TableEnd())
#define UI_TableF(col_pct_count, col_pcts, ...) DeferLoop(UI_TableBeginF(col_pct_count, col_pcts, __VA_ARGS__), UI_TableEnd())
#define UI_NamedTableVector(s) DeferLoop(UI_NamedTableVectorBegin(s), UI_TableVectorEnd())
#define UI_NamedTableVectorF(...) DeferLoop(UI_NamedTableVectorBeginF(__VA_ARGS__), UI_TableVectorEnd())
#define UI_TableVector DeferLoop(UI_TableVectorBegin(), UI_TableVectorEnd())
#define UI_TableCell DeferLoop(UI_TableCellBegin(), UI_TableCellEnd())
#define UI_TableCellSized(size) DeferLoop(UI_TableCellSizedBegin(size), UI_TableCellEnd())

#define UI_ScrollList(params, scroll_pt_out, cursor_out, mark_out, visible_row_range_out, signal_out) DeferLoop(UI_ScrollListBegin((params), (scroll_pt_out), (cursor_out), (mark_out), (visible_row_range_out), (signal_out)), UI_ScrollListEnd())

#endif // UI_BASIC_WIDGETS_H