# ui

The editor's immediate-mode UI: boxes with sizes and flags are built each frame, laid out, and read back as signals (clicked, dragging, hovering). It comes from raddebugger's UI (`ui_core`, `ui_basic_widgets`), with the editor's own additions marked "bgz:" in the code: themes, `UI_ListRow`, `UI_ScrollRows`, spans and `UI_LineText`.

`ui_inc.h` includes the three headers (`ui_core.h`, `ui_basic_widgets.h`, `ui_colors.h`); `ui_inc.c` includes the two `.c` files. The stack functions (`UI_Push...`, `UI_Pop...`, `UI_SetNext...`) are generated into `generated/ui.meta.h` and `ui.meta.c`.

## State

- `ui_state` (ui_core.c): the selected `UI_State`, thread-local. It holds the box cache, animation cache, theme pattern cache, hot and active box keys, tooltip and context menu state, and the build stacks.
- `ui_nil_box`, `ui_nil_anim_node` (ui_core.h): the nil values the box and animation caches point at.
- `ui_ts_col_pct_count`, `ui_ts_col_pcts_stable`, `ui_ts_vector_idx`, `ui_ts_cell_idx` (ui_basic_widgets.c): the table being built.
- `ui_scroll_list_scroll_pt_ptr`, `ui_scroll_list_scroll_bar_dim_px`, `ui_scroll_list_dim_px`, `ui_scroll_list_scroll_idx_rng` (ui_basic_widgets.c): the scroll list being built.
- The caller owns a `UI_ScrollRows` per scrolling list, and the theme (`UI_Theme`) it passes to `UI_BeginBuild`.

## Functions and ops

- Keys, events and text ops: `UI_KeyFromString`, `UI_KeyMatch`, `UI_NextEvent`, `UI_EatEvent`, `UI_KeyPress`, `UI_SlotPress`, `UI_SingleLineTxtOpFromEvent`.
- State and frame: `UI_StateAlloc`, `UI_SelectState`, `UI_BeginBuild`, `UI_EndBuild`, `UI_LayoutRoot` and the `UI_CalcSizes...` / `UI_Layout...` passes.
- Boxes: `UI_BuildBoxFromString`, `UI_BuildBoxFromStringf`, `UI_BoxEquipDisplayString`, `UI_BoxEquipCustomDraw`, `UI_BoxFromKey`, and `UI_SignalFromBox` (clicks, drags, hover).
- Style stacks: `UI_PushParent`, `UI_PushPrefWidth`, `UI_PushFont`, `UI_PushColorTag` and so on, with the loop macros `UI_Parent`, `UI_PrefWidth`, `UI_Flags`, `UI_ColorTag`, `UI_Focus`. Sizes are made with `UI_Px`, `UI_Em`, `UI_TextDim`, `UI_Pct`, `UI_ChildrenSum`.
- Interaction helpers: tooltips (`UI_Tooltip`), context menus (`UI_CtxMenuOpen`, `UI_CtxMenu`), auto focus (`UI_SetAutoFocusHotKey`), drag data (`UI_StoreDragData`, `UI_GetDragData`), animation (`UI_Anim`).
- bgz: the label font: `UI_SetLabelFont(font, raster_flags)` once a frame after `UI_BeginBuild` (frame.c sets Inter, or the text font when it is missing), read back with `UI_LabelFont` and `UI_LabelRasterFlags`. `UI_LabelFontScope { ... }` draws what it holds in it: words that are not code, such as the status bar, tabs and the file tree.
- Icons: `UI_IconKind`, `UI_IconStringFromKind`. The editor adds `UI_IconKind_WindowMinimize`, `UI_IconKind_WindowMaximize`, `UI_IconKind_WindowRestore`, `UI_IconKind_WindowClose`, `UI_IconKind_FolderClosed`, `UI_IconKind_FolderOpen` and `UI_IconKind_File`.
- Basic widgets: `UI_Label`, `UI_Button`, `UI_HoverLabel`, `UI_LineEdit`, `UI_Image`, `UI_Expander`, `UI_SortHeader`, `UI_Divider`, rows and columns (`UI_Row`, `UI_Column`), floating panes (`UI_Pane`), tables (`UI_Table`, `UI_TableVector`, `UI_TableCell`).
- `UI_ScrollList` (`UI_ScrollListBegin`, `UI_ScrollListEnd`, `UI_ScrollBar`): raddebugger's scroll list. `UI_ScrollListFlag_NoScrollBar` is an editor addition. bgz: `UI_ScrollBar` has no arrow buttons, and its thumb is drawn by `_UI_DrawScrollThumb`: a thin rounded bar that widens to the bar's full width under the mouse or while dragged (up and down bars only).
- bgz: themes. A `UI_Theme` is a table of `UI_ThemePattern` (tags, `UI_ColorSlot`, colour). Boxes ask for their text, background and border under the tags pushed with `UI_ColorTag`; the pattern with the most matching tags wins. `UI_ThemeColor(tags, slot)` asks for a colour directly. frame.c holds the editor's table.
- bgz: `UI_ListRow(row_h, selected, flags, fmt, ...)`: a list row, full width, with the selected colours when selected.
- bgz: `UI_ScrollRowsBegin` and `UI_ScrollRowsEnd`: a list of equal-height rows that scrolls by wheel and scroll bar and glides. `UI_ScrollRowsBegin` returns a `UI_ScrollRowsFrame` (`view_box`, `list_box`, `list_signal`, `mouse_row`, then `rows_box`, `first_row`, `last_row`). The caller builds rows `first_row` to `last_row` inside `rows_box`. `UI_ScrollRowsFlag_FollowEnd`, `UI_ScrollRowsFlag_RowClicks`, `UI_ScrollRowsFlag_DrawBackground`, `UI_ScrollRowsFlag_NoGlide` and `UI_ScrollRowsFlag_AllRows` change how it behaves.
- Rounded corners: `UI_CornerRadius(px)` around a box rounds it (`corner_radii`, one per corner), and `UI_BoxFlag_RoundChildrenByParent` rounds the children at its corners with it. `FR_DrawUITree` passes the radii to the renderer for backgrounds and borders.
- bgz: spans: `UI_SpanList` of `UI_SpanNode` (x0, x1 from a box's left, colour), pushed with `UI_SpanListPush` on the frame arena and drawn behind a box's text by `UI_DrawSpans`, a `UI_BoxEquipCustomDraw` callback. The text view's tints (selection, find hits, brace, symbol under the cursor) and the palette's matched characters use it.
- bgz: `UI_LineText`: one line of text someone types into (256 bytes; `cursor` and `mark` are byte offsets from 0, like a document's `byte_pos`), kept by its owner. Unlike `UI_LineEdit`, nothing changes it while the UI builds. `UI_LineTextEditFromEvent(arena, event)` turns a key into a `UI_LineTextEdit` (Insert, Delete, Move, SelectAll; Null when it is not a text key), which the owner carries in an op; `UI_LineTextApply` applies it inside that op, through rad's `UI_SingleLineTxtOpFromEvent`. `UI_LineTextSet`, `UI_LineTextSelectAll`, `UI_LineTextString`. `UI_LineTextBox` draws it with its cursor and selection.
- bgz: `ui_colors.h`: the editor's palette, the few colours the theme table is built from: backgrounds `UI_BG_0` (the window's edges) to `UI_BG_4` (hover, buttons), `UI_LINE`, text `UI_FG`, `UI_FG_SOFT`, `UI_FG_DIM`, `UI_FG_WEAK`, `UI_ACCENT` and `UI_ACCENT_BG`, and `UI_ERROR`, `UI_WARNING`, `UI_SUCCESS`, `UI_HIGHLIGHT`. Older base colours (`UI_NAVY`, `UI_GREY_85`, `UI_TRANSPARENT` and more) are still there for one-offs. All plain braces for the theme table.

## Settings

None.

## Used by

In the slideshow (this copy, from the editor project's src\editor\ui):

- main.c: `UI_StateAlloc` at startup, stored in `app_state.ui_state`, and the icon font (`data\fonts\Icons.ttf`) in `app_state.ui_icon_font`. Defines `PRIVATE` ahead of the UI headers.
- frame.c: `_FR_UIEventsFromEvents`, `_FR_BeginUI` (the theme table - the editor's palette plus the slideshow's tags: `backdrop`, `panel`, `panel_bar`, `header`, `row`, `row hot`, `dim`, `notice`, `deck`, `folder`, `action`, `syntax`, `picture` - the icons, the regular font at `FR_UIFontSize(15 * FR_UIScale)`, drawn Smooth like the hand-drawn text, and hover/press animation rates), `UI_EndBuild`, and `_FR_DrawUITree`, which draws the tree last into the frame's one draw bucket. FrameWork builds the open screen first (on top, first for clicks), then the edit screen's panels, under `UI_BoxFlag_DisableIDString` so text with `##` in it shows whole.
- frame_open.c: `OPEN_Build`: a clickable backdrop (closes the panel, takes what misses it), the panel, its rows in `UI_ScrollRowsBegin`/`End` (`app_state.open_scroll`) as `UI_ListRow`s, colour by row kind; hover sets the row's background from `row hot`.
- frame_edit.c: `EDIT_Build`: the Format panel's header, its lines as `UI_ListRow`s, the list of pictures in `UI_ScrollRows` (`app_state.pictures_scroll`), and the status line. Not interactive while the open screen is over it.
- ui_basic_widgets.c, changed here only: `UI_ScrollBar` gives the thumb the pushed background colour. The thumb is drawn by `_UI_DrawScrollThumb` in its background colour, which a box without `UI_BoxFlag_DrawBackground` is never given, so it was not seen. The editor's copy has the same code.

In the editor, where this came from:

- query (query.c): its answer is a `UI_LineText`.
- palette (palette.c): `UI_ScrollRowsBegin`, `UI_ListRow`, and spans for a search's matches.
- frame.c: builds the whole window each frame (`UI_BeginBuild` ... `UI_EndBuild`): title bar, panels, dividers, tabs, the overlays (alert, query box, auto complete, context actions, and the box the palette is built in), and the theme table with `UI_ThemeColor`. It does not use the context menu.
- main.c: `UI_StateAlloc` at startup, stored in `app_state.ui_state`.
- file_tree, render_log (plugins) and console, minimap, search (layers): build their views with the widgets; file_tree and console use `UI_ScrollRowsBegin` and `UI_ListRow`, minimap reads colours with `UI_ThemeColor`.
