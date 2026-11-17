// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef UI_H
#define UI_H

////////////////////////////////
//~ rjf: Icon Info

typedef enum UI_IconKind {
   UI_IconKind_Null,
   UI_IconKind_RightArrow,
   UI_IconKind_DownArrow,
   UI_IconKind_LeftArrow,
   UI_IconKind_UpArrow,
   UI_IconKind_RightCaret,
   UI_IconKind_DownCaret,
   UI_IconKind_LeftCaret,
   UI_IconKind_UpCaret,
   UI_IconKind_CheckHollow,
   UI_IconKind_CheckFilled,
   // The editor's own, for its title bar's window buttons.
   UI_IconKind_WindowMinimize,
   UI_IconKind_WindowMaximize,
   UI_IconKind_WindowRestore,
   UI_IconKind_WindowClose,
   // And for the file tree.
   UI_IconKind_FolderClosed,
   UI_IconKind_FolderOpen,
   UI_IconKind_File,
   UI_IconKind_COUNT
} UI_IconKind;

typedef struct UI_IconInfo UI_IconInfo;
struct UI_IconInfo {
   FNT_Tag icon_font;
   String8 icon_kind_text_map[UI_IconKind_COUNT];
};

////////////////////////////////
//~ rjf: Mouse Button Kinds

typedef enum UI_MouseButtonKind {
   UI_MouseButtonKind_Left,
   UI_MouseButtonKind_Middle,
   UI_MouseButtonKind_Right,
   UI_MouseButtonKind_COUNT
} UI_MouseButtonKind;

////////////////////////////////
//~ rjf: Codepath Permissions

typedef U32 UI_PermissionFlags;
enum {
   UI_PermissionFlag_ClicksLeft = (1 << 0),
   UI_PermissionFlag_ClicksMiddle = (1 << 1),
   UI_PermissionFlag_ClicksRight = (1 << 2),
   UI_PermissionFlag_ScrollX = (1 << 3),
   UI_PermissionFlag_ScrollY = (1 << 4),
   UI_PermissionFlag_KeyboardPrimary = (1 << 5),
   UI_PermissionFlag_KeyboardSecondary = (1 << 6),
   UI_PermissionFlag_Text = (1 << 7),

   //- rjf bundles
   UI_PermissionFlag_Keyboard = (UI_PermissionFlag_KeyboardPrimary | UI_PermissionFlag_KeyboardSecondary),
   UI_PermissionFlag_Clicks = (UI_PermissionFlag_ClicksLeft | UI_PermissionFlag_ClicksMiddle | UI_PermissionFlag_ClicksRight),
   UI_PermissionFlag_All = 0xffffffff,
};

////////////////////////////////
//~ rjf: Focus Types

typedef enum UI_FocusKind {
   UI_FocusKind_Null,
   UI_FocusKind_Off,
   UI_FocusKind_On,
   UI_FocusKind_Root,
   UI_FocusKind_COUNT
} UI_FocusKind;

////////////////////////////////
//~ rjf: Events

// TODO(rjf): clean all this up

typedef enum UI_EventKind {
   UI_EventKind_Null,
   UI_EventKind_Press,
   UI_EventKind_Release,
   UI_EventKind_Text,
   UI_EventKind_Navigate,
   UI_EventKind_Edit,
   UI_EventKind_MouseMove,
   UI_EventKind_Scroll,
   UI_EventKind_FileDrop,
   UI_EventKind_COUNT
} UI_EventKind;

typedef enum UI_EventActionSlot {
   UI_EventActionSlot_Null,
   UI_EventActionSlot_Accept,
   UI_EventActionSlot_Cancel,
   UI_EventActionSlot_Edit,
   UI_EventActionSlot_FocusMenu,
   UI_EventActionSlot_COUNT
} UI_EventActionSlot;

typedef U32 UI_EventFlags;
enum {
   UI_EventFlag_KeepMark = (1 << 0),
   UI_EventFlag_Delete = (1 << 1),
   UI_EventFlag_Copy = (1 << 2),
   UI_EventFlag_Paste = (1 << 3),
   UI_EventFlag_ZeroDeltaOnSelect = (1 << 4),
   UI_EventFlag_PickSelectSide = (1 << 5),
   UI_EventFlag_CapAtLine = (1 << 6),
   UI_EventFlag_ExplicitDirectional = (1 << 7),
   UI_EventFlag_Reorder = (1 << 8),
   UI_EventFlag_Secondary = (1 << 9),
};

typedef enum UI_EventDeltaUnit {
   UI_EventDeltaUnit_Null,
   UI_EventDeltaUnit_Char,
   UI_EventDeltaUnit_Word,
   UI_EventDeltaUnit_Line,
   UI_EventDeltaUnit_Page,
   UI_EventDeltaUnit_Whole,
   UI_EventDeltaUnit_COUNT
} UI_EventDeltaUnit;

typedef struct UI_Event UI_Event;
struct UI_Event {
   UI_EventKind       kind;
   UI_EventActionSlot slot;
   UI_EventFlags      flags;
   UI_EventDeltaUnit  delta_unit;
   WM_Key             key;
   WM_Modifiers       modifiers;
   String8            string;
   String8List        paths;
   Vec2F32            pos;
   Vec2F32            delta_2f32;
   Vec2S32            delta_2s32;
   U64                timestamp_us;
};

typedef struct UI_EventNode UI_EventNode;
struct UI_EventNode {
   UI_EventNode* next;
   UI_EventNode* prev;
   UI_Event      v;
};

typedef struct UI_EventList UI_EventList;
struct UI_EventList {
   UI_EventNode* first;
   UI_EventNode* last;
   U64           count;
};

////////////////////////////////
//~ rjf: Textual Operations

typedef U32 UI_TxtOpFlags;
enum {
   UI_TxtOpFlag_Invalid = (1 << 0),
   UI_TxtOpFlag_Copy = (1 << 1),
};

typedef struct UI_TxtOp UI_TxtOp;
struct UI_TxtOp {
   UI_TxtOpFlags flags;
   String8       replace;
   String8       copy;
   TxtRng        range;
   TxtPt         cursor;
   TxtPt         mark;
};

////////////////////////////////
//~ rjf: Keys

typedef struct UI_Key UI_Key;
struct UI_Key {
   U64 u64[1];
};

////////////////////////////////
//~ rjf: Sizes

typedef enum UI_SizeKind {
   UI_SizeKind_Null,
   UI_SizeKind_Pixels,      // size is computed via a preferred pixel value
   UI_SizeKind_TextContent, // size is computed via the dimensions of box's rendered string
   UI_SizeKind_ParentPct,   // size is computed via a well-determined parent or grandparent size
   UI_SizeKind_ChildrenSum, // size is computed via summing well-determined sizes of children
} UI_SizeKind;

typedef struct UI_Size UI_Size;
struct UI_Size {
   UI_SizeKind kind;
   F32         value;
   F32         strictness;
};

////////////////////////////////
//~ bgz: Themes
//
// A theme is a table of patterns (frame.c has the editor's), each one tags, a
// slot and a colour. Tags are any words, pushed around boxes with UI_ColorTag, that
// say where a box is ("panel header focused"). The slot is which of a box's
// colours it is: a box built with no colour pushed asks for its text, background
// and border slots under the tags pushed around it. Code that needs a colour
// itself asks with UI_ThemeColor(tags, slot), whose tags count on top of the
// pushed ones.
//
// Of the patterns for that slot whose tags are all there, the one with the most
// tags wins; on a tie, the earlier one. A pattern with no tags is the slot's
// default. A pushed colour still beats the theme.

typedef enum UI_ColorSlot {
   UI_ColorSlot_Text,
   UI_ColorSlot_Background,
   UI_ColorSlot_Border,
   UI_ColorSlot_Selection,
   UI_ColorSlot_Cursor,
   UI_ColorSlot_COUNT,
} UI_ColorSlot;

typedef struct UI_ThemePattern UI_ThemePattern;
struct UI_ThemePattern {
   char*        tags; // space separated, "" for none
   UI_ColorSlot slot;
   Vec4F32      color;
};

typedef struct UI_Theme UI_Theme;
struct UI_Theme {
   UI_ThemePattern* patterns;
   U64              patterns_count;
};

////////////////////////////////
//~ rjf: Animation Info

typedef struct UI_AnimationInfo UI_AnimationInfo;
struct UI_AnimationInfo {
   F32 hot_animation_rate;
   F32 active_animation_rate;
   F32 focus_animation_rate;
   F32 tooltip_animation_rate;
   F32 menu_animation_rate;
   F32 scroll_animation_rate;
   S64 scroll_rows_per_notch; // bgz: rows one wheel notch scrolls - [general]'s scroll_speed. 0: 4
};

////////////////////////////////
//~ rjf: Scroll Positions

typedef struct UI_ScrollPt UI_ScrollPt;
struct UI_ScrollPt {
   S64 idx;
   F32 off;
};

typedef union UI_ScrollPt2 UI_ScrollPt2;
union UI_ScrollPt2 {
   UI_ScrollPt v[2];
   struct
   {
      UI_ScrollPt x;
      UI_ScrollPt y;
   };
};

////////////////////////////////
//~ rjf: Box Types

typedef enum UI_TextAlign {
   UI_TextAlign_Left,
   UI_TextAlign_Center,
   UI_TextAlign_Right,
   UI_TextAlign_COUNT
} UI_TextAlign;

struct UI_Box;
#define UI_BOX_CUSTOM_DRAW(name) void name(struct UI_Box* box, void* user_data)
typedef UI_BOX_CUSTOM_DRAW(UI_BoxCustomDrawFunctionType);

typedef U64 UI_BoxFlags;
//{
//- rjf: interaction
#define UI_BoxFlag_MouseClickable (UI_BoxFlags)(1ull << 0)
#define UI_BoxFlag_KeyboardClickable (UI_BoxFlags)(1ull << 1)
#define UI_BoxFlag_DropSite (UI_BoxFlags)(1ull << 2)
#define UI_BoxFlag_ClickToFocus (UI_BoxFlags)(1ull << 3)
#define UI_BoxFlag_Scroll (UI_BoxFlags)(1ull << 4)
#define UI_BoxFlag_ViewScrollX (UI_BoxFlags)(1ull << 5)
#define UI_BoxFlag_ViewScrollY (UI_BoxFlags)(1ull << 6)
#define UI_BoxFlag_ViewClampX (UI_BoxFlags)(1ull << 7)
#define UI_BoxFlag_ViewClampY (UI_BoxFlags)(1ull << 8)
#define UI_BoxFlag_FocusHot (UI_BoxFlags)(1ull << 9)
#define UI_BoxFlag_FocusActive (UI_BoxFlags)(1ull << 10)
#define UI_BoxFlag_FocusHotDisabled (UI_BoxFlags)(1ull << 11)
#define UI_BoxFlag_FocusActiveDisabled (UI_BoxFlags)(1ull << 12)
#define UI_BoxFlag_DefaultFocusNavX (UI_BoxFlags)(1ull << 13)
#define UI_BoxFlag_DefaultFocusNavY (UI_BoxFlags)(1ull << 14)
#define UI_BoxFlag_DefaultFocusEdit (UI_BoxFlags)(1ull << 15)
#define UI_BoxFlag_FocusNavSkip (UI_BoxFlags)(1ull << 16)
#define UI_BoxFlag_DisableTruncatedHover (UI_BoxFlags)(1ull << 17)
#define UI_BoxFlag_Disabled (UI_BoxFlags)(1ull << 18)

//- rjf: layout
#define UI_BoxFlag_FloatingX (UI_BoxFlags)(1ull << 19)
#define UI_BoxFlag_FloatingY (UI_BoxFlags)(1ull << 20)
#define UI_BoxFlag_FixedWidth (UI_BoxFlags)(1ull << 21)
#define UI_BoxFlag_FixedHeight (UI_BoxFlags)(1ull << 22)
#define UI_BoxFlag_AllowOverflowX (UI_BoxFlags)(1ull << 23)
#define UI_BoxFlag_AllowOverflowY (UI_BoxFlags)(1ull << 24)
#define UI_BoxFlag_SkipViewOffX (UI_BoxFlags)(1ull << 25)
#define UI_BoxFlag_SkipViewOffY (UI_BoxFlags)(1ull << 26)

//- rjf: appearance / animation
#define UI_BoxFlag_DrawDropShadow (UI_BoxFlags)(1ull << 27)
#define UI_BoxFlag_DrawBackgroundBlur (UI_BoxFlags)(1ull << 28)
#define UI_BoxFlag_DrawBackground (UI_BoxFlags)(1ull << 29)
#define UI_BoxFlag_DrawBorder (UI_BoxFlags)(1ull << 30)
#define UI_BoxFlag_DrawSideTop (UI_BoxFlags)(1ull << 31)
#define UI_BoxFlag_DrawSideBottom (UI_BoxFlags)(1ull << 32)
#define UI_BoxFlag_DrawSideLeft (UI_BoxFlags)(1ull << 33)
#define UI_BoxFlag_DrawSideRight (UI_BoxFlags)(1ull << 34)
#define UI_BoxFlag_DrawText (UI_BoxFlags)(1ull << 35)
#define UI_BoxFlag_DrawTextFastpathCodepoint (UI_BoxFlags)(1ull << 36)
#define UI_BoxFlag_DrawTextWeak (UI_BoxFlags)(1ull << 37)
#define UI_BoxFlag_DrawHotEffects (UI_BoxFlags)(1ull << 38)
#define UI_BoxFlag_DrawActiveEffects (UI_BoxFlags)(1ull << 39)
#define UI_BoxFlag_DrawOverlay (UI_BoxFlags)(1ull << 40)
#define UI_BoxFlag_DrawBucket (UI_BoxFlags)(1ull << 41)
#define UI_BoxFlag_DrawFadeTop (UI_BoxFlags)(1ull << 42)
#define UI_BoxFlag_DrawFadeBottom (UI_BoxFlags)(1ull << 43)
#define UI_BoxFlag_DrawFadeLeft (UI_BoxFlags)(1ull << 44)
#define UI_BoxFlag_DrawFadeRight (UI_BoxFlags)(1ull << 45)
#define UI_BoxFlag_Clip (UI_BoxFlags)(1ull << 46)
#define UI_BoxFlag_AnimatePosX (UI_BoxFlags)(1ull << 47)
#define UI_BoxFlag_AnimatePosY (UI_BoxFlags)(1ull << 48)
#define UI_BoxFlag_DisableTextTrunc (UI_BoxFlags)(1ull << 49)
#define UI_BoxFlag_DisableIDString (UI_BoxFlags)(1ull << 50)
#define UI_BoxFlag_DisableFocusBorder (UI_BoxFlags)(1ull << 51)
#define UI_BoxFlag_DisableFocusOverlay (UI_BoxFlags)(1ull << 52)
#define UI_BoxFlag_HasDisplayString (UI_BoxFlags)(1ull << 53)
#define UI_BoxFlag_HasFuzzyMatchRanges (UI_BoxFlags)(1ull << 54)
#define UI_BoxFlag_RoundChildrenByParent (UI_BoxFlags)(1ull << 55)
#define UI_BoxFlag_SquishAnchored (UI_BoxFlags)(1ull << 56)

//- rjf: debug
#define UI_BoxFlag_Debug (UI_BoxFlags)(1ull << 63)

//- rjf: bundles
#define UI_BoxFlag_Clickable (UI_BoxFlag_MouseClickable | UI_BoxFlag_KeyboardClickable)
#define UI_BoxFlag_DefaultFocusNav (UI_BoxFlag_DefaultFocusNavX | UI_BoxFlag_DefaultFocusNavY | UI_BoxFlag_DefaultFocusEdit)
#define UI_BoxFlag_Floating (UI_BoxFlag_FloatingX | UI_BoxFlag_FloatingY)
#define UI_BoxFlag_FixedSize (UI_BoxFlag_FixedWidth | UI_BoxFlag_FixedHeight)
#define UI_BoxFlag_AllowOverflow (UI_BoxFlag_AllowOverflowX | UI_BoxFlag_AllowOverflowY)
#define UI_BoxFlag_AnimatePos (UI_BoxFlag_AnimatePosX | UI_BoxFlag_AnimatePosY)
#define UI_BoxFlag_ViewScroll (UI_BoxFlag_ViewScrollX | UI_BoxFlag_ViewScrollY)
#define UI_BoxFlag_ViewClamp (UI_BoxFlag_ViewClampX | UI_BoxFlag_ViewClampY)
#define UI_BoxFlag_DisableFocusEffects (UI_BoxFlag_DisableFocusBorder | UI_BoxFlag_DisableFocusOverlay)
//}

typedef struct UI_Box UI_Box;
struct UI_Box {
   //- rjf: persistent links
   UI_Box* hash_next;
   UI_Box* hash_prev;

   //- rjf: per-build links/data
   UI_Box* first;
   UI_Box* last;
   UI_Box* next;
   UI_Box* prev;
   UI_Box* parent;
   U64     child_count;

   //- rjf: per-build equipment
   UI_Key                        key;
   UI_BoxFlags                   flags;
   UI_Key                        tags_key;
   String8                       string;
   UI_TextAlign                  text_align;
   Vec2F32                       fixed_position;
   Vec2F32                       fixed_size;
   Vec2F32                       min_size;
   UI_Size                       pref_size[Axis2_COUNT];
   Axis2                         child_layout_axis;
   WM_Cursor                 hover_cursor;
   U32                           fastpath_codepoint;
   UI_Key                        group_key;
   D_Bucket*                     draw_bucket;
   UI_BoxCustomDrawFunctionType* custom_draw;
   void*                         custom_draw_user_data;
   Vec4F32                       background_color;
   Vec4F32                       text_color;
   Vec4F32                       border_color;
   FNT_Tag                       font;
   F32                           font_size;
   F32                           tab_size;
   FNT_RasterFlags               text_raster_flags;
   F32                           corner_radii[Corner_COUNT];
   F32                           blur_size;
   F32                           transparency;
   F32                           squish;
   F32                           text_padding;

   //- rjf: per-build artifacts
   DR_FStrList         display_fstrs;
   DR_FRunList         display_fruns;
   Rng2F32             rect;
   Vec2F32             fixed_position_animated;
   Vec2F32             position_delta;
   FuzzyMatchRangeList fuzzy_match_ranges;

   //- rjf: persistent data
   U64     first_touched_build_index;
   U64     last_touched_build_index;
   U64     first_disabled_build_index;
   F32     hot_t;
   F32     active_t;
   F32     disabled_t;
   F32     focus_hot_t;
   F32     focus_active_t;
   F32     focus_active_disabled_t;
   Vec2F32 view_off;
   Vec2F32 view_off_target;
   Vec2F32 view_bounds;
   UI_Key  default_nav_focus_hot_key;
   UI_Key  default_nav_focus_active_key;
   UI_Key  default_nav_focus_next_hot_key;
   UI_Key  default_nav_focus_next_active_key;
};

typedef struct UI_BoxRec UI_BoxRec;
struct UI_BoxRec {
   UI_Box* next;
   S32     push_count;
   S32     pop_count;
};

typedef struct UI_BoxNode UI_BoxNode;
struct UI_BoxNode {
   UI_BoxNode* next;
   UI_Box*     box;
};

typedef struct UI_BoxList UI_BoxList;
struct UI_BoxList {
   UI_BoxNode* first;
   UI_BoxNode* last;
   U64         count;
};

typedef U32 UI_SignalFlags;
enum {
   // rjf: mouse press -> box was pressed while hovering
   UI_SignalFlag_LeftPressed = (1 << 0),
   UI_SignalFlag_MiddlePressed = (1 << 1),
   UI_SignalFlag_RightPressed = (1 << 2),

   // rjf: dragging -> box was previously pressed, user is still holding button
   UI_SignalFlag_LeftDragging = (1 << 3),
   UI_SignalFlag_MiddleDragging = (1 << 4),
   UI_SignalFlag_RightDragging = (1 << 5),

   // rjf: double-dragging -> box was previously double-clicked, user is still holding button
   UI_SignalFlag_LeftDoubleDragging = (1 << 6),
   UI_SignalFlag_MiddleDoubleDragging = (1 << 7),
   UI_SignalFlag_RightDoubleDragging = (1 << 8),

   // rjf: triple-dragging -> box was previously triple-clicked, user is still holding button
   UI_SignalFlag_LeftTripleDragging = (1 << 9),
   UI_SignalFlag_MiddleTripleDragging = (1 << 10),
   UI_SignalFlag_RightTripleDragging = (1 << 11),

   // rjf: released -> box was previously pressed & user released, in or out of bounds
   UI_SignalFlag_LeftReleased = (1 << 12),
   UI_SignalFlag_MiddleReleased = (1 << 13),
   UI_SignalFlag_RightReleased = (1 << 14),

   // rjf: clicked -> box was previously pressed & user released, in bounds
   UI_SignalFlag_LeftClicked = (1 << 15),
   UI_SignalFlag_MiddleClicked = (1 << 16),
   UI_SignalFlag_RightClicked = (1 << 17),

   // rjf: double clicked -> box was previously clicked, pressed again
   UI_SignalFlag_LeftDoubleClicked = (1 << 18),
   UI_SignalFlag_MiddleDoubleClicked = (1 << 19),
   UI_SignalFlag_RightDoubleClicked = (1 << 20),

   // rjf: triple clicked -> box was previously clicked twice, pressed again
   UI_SignalFlag_LeftTripleClicked = (1 << 21),
   UI_SignalFlag_MiddleTripleClicked = (1 << 22),
   UI_SignalFlag_RightTripleClicked = (1 << 23),

   // rjf: keyboard pressed -> box had focus, user activated via their keyboard
   UI_SignalFlag_KeyboardPressed = (1 << 24),

   // rjf: passive mouse info
   UI_SignalFlag_Hovering = (1 << 25),  // hovering specifically this box
   UI_SignalFlag_MouseOver = (1 << 26), // mouse is over, but may be occluded

   // rjf: committing state changes via user interaction
   UI_SignalFlag_Commit = (1 << 27),

   // rjf: high-level combos
   UI_SignalFlag_Pressed = UI_SignalFlag_LeftPressed | UI_SignalFlag_KeyboardPressed,
   UI_SignalFlag_Released = UI_SignalFlag_LeftReleased,
   UI_SignalFlag_Clicked = UI_SignalFlag_LeftClicked | UI_SignalFlag_KeyboardPressed,
   UI_SignalFlag_DoubleClicked = UI_SignalFlag_LeftDoubleClicked,
   UI_SignalFlag_TripleClicked = UI_SignalFlag_LeftTripleClicked,
   UI_SignalFlag_Dragging = UI_SignalFlag_LeftDragging,
};

typedef struct UI_Signal UI_Signal;
struct UI_Signal {
   UI_Box*        box;
   WM_Modifiers   event_flags;
   Vec2S16        scroll;
   UI_SignalFlags f;
};

#define UI_Pressed(s) !!((s).f & UI_SignalFlag_Pressed)
#define UI_Clicked(s) !!((s).f & UI_SignalFlag_Clicked)
#define UI_Released(s) !!((s).f & UI_SignalFlag_Released)
#define UI_DoubleClicked(s) !!((s).f & UI_SignalFlag_DoubleClicked)
#define UI_TripleClicked(s) !!((s).f & UI_SignalFlag_TripleClicked)
#define UI_MiddleClicked(s) !!((s).f & UI_SignalFlag_MiddleClicked)
#define UI_RightClicked(s) !!((s).f & UI_SignalFlag_RightClicked)
#define UI_Dragging(s) !!((s).f & UI_SignalFlag_Dragging)
#define UI_Hovering(s) !!((s).f & UI_SignalFlag_Hovering)
#define UI_MouseOver(s) !!((s).f & UI_SignalFlag_MouseOver)
#define UI_Committed(s) !!((s).f & UI_SignalFlag_Commit)

typedef struct UI_Nav UI_Nav;
struct UI_Nav {
   B32     moved;
   Vec2S64 new_p;
};

////////////////////////////////
//~ rjf: Animation State Types

typedef struct UI_AnimParams UI_AnimParams;
struct UI_AnimParams {
   F32 initial;
   F32 target;
   F32 rate;
   F32 epsilon;
   B32 reset;
};

typedef struct UI_AnimNode UI_AnimNode;
struct UI_AnimNode {
   UI_AnimNode*  slot_next;
   UI_AnimNode*  slot_prev;
   UI_AnimNode*  lru_next;
   UI_AnimNode*  lru_prev;
   U64           first_touched_build_index;
   U64           last_touched_build_index;
   UI_Key        key;
   UI_AnimParams params;
   F32           current;
};

typedef struct UI_AnimSlot UI_AnimSlot;
struct UI_AnimSlot {
   UI_AnimNode* first;
   UI_AnimNode* last;
};

////////////////////////////////
//~ rjf: Generated Code

#include "generated/ui.meta.h"

////////////////////////////////
//~ rjf: State Types

//- rjf: cache for mapping 64-bit key -> array of tags

typedef struct UI_TagsCacheNode UI_TagsCacheNode;
struct UI_TagsCacheNode {
   UI_TagsCacheNode* next;
   UI_Key            key;
   String8Array      tags;
};

typedef struct UI_TagsCacheSlot UI_TagsCacheSlot;
struct UI_TagsCacheSlot {
   UI_TagsCacheNode* first;
   UI_TagsCacheNode* last;
};

typedef struct UI_TagsKeyStackNode UI_TagsKeyStackNode;
struct UI_TagsKeyStackNode {
   UI_TagsKeyStackNode* next;
   UI_Key               key;
};

//- rjf: cache for mapping 64-bit key -> theme pattern

typedef struct UI_ThemePatternCacheNode UI_ThemePatternCacheNode;
struct UI_ThemePatternCacheNode {
   UI_ThemePatternCacheNode* slot_next;
   UI_ThemePatternCacheNode* slot_prev;
   UI_ThemePatternCacheNode* lru_next;
   UI_ThemePatternCacheNode* lru_prev;
   U64                       last_build_index_accessed;
   UI_Key                    key;
   Vec4F32                   target_rgba;
   Vec4F32                   current_rgba;
};

typedef struct UI_ThemePatternCacheSlot UI_ThemePatternCacheSlot;
struct UI_ThemePatternCacheSlot {
   UI_ThemePatternCacheNode* first;
   UI_ThemePatternCacheNode* last;
};

//- rjf: cache for mapping 64-bit key -> box

typedef struct UI_BoxHashSlot UI_BoxHashSlot;
struct UI_BoxHashSlot {
   UI_Box* hash_first;
   UI_Box* hash_last;
};

//- rjf: main state bundle

typedef struct UI_State UI_State;
struct UI_State {
   //- rjf: main arena
   Arena* arena;

   //- rjf: fixed keys
   UI_Key external_key;

   //- rjf: build arenas
   Arena* build_arenas[2];
   U64    build_index;

   //- rjf: box cache
   UI_Box*         first_free_box;
   U64             box_table_size;
   UI_BoxHashSlot* box_table;

   //- rjf: anim cache
   UI_AnimNode* free_anim_node;
   UI_AnimNode* lru_anim_node;
   UI_AnimNode* mru_anim_node;
   U64          anim_slots_count;
   UI_AnimSlot* anim_slots;

   //- rjf: build state machine state
   B32                  is_in_open_ctx_menu;
   String8              autocomplete_string;
   B32                  tooltip_can_overflow_window;
   UI_Key               tooltip_anchor_key;
   String8Array         current_gen_tags;
   U64                  current_gen_tags_gen;
   UI_TagsKeyStackNode* tags_key_stack_top;
   UI_TagsKeyStackNode* tags_key_stack_free;
   U64                  tags_cache_slots_count;
   UI_TagsCacheSlot*    tags_cache_slots;

   //- rjf: theme pattern cache
   U64                       theme_pattern_cache_slots_count;
   UI_ThemePatternCacheSlot* theme_pattern_cache_slots;
   UI_ThemePatternCacheNode* theme_pattern_cache_node_free;
   UI_ThemePatternCacheNode* lru_theme_pattern_cache_node;
   UI_ThemePatternCacheNode* mru_theme_pattern_cache_node;

   //- rjf: build phase output
   UI_Box* root;
   UI_Box* tooltip_root;
   UI_Box* ctx_menu_root;
   UI_Key  default_nav_root_key;
   U64     build_box_count;
   U64     last_build_box_count;
   B32     ctx_menu_touched_this_frame;
   B32     is_animating;

   //- rjf: build parameters
   UI_IconInfo      icon_info;
   UI_Theme*        theme;
   FNT_Tag          label_font;         // bgz: UI_SetLabelFont
   FNT_RasterFlags  label_raster_flags;
   UI_AnimationInfo animation_info;
   WM_Window        window;
   UI_EventList*    events;
   Vec2F32          mouse;
   F32              animation_dt;
   F32              default_animation_rate;

   //- rjf: user interaction state
   UI_Key      hot_box_key;
   UI_Key      active_box_key[UI_MouseButtonKind_COUNT];
   UI_Key      drop_hot_box_key;
   UI_Key      clipboard_copy_key;
   U64         press_timestamp_history_us[UI_MouseButtonKind_COUNT][3];
   UI_Key      press_key_history[UI_MouseButtonKind_COUNT][3];
   Vec2F32     press_pos_history[UI_MouseButtonKind_COUNT][3];
   Vec2F32     drag_start_mouse;
   Arena*      drag_state_arena;
   String8     drag_state_data;
   Arena*      string_hover_arena;
   String8     string_hover_string;
   F32         string_hover_size;
   DR_FStrList string_hover_fstrs;
   U64         string_hover_begin_us;
   U64         string_hover_build_index;
   U64         last_time_mousemoved_us;

   //- rjf: tooltip state
   F32 tooltip_open_t;
   B32 tooltip_open;

   //- rjf: context menu state
   UI_Key  ctx_menu_anchor_key;
   UI_Key  next_ctx_menu_anchor_key;
   Vec2F32 ctx_menu_anchor_box_last_pos;
   Vec2F32 ctx_menu_anchor_off;
   B32     ctx_menu_open;
   B32     next_ctx_menu_open;
   F32     ctx_menu_open_t;
   UI_Key  ctx_menu_key;
   B32     ctx_menu_changed;

   //- rjf: build phase stacks
   UI_DeclStackNils;
   UI_DeclStacks;
};

////////////////////////////////
//~ rjf: Basic Type Functions

exported String8 UI_HashPartFromKeyString(String8 string);
exported String8 UI_DisplayPartFromKeyString(String8 string);
exported UI_Key  UI_KeyZero(void);
exported UI_Key  UI_KeyMake(U64 v);
exported UI_Key  UI_KeyFromString(UI_Key seed_key, String8 string);
exported UI_Key  UI_KeyFromStringf(UI_Key seed_key, char* fmt, ...);
exported B32     UI_KeyMatch(UI_Key a, UI_Key b);

////////////////////////////////
//~ rjf: Event Type Functions

exported UI_EventNode* UI_EventListPush(Arena* arena, UI_EventList* list, UI_Event* v);
exported void          UI_EatEventNode(UI_EventList* list, UI_EventNode* node);

////////////////////////////////
//~ rjf: Text Operation Functions

exported B32      UI_CharIsScanBoundary(U8 c);
exported S64      UI_ScannedColumnFromColumn(String8 string, S64 start_column, Side side);
exported UI_TxtOp UI_SingleLineTxtOpFromEvent(Arena* arena, UI_Event* event, String8 string, TxtPt cursor, TxtPt mark);
exported String8  UI_PushStringReplaceRange(Arena* arena, String8 string, Rng1S64 range, String8 replace);

////////////////////////////////
//~ rjf: Size Type Functions

exported UI_Size UI_SizeMake(UI_SizeKind kind, F32 value, F32 strictness);
#define UI_Px(value, strictness) UI_SizeMake(UI_SizeKind_Pixels, value, strictness)
#define UI_Em(value, strictness) UI_SizeMake(UI_SizeKind_Pixels, (value) * UI_TopFontSize(), strictness)
#define UI_TextDim(padding, strictness) UI_SizeMake(UI_SizeKind_TextContent, padding, strictness)
#define UI_Pct(value, strictness) UI_SizeMake(UI_SizeKind_ParentPct, value, strictness)
#define UI_ChildrenSum(strictness) UI_SizeMake(UI_SizeKind_ChildrenSum, 0.f, strictness)

////////////////////////////////
//~ rjf: Scroll Point Type Functions

exported UI_ScrollPt UI_ScrollPtMake(S64 idx, F32 off);
exported void        UI_ScrollPtTargetIdx(UI_ScrollPt* v, S64 idx);
exported void        UI_ScrollPtClampIdx(UI_ScrollPt* v, Rng1S64 range);

////////////////////////////////
//~ rjf: Box Type Functions

read_only global UI_Box ui_nil_box =
    {
        &ui_nil_box,
        &ui_nil_box,
        &ui_nil_box,
        &ui_nil_box,
        &ui_nil_box,
        &ui_nil_box,
        &ui_nil_box,
};
exported B32       UI_BoxIsNil(UI_Box* box);
exported UI_BoxRec UI_BoxRecDf(UI_Box* box, UI_Box* root, U64 sib_member_off, U64 child_member_off);
#define UI_BoxRecDfPre(box, root) UI_BoxRecDf(box, root, OffsetOf(UI_Box, next), OffsetOf(UI_Box, first))
#define UI_BoxRecDfPost(box, root) UI_BoxRecDf(box, root, OffsetOf(UI_Box, prev), OffsetOf(UI_Box, last))
exported void UI_BoxListPush(Arena* arena, UI_BoxList* list, UI_Box* box);

////////////////////////////////
//~ rjf: State Allocating / Selection

exported UI_State* UI_StateAlloc(void);
exported void      UI_StateRelease(UI_State* state);
exported UI_Box*   UI_RootFromState(UI_State* state);
exported B32       UI_AnimatingFromState(UI_State* state);
exported void      UI_SelectState(UI_State* state);
exported UI_State* UI_GetSelectedState(void);

////////////////////////////////
//~ rjf: Implicit State Accessors/Mutators

//- rjf: per-frame info
exported Arena*    UI_BuildArena(void);
exported WM_Window UI_Window(void);
exported Vec2F32   UI_Mouse(void);
exported FNT_Tag   UI_IconFont(void);

// bgz: the font for words that are not code - the title bar, tabs, the file
// tree - and how it is drawn. Set once a frame after UI_BeginBuild; the text
// font when there is no other. UI_LabelFontScope draws what it holds in it.
exported void            UI_SetLabelFont(FNT_Tag font, FNT_RasterFlags raster_flags);
exported FNT_Tag         UI_LabelFont(void);
exported FNT_RasterFlags UI_LabelRasterFlags(void);
#define UI_LabelFontScope DeferLoop((UI_PushFont(UI_LabelFont()), UI_PushTextRasterFlags(UI_LabelRasterFlags())), (UI_PopTextRasterFlags(), UI_PopFont()))
exported String8   UI_IconStringFromKind(UI_IconKind icon_kind);
exported F32       UI_Dt(void);

//- rjf: event pumping
exported B32  UI_NextEvent(UI_Event** ev);
exported void UI_EatEvent(UI_Event* ev);

//- rjf: event consumption helpers
exported B32 UI_KeyPress(WM_Modifiers mods, WM_Key key);
exported B32 UI_KeyRelease(WM_Modifiers mods, WM_Key key);
exported B32 UI_Text(U32 character);
exported B32 UI_SlotPress(UI_EventActionSlot slot);

//- rjf: autocomplete info
exported void    UI_SetAutocompleteString(String8 string);
exported String8 UI_AutocompleteString(void);
exported String8 UI_Autocomplete(void);

//- rjf: drag data
exported Vec2F32 UI_DragStartMouse(void);
exported Vec2F32 UI_DragDelta(void);
exported void    UI_StoreDragData(String8 string);
exported String8 UI_GetDragData(U64 min_required_size);
#define UI_StoreDragStruct(ptr) UI_StoreDragData(Str8Struct(ptr))
#define UI_GetDragStruct(type) ((type*)UI_GetDragData(sizeof(type)).str)

//- rjf: hovered string info
exported B32         UI_StringHoverActive(void);
exported DR_FStrList UI_StringHoverFstrs(Arena* arena);

//- rjf: interaction keys
exported UI_Key UI_HotKey(void);
exported UI_Key UI_ActiveKey(UI_MouseButtonKind button_kind);
exported UI_Key UI_DropHotKey(void);

//- rjf: controls over interaction
exported void UI_KillAction(void);

//- rjf: box cache lookup
exported UI_Box* UI_BoxFromKey(UI_Key key);

////////////////////////////////
//~ rjf: Top-Level Building API

exported void UI_BeginBuild(WM_Window window, UI_EventList* events, UI_IconInfo* icon_info, UI_Theme* theme, UI_AnimationInfo* animation_info, F32 real_dt, F32 animation_dt);
exported void UI_EndBuild(void);
exported void UI_CalcSizesStandaloneInPlace(UI_Box* root, Axis2 axis);
exported void UI_CalcSizesUpwardsDependentInPlace(UI_Box* root, Axis2 axis);
exported void UI_CalcSizesDownwardsDependentInPlace(UI_Box* root, Axis2 axis);
exported void UI_LayoutEnforceConstraintsInPlace(UI_Box* root, Axis2 axis);
exported void UI_LayoutPositionInPlace(UI_Box* root, Axis2 axis);
exported void UI_LayoutRoot(UI_Box* root, Axis2 axis);

////////////////////////////////
//~ rjf: Box Tree Building API

//- rjf: spacers
exported UI_Signal UI_Spacer(UI_Size size);

//- rjf: tooltips
exported void UI_TooltipBeginBase(void);
exported void UI_TooltipEndBase(void);
exported void UI_TooltipBegin(void);
exported void UI_TooltipEnd(void);

//- rjf: context menus
exported void UI_CtxMenuOpen(UI_Key key, UI_Key anchor_box_key, Vec2F32 anchor_off);
exported void UI_CtxMenuClose(void);
exported B32  UI_BeginCtxMenu(UI_Key key);
exported void UI_EndCtxMenu(void);
exported B32  UI_CtxMenuIsOpen(UI_Key key);
exported B32  UI_AnyCtxMenuIsOpen(void);

//- rjf: focus tree coloring
exported B32 UI_IsFocusHot(void);
exported B32 UI_IsFocusActive(void);

//- rjf: implicit auto-managed tree-based focus state
exported B32  UI_IsKeyAutoFocusActive(UI_Key key);
exported B32  UI_IsKeyAutoFocusHot(UI_Key key);
exported void UI_SetAutoFocusActiveKey(UI_Key key);
exported void UI_SetAutoFocusHotKey(UI_Key key);

//- rjf: current style tags key
exported UI_Key UI_TopTagsKey(void);

//- rjf: theme color lookups
exported Vec4F32 UI_ColorFromSlot(UI_ColorSlot slot);
exported Vec4F32 UI_ColorFromTagsKeyExtras(UI_Key key, String8 extras, UI_ColorSlot slot);
exported Vec4F32 UI_ColorFromTagsKeySlot(UI_Key key, UI_ColorSlot slot);
exported Vec4F32 UI_ThemeColor(String8 tags, UI_ColorSlot slot);

//- rjf: box node construction
exported UI_Box* UI_BuildBoxFromKey(UI_BoxFlags flags, UI_Key key);
exported UI_Key  UI_ActiveSeedKey(void);
exported UI_Box* UI_BuildBoxFromString(UI_BoxFlags flags, String8 string);
exported UI_Box* UI_BuildBoxFromStringf(UI_BoxFlags flags, char* fmt, ...);

//- rjf: box node equipment
exported inline void UI_BoxEquipDisplayString(UI_Box* box, String8 string);
exported inline void UI_BoxEquipDisplayFstrs(UI_Box* box, DR_FStrList* strings);
exported inline void UI_BoxEquipDrawBucket(UI_Box* box, D_Bucket* bucket);
exported inline void UI_BoxEquipCustomDraw(UI_Box* box, UI_BoxCustomDrawFunctionType* custom_draw, void* user_data);
exported inline void UI_BoxEquipFuzzyMatchRanges(UI_Box* box, FuzzyMatchRangeList* matches);

//- rjf: box accessors / queries
exported String8 UI_BoxDisplayString(UI_Box* box);
exported Vec2F32 UI_BoxTextPosition(UI_Box* box);
exported U64     UI_BoxCharPosFromXy(UI_Box* box, Vec2F32 xy);

////////////////////////////////
//~ rjf: User Interaction

exported UI_Signal UI_SignalFromBox(UI_Box* box);

////////////////////////////////
//~ rjf: Animation Cache Interaction API

read_only global UI_AnimNode ui_nil_anim_node =
    {
        &ui_nil_anim_node,
        &ui_nil_anim_node,
};

F32 UI_Anim_(UI_Key key, UI_AnimParams* params);
#define UI_Anim(key, target_val, ...) UI_Anim_((key), &(UI_AnimParams){.target = (target_val), .rate = (ui_state->default_animation_rate), __VA_ARGS__})

////////////////////////////////
//~ rjf: Stacks

exported void UI_PushTagsKeyFromAppendedString(String8 string);
exported void UI_PopTagsKey(void);

//- rjf: base
exported UI_Box*            UI_TopParent(void);
exported Axis2              UI_TopChildLayoutAxis(void);
exported F32                UI_TopFixedX(void);
exported F32                UI_TopFixedY(void);
exported F32                UI_TopFixedWidth(void);
exported F32                UI_TopFixedHeight(void);
exported UI_Size            UI_TopPrefWidth(void);
exported UI_Size            UI_TopPrefHeight(void);
exported F32                UI_TopMinWidth(void);
exported F32                UI_TopMinHeight(void);
exported UI_PermissionFlags UI_TopPermissionFlags(void);
exported UI_BoxFlags        UI_TopFlags(void);
exported UI_BoxFlags        UI_TopOmitFlags(void);
exported UI_FocusKind       UI_TopFocusHot(void);
exported UI_FocusKind       UI_TopFocusActive(void);
exported U32                UI_TopFastpathCodepoint(void);
exported UI_Key             UI_TopGroupKey(void);
exported F32                UI_TopTransparency(void);
exported String8            UI_TopColorTag(void);
exported Vec4F32            UI_TopBackgroundColor(void);
exported Vec4F32            UI_TopTextColor(void);
exported Vec4F32            UI_TopBorderColor(void);
exported F32                UI_TopSquish(void);
exported WM_Cursor      UI_TopHoverCursor(void);
exported FNT_Tag            UI_TopFont(void);
exported F32                UI_TopFontSize(void);
exported FNT_RasterFlags    UI_TopTextRasterFlags(void);
exported F32                UI_TopTabSize(void);
exported F32                UI_TopCornerRadius00(void);
exported F32                UI_TopCornerRadius01(void);
exported F32                UI_TopCornerRadius10(void);
exported F32                UI_TopCornerRadius11(void);
exported F32                UI_TopBlurSize(void);
exported F32                UI_TopTextPadding(void);
exported UI_TextAlign       UI_TopTextAlignment(void);
exported UI_Box*            UI_BottomParent(void);
exported Axis2              UI_BottomChildLayoutAxis(void);
exported F32                UI_BottomFixedX(void);
exported F32                UI_BottomFixedY(void);
exported F32                UI_BottomFixedWidth(void);
exported F32                UI_BottomFixedHeight(void);
exported UI_Size            UI_BottomPrefWidth(void);
exported UI_Size            UI_BottomPrefHeight(void);
exported F32                UI_BottomMinWidth(void);
exported F32                UI_BottomMinHeight(void);
exported UI_PermissionFlags UI_BottomPermissionFlags(void);
exported UI_BoxFlags        UI_BottomFlags(void);
exported UI_BoxFlags        UI_BottomOmitFlags(void);
exported UI_FocusKind       UI_BottomFocusHot(void);
exported UI_FocusKind       UI_BottomFocusActive(void);
exported U32                UI_BottomFastpathCodepoint(void);
exported UI_Key             UI_BottomGroupKey(void);
exported F32                UI_BottomTransparency(void);
exported String8            UI_BottomColorTag(void);
exported Vec4F32            UI_BottomBackgroundColor(void);
exported Vec4F32            UI_BottomTextColor(void);
exported Vec4F32            UI_BottomBorderColor(void);
exported F32                UI_BottomSquish(void);
exported WM_Cursor      UI_BottomHoverCursor(void);
exported FNT_Tag            UI_BottomFont(void);
exported F32                UI_BottomFontSize(void);
exported FNT_RasterFlags    UI_BottomTextRasterFlags(void);
exported F32                UI_BottomTabSize(void);
exported F32                UI_BottomCornerRadius00(void);
exported F32                UI_BottomCornerRadius01(void);
exported F32                UI_BottomCornerRadius10(void);
exported F32                UI_BottomCornerRadius11(void);
exported F32                UI_BottomBlurSize(void);
exported F32                UI_BottomTextPadding(void);
exported UI_TextAlign       UI_BottomTextAlignment(void);
exported UI_Box*            UI_PushParent(UI_Box* v);
exported Axis2              UI_PushChildLayoutAxis(Axis2 v);
exported F32                UI_PushFixedX(F32 v);
exported F32                UI_PushFixedY(F32 v);
exported F32                UI_PushFixedWidth(F32 v);
exported F32                UI_PushFixedHeight(F32 v);
exported UI_Size            UI_PushPrefWidth(UI_Size v);
exported UI_Size            UI_PushPrefHeight(UI_Size v);
exported F32                UI_PushMinWidth(F32 v);
exported F32                UI_PushMinHeight(F32 v);
exported UI_PermissionFlags UI_PushPermissionFlags(UI_PermissionFlags v);
exported UI_BoxFlags        UI_PushFlags(UI_BoxFlags v);
exported UI_BoxFlags        UI_PushOmitFlags(UI_BoxFlags v);
exported UI_FocusKind       UI_PushFocusHot(UI_FocusKind v);
exported UI_FocusKind       UI_PushFocusActive(UI_FocusKind v);
exported U32                UI_PushFastpathCodepoint(U32 v);
exported UI_Key             UI_PushGroupKey(UI_Key v);
exported F32                UI_PushTransparency(F32 v);
exported String8            UI_PushColorTag(String8 v);
exported Vec4F32            UI_PushBackgroundColor(Vec4F32 v);
exported Vec4F32            UI_PushTextColor(Vec4F32 v);
exported Vec4F32            UI_PushBorderColor(Vec4F32 v);
exported F32                UI_PushSquish(F32 v);
exported WM_Cursor      UI_PushHoverCursor(WM_Cursor v);
exported FNT_Tag            UI_PushFont(FNT_Tag v);
exported F32                UI_PushFontSize(F32 v);
exported FNT_RasterFlags    UI_PushTextRasterFlags(FNT_RasterFlags v);
exported F32                UI_PushTabSize(F32 v);
exported F32                UI_PushCornerRadius00(F32 v);
exported F32                UI_PushCornerRadius01(F32 v);
exported F32                UI_PushCornerRadius10(F32 v);
exported F32                UI_PushCornerRadius11(F32 v);
exported F32                UI_PushBlurSize(F32 v);
exported F32                UI_PushTextPadding(F32 v);
exported UI_TextAlign       UI_PushTextAlignment(UI_TextAlign v);
exported UI_Box*            UI_PopParent(void);
exported Axis2              UI_PopChildLayoutAxis(void);
exported F32                UI_PopFixedX(void);
exported F32                UI_PopFixedY(void);
exported F32                UI_PopFixedWidth(void);
exported F32                UI_PopFixedHeight(void);
exported UI_Size            UI_PopPrefWidth(void);
exported UI_Size            UI_PopPrefHeight(void);
exported F32                UI_PopMinWidth(void);
exported F32                UI_PopMinHeight(void);
exported UI_PermissionFlags UI_PopPermissionFlags(void);
exported UI_BoxFlags        UI_PopFlags(void);
exported UI_BoxFlags        UI_PopOmitFlags(void);
exported UI_FocusKind       UI_PopFocusHot(void);
exported UI_FocusKind       UI_PopFocusActive(void);
exported U32                UI_PopFastpathCodepoint(void);
exported UI_Key             UI_PopGroupKey(void);
exported F32                UI_PopTransparency(void);
exported String8            UI_PopColorTag(void);
exported Vec4F32            UI_PopBackgroundColor(void);
exported Vec4F32            UI_PopTextColor(void);
exported Vec4F32            UI_PopBorderColor(void);
exported F32                UI_PopSquish(void);
exported WM_Cursor      UI_PopHoverCursor(void);
exported FNT_Tag            UI_PopFont(void);
exported F32                UI_PopFontSize(void);
exported FNT_RasterFlags    UI_PopTextRasterFlags(void);
exported F32                UI_PopTabSize(void);
exported F32                UI_PopCornerRadius00(void);
exported F32                UI_PopCornerRadius01(void);
exported F32                UI_PopCornerRadius10(void);
exported F32                UI_PopCornerRadius11(void);
exported F32                UI_PopBlurSize(void);
exported F32                UI_PopTextPadding(void);
exported UI_TextAlign       UI_PopTextAlignment(void);
exported UI_Box*            UI_SetNextParent(UI_Box* v);
exported Axis2              UI_SetNextChildLayoutAxis(Axis2 v);
exported F32                UI_SetNextFixedX(F32 v);
exported F32                UI_SetNextFixedY(F32 v);
exported F32                UI_SetNextFixedWidth(F32 v);
exported F32                UI_SetNextFixedHeight(F32 v);
exported UI_Size            UI_SetNextPrefWidth(UI_Size v);
exported UI_Size            UI_SetNextPrefHeight(UI_Size v);
exported F32                UI_SetNextMinWidth(F32 v);
exported F32                UI_SetNextMinHeight(F32 v);
exported UI_PermissionFlags UI_SetNextPermissionFlags(UI_PermissionFlags v);
exported UI_BoxFlags        UI_SetNextFlags(UI_BoxFlags v);
exported UI_BoxFlags        UI_SetNextOmitFlags(UI_BoxFlags v);
exported UI_FocusKind       UI_SetNextFocusHot(UI_FocusKind v);
exported UI_FocusKind       UI_SetNextFocusActive(UI_FocusKind v);
exported U32                UI_SetNextFastpathCodepoint(U32 v);
exported UI_Key             UI_SetNextGroupKey(UI_Key v);
exported F32                UI_SetNextTransparency(F32 v);
exported String8            UI_SetNextColorTag(String8 v);
exported Vec4F32            UI_SetNextBackgroundColor(Vec4F32 v);
exported Vec4F32            UI_SetNextTextColor(Vec4F32 v);
exported Vec4F32            UI_SetNextBorderColor(Vec4F32 v);
exported F32                UI_SetNextSquish(F32 v);
exported WM_Cursor      UI_SetNextHoverCursor(WM_Cursor v);
exported FNT_Tag            UI_SetNextFont(FNT_Tag v);
exported F32                UI_SetNextFontSize(F32 v);
exported FNT_RasterFlags    UI_SetNextTextRasterFlags(FNT_RasterFlags v);
exported F32                UI_SetNextTabSize(F32 v);
exported F32                UI_SetNextCornerRadius00(F32 v);
exported F32                UI_SetNextCornerRadius01(F32 v);
exported F32                UI_SetNextCornerRadius10(F32 v);
exported F32                UI_SetNextCornerRadius11(F32 v);
exported F32                UI_SetNextBlurSize(F32 v);
exported F32                UI_SetNextTextPadding(F32 v);
exported UI_TextAlign       UI_SetNextTextAlignment(UI_TextAlign v);

//- rjf: helpers
exported Rng2F32 UI_PushRect(Rng2F32 rect);
exported Rng2F32 UI_PopRect(void);
exported void    UI_SetNextRect(Rng2F32 rect);
exported UI_Size UI_PushPrefSize(Axis2 axis, UI_Size v);
exported UI_Size UI_PopPrefSize(Axis2 axis);
exported UI_Size UI_SetNextPrefSize(Axis2 axis, UI_Size v);
exported void    UI_PushCornerRadius(F32 v);
exported void    UI_PopCornerRadius(void);
exported void    UI_PushColorTagf(char* fmt, ...);
exported F32     UI_TopPxHeight(void);

////////////////////////////////
//~ rjf: Macro Loop Wrappers

//- rjf: stacks (base)
#define UI_Parent(v) DeferLoop(UI_PushParent(v), UI_PopParent())
#define UI_ChildLayoutAxis(v) DeferLoop(UI_PushChildLayoutAxis(v), UI_PopChildLayoutAxis())
#define UI_FixedX(v) DeferLoop(UI_PushFixedX(v), UI_PopFixedX())
#define UI_FixedY(v) DeferLoop(UI_PushFixedY(v), UI_PopFixedY())
#define UI_FixedWidth(v) DeferLoop(UI_PushFixedWidth(v), UI_PopFixedWidth())
#define UI_FixedHeight(v) DeferLoop(UI_PushFixedHeight(v), UI_PopFixedHeight())
#define UI_PrefWidth(v) DeferLoop(UI_PushPrefWidth(v), UI_PopPrefWidth())
#define UI_PrefHeight(v) DeferLoop(UI_PushPrefHeight(v), UI_PopPrefHeight())
#define UI_MinWidth(v) DeferLoop(UI_PushMinWidth(v), UI_PopMinWidth())
#define UI_MinHeight(v) DeferLoop(UI_PushMinHeight(v), UI_PopMinHeight())
#define UI_PermissionFlags(v) DeferLoop(UI_PushPermissionFlags(v), UI_PopPermissionFlags())
#define UI_Flags(v) DeferLoop(UI_PushFlags(v), UI_PopFlags())
#define UI_OmitFlags(v) DeferLoop(UI_PushOmitFlags(v), UI_PopOmitFlags())
#define UI_FocusHot(v) DeferLoop(UI_PushFocusHot(v), UI_PopFocusHot())
#define UI_FocusActive(v) DeferLoop(UI_PushFocusActive(v), UI_PopFocusActive())
#define UI_FastpathCodepoint(v) DeferLoop(UI_PushFastpathCodepoint(v), UI_PopFastpathCodepoint())
#define UI_GroupKey(v) DeferLoop(UI_PushGroupKey(v), UI_PopGroupKey())
#define UI_Transparency(v) DeferLoop(UI_PushTransparency(v), UI_PopTransparency())
#define UI_ColorTag(v) DeferLoop(UI_PushColorTag(v), UI_PopColorTag())
#define UI_BackgroundColor(v) DeferLoop(UI_PushBackgroundColor(v), UI_PopBackgroundColor())
#define UI_TextColor(v) DeferLoop(UI_PushTextColor(v), UI_PopTextColor())
#define UI_Squish(v) DeferLoop(UI_PushSquish(v), UI_PopSquish())
#define UI_HoverCursor(v) DeferLoop(UI_PushHoverCursor(v), UI_PopHoverCursor())
#define UI_Font(v) DeferLoop(UI_PushFont(v), UI_PopFont())
#define UI_FontSize(v) DeferLoop(UI_PushFontSize(v), UI_PopFontSize())
#define UI_TextRasterFlags(v) DeferLoop(UI_PushTextRasterFlags(v), UI_PopTextRasterFlags())
#define UI_TabSize(v) DeferLoop(UI_PushTabSize(v), UI_PopTabSize())
#define UI_CornerRadius00(v) DeferLoop(UI_PushCornerRadius00(v), UI_PopCornerRadius00())
#define UI_CornerRadius01(v) DeferLoop(UI_PushCornerRadius01(v), UI_PopCornerRadius01())
#define UI_CornerRadius10(v) DeferLoop(UI_PushCornerRadius10(v), UI_PopCornerRadius10())
#define UI_CornerRadius11(v) DeferLoop(UI_PushCornerRadius11(v), UI_PopCornerRadius11())
#define UI_BlurSize(v) DeferLoop(UI_PushBlurSize(v), UI_PopBlurSize())
#define UI_TextPadding(v) DeferLoop(UI_PushTextPadding(v), UI_PopTextPadding())
#define UI_TextAlignment(v) DeferLoop(UI_PushTextAlignment(v), UI_PopTextAlignment())

//- rjf: stacks (compositions)
#define UI_FixedPos(v) DeferLoop((UI_PushFixedX((v).x), UI_PushFixedY((v).y)), (UI_PopFixedX(), UI_PopFixedY()))
#define UI_FixedSize(v) DeferLoop((UI_PushFixedWidth((v).x), UI_PushFixedHeight((v).y)), (UI_PopFixedWidth(), UI_PopFixedHeight()))
#define UI_WidthFill UI_PrefWidth(UI_Pct(1.f, 0.f))
#define UI_HeightFill UI_PrefHeight(UI_Pct(1.f, 0.f))
#define UI_Rect(r) DeferLoop(UI_PushRect(r), UI_PopRect())
#define UI_PrefSize(axis, v) DeferLoop(UI_PushPrefSize((axis), (v)), UI_PopPrefSize(axis))
#define UI_CornerRadius(v) DeferLoop(UI_PushCornerRadius(v), UI_PopCornerRadius())
#define UI_Focus(kind) DeferLoop((UI_PushFocusHot(kind), UI_PushFocusActive(kind)), (UI_PopFocusHot(), UI_PopFocusActive()))
#define UI_FlagsAdd(v) DeferLoop(UI_PushFlags(UI_TopFlags() | (v)), UI_PopFlags())
#define UI_ColorTagF(...) DeferLoop(UI_PushColorTagf(__VA_ARGS__), UI_PopColorTag())

//- rjf: tooltip
#define UI_TooltipBase DeferLoop(UI_TooltipBeginBase(), UI_TooltipEndBase())
#define UI_Tooltip DeferLoop(UI_TooltipBegin(), UI_TooltipEnd())

//- rjf: context menu
#define UI_CtxMenu(key) DeferLoopChecked(UI_BeginCtxMenu(key), UI_EndCtxMenu())

//- rjf: debug
#define UI_Debug UI_FlagsAdd(UI_BoxFlag_Debug)

#endif // UI_H
