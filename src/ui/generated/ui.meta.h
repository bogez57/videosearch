// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

//- GENERATED CODE

#ifndef UI_META_H
#define UI_META_H

typedef struct UI_ParentNode UI_ParentNode;
struct UI_ParentNode {
   UI_ParentNode* next;
   UI_Box*        v;
};
typedef struct UI_ChildLayoutAxisNode UI_ChildLayoutAxisNode;
struct UI_ChildLayoutAxisNode {
   UI_ChildLayoutAxisNode* next;
   Axis2                   v;
};
typedef struct UI_FixedXNode UI_FixedXNode;
struct UI_FixedXNode {
   UI_FixedXNode* next;
   F32            v;
};
typedef struct UI_FixedYNode UI_FixedYNode;
struct UI_FixedYNode {
   UI_FixedYNode* next;
   F32            v;
};
typedef struct UI_FixedWidthNode UI_FixedWidthNode;
struct UI_FixedWidthNode {
   UI_FixedWidthNode* next;
   F32                v;
};
typedef struct UI_FixedHeightNode UI_FixedHeightNode;
struct UI_FixedHeightNode {
   UI_FixedHeightNode* next;
   F32                 v;
};
typedef struct UI_PrefWidthNode UI_PrefWidthNode;
struct UI_PrefWidthNode {
   UI_PrefWidthNode* next;
   UI_Size           v;
};
typedef struct UI_PrefHeightNode UI_PrefHeightNode;
struct UI_PrefHeightNode {
   UI_PrefHeightNode* next;
   UI_Size            v;
};
typedef struct UI_MinWidthNode UI_MinWidthNode;
struct UI_MinWidthNode {
   UI_MinWidthNode* next;
   F32              v;
};
typedef struct UI_MinHeightNode UI_MinHeightNode;
struct UI_MinHeightNode {
   UI_MinHeightNode* next;
   F32               v;
};
typedef struct UI_PermissionFlagsNode UI_PermissionFlagsNode;
struct UI_PermissionFlagsNode {
   UI_PermissionFlagsNode* next;
   UI_PermissionFlags      v;
};
typedef struct UI_FlagsNode UI_FlagsNode;
struct UI_FlagsNode {
   UI_FlagsNode* next;
   UI_BoxFlags   v;
};
typedef struct UI_OmitFlagsNode UI_OmitFlagsNode;
struct UI_OmitFlagsNode {
   UI_OmitFlagsNode* next;
   UI_BoxFlags       v;
};
typedef struct UI_FocusHotNode UI_FocusHotNode;
struct UI_FocusHotNode {
   UI_FocusHotNode* next;
   UI_FocusKind     v;
};
typedef struct UI_FocusActiveNode UI_FocusActiveNode;
struct UI_FocusActiveNode {
   UI_FocusActiveNode* next;
   UI_FocusKind        v;
};
typedef struct UI_FastpathCodepointNode UI_FastpathCodepointNode;
struct UI_FastpathCodepointNode {
   UI_FastpathCodepointNode* next;
   U32                       v;
};
typedef struct UI_GroupKeyNode UI_GroupKeyNode;
struct UI_GroupKeyNode {
   UI_GroupKeyNode* next;
   UI_Key           v;
};
typedef struct UI_TransparencyNode UI_TransparencyNode;
struct UI_TransparencyNode {
   UI_TransparencyNode* next;
   F32                  v;
};
typedef struct UI_TagNode UI_TagNode;
struct UI_TagNode {
   UI_TagNode* next;
   String8     v;
};
typedef struct UI_BackgroundColorNode UI_BackgroundColorNode;
struct UI_BackgroundColorNode {
   UI_BackgroundColorNode* next;
   Vec4F32                 v;
};
typedef struct UI_TextColorNode UI_TextColorNode;
struct UI_TextColorNode {
   UI_TextColorNode* next;
   Vec4F32           v;
};
typedef struct UI_BorderColorNode UI_BorderColorNode;
struct UI_BorderColorNode {
   UI_BorderColorNode* next;
   Vec4F32             v;
};
typedef struct UI_SquishNode UI_SquishNode;
struct UI_SquishNode {
   UI_SquishNode* next;
   F32            v;
};
typedef struct UI_HoverCursorNode UI_HoverCursorNode;
struct UI_HoverCursorNode {
   UI_HoverCursorNode* next;
   WM_Cursor       v;
};
typedef struct UI_FontNode UI_FontNode;
struct UI_FontNode {
   UI_FontNode* next;
   FNT_Tag      v;
};
typedef struct UI_FontSizeNode UI_FontSizeNode;
struct UI_FontSizeNode {
   UI_FontSizeNode* next;
   F32              v;
};
typedef struct UI_TextRasterFlagsNode UI_TextRasterFlagsNode;
struct UI_TextRasterFlagsNode {
   UI_TextRasterFlagsNode* next;
   FNT_RasterFlags         v;
};
typedef struct UI_TabSizeNode UI_TabSizeNode;
struct UI_TabSizeNode {
   UI_TabSizeNode* next;
   F32             v;
};
typedef struct UI_CornerRadius00Node UI_CornerRadius00Node;
struct UI_CornerRadius00Node {
   UI_CornerRadius00Node* next;
   F32                    v;
};
typedef struct UI_CornerRadius01Node UI_CornerRadius01Node;
struct UI_CornerRadius01Node {
   UI_CornerRadius01Node* next;
   F32                    v;
};
typedef struct UI_CornerRadius10Node UI_CornerRadius10Node;
struct UI_CornerRadius10Node {
   UI_CornerRadius10Node* next;
   F32                    v;
};
typedef struct UI_CornerRadius11Node UI_CornerRadius11Node;
struct UI_CornerRadius11Node {
   UI_CornerRadius11Node* next;
   F32                    v;
};
typedef struct UI_BlurSizeNode UI_BlurSizeNode;
struct UI_BlurSizeNode {
   UI_BlurSizeNode* next;
   F32              v;
};
typedef struct UI_TextPaddingNode UI_TextPaddingNode;
struct UI_TextPaddingNode {
   UI_TextPaddingNode* next;
   F32                 v;
};
typedef struct UI_TextAlignmentNode UI_TextAlignmentNode;
struct UI_TextAlignmentNode {
   UI_TextAlignmentNode* next;
   UI_TextAlign          v;
};
#define UI_DeclStackNils                                         \
   struct                                                        \
   {                                                             \
      UI_ParentNode            parent_nil_stack_top;             \
      UI_ChildLayoutAxisNode   child_layout_axis_nil_stack_top;  \
      UI_FixedXNode            fixed_x_nil_stack_top;            \
      UI_FixedYNode            fixed_y_nil_stack_top;            \
      UI_FixedWidthNode        fixed_width_nil_stack_top;        \
      UI_FixedHeightNode       fixed_height_nil_stack_top;       \
      UI_PrefWidthNode         pref_width_nil_stack_top;         \
      UI_PrefHeightNode        pref_height_nil_stack_top;        \
      UI_MinWidthNode          min_width_nil_stack_top;          \
      UI_MinHeightNode         min_height_nil_stack_top;         \
      UI_PermissionFlagsNode   permission_flags_nil_stack_top;   \
      UI_FlagsNode             flags_nil_stack_top;              \
      UI_OmitFlagsNode         omit_flags_nil_stack_top;         \
      UI_FocusHotNode          focus_hot_nil_stack_top;          \
      UI_FocusActiveNode       focus_active_nil_stack_top;       \
      UI_FastpathCodepointNode fastpath_codepoint_nil_stack_top; \
      UI_GroupKeyNode          group_key_nil_stack_top;          \
      UI_TransparencyNode      transparency_nil_stack_top;       \
      UI_TagNode               tag_nil_stack_top;                \
      UI_BackgroundColorNode   background_color_nil_stack_top;   \
      UI_TextColorNode         text_color_nil_stack_top;         \
      UI_BorderColorNode       border_color_nil_stack_top;       \
      UI_SquishNode            squish_nil_stack_top;             \
      UI_HoverCursorNode       hover_cursor_nil_stack_top;       \
      UI_FontNode              font_nil_stack_top;               \
      UI_FontSizeNode          font_size_nil_stack_top;          \
      UI_TextRasterFlagsNode   text_raster_flags_nil_stack_top;  \
      UI_TabSizeNode           tab_size_nil_stack_top;           \
      UI_CornerRadius00Node    corner_radius_00_nil_stack_top;   \
      UI_CornerRadius01Node    corner_radius_01_nil_stack_top;   \
      UI_CornerRadius10Node    corner_radius_10_nil_stack_top;   \
      UI_CornerRadius11Node    corner_radius_11_nil_stack_top;   \
      UI_BlurSizeNode          blur_size_nil_stack_top;          \
      UI_TextPaddingNode       text_padding_nil_stack_top;       \
      UI_TextAlignmentNode     text_alignment_nil_stack_top;     \
   }
#define UI_InitStackNils(state)                                      \
   state->parent_nil_stack_top.v = &ui_nil_box;                      \
   state->child_layout_axis_nil_stack_top.v = Axis2_X;               \
   state->fixed_x_nil_stack_top.v = 0;                               \
   state->fixed_y_nil_stack_top.v = 0;                               \
   state->fixed_width_nil_stack_top.v = 0;                           \
   state->fixed_height_nil_stack_top.v = 0;                          \
   state->pref_width_nil_stack_top.v = UI_Px(250.f, 1.f);            \
   state->pref_height_nil_stack_top.v = UI_Px(30.f, 1.f);            \
   state->min_width_nil_stack_top.v = 0;                             \
   state->min_height_nil_stack_top.v = 0;                            \
   state->permission_flags_nil_stack_top.v = UI_PermissionFlag_All;  \
   state->flags_nil_stack_top.v = 0;                                 \
   state->omit_flags_nil_stack_top.v = 0;                            \
   state->focus_hot_nil_stack_top.v = UI_FocusKind_Null;             \
   state->focus_active_nil_stack_top.v = UI_FocusKind_Null;          \
   state->fastpath_codepoint_nil_stack_top.v = 0;                    \
   state->group_key_nil_stack_top.v = UI_KeyZero();                  \
   state->transparency_nil_stack_top.v = 0;                          \
   state->tag_nil_stack_top.v = Str8Lit("");                         \
   state->background_color_nil_stack_top.v = V4F32(0, 0, 0, 0);      \
   state->text_color_nil_stack_top.v = V4F32(0, 0, 0, 0);            \
   state->border_color_nil_stack_top.v = V4F32(0, 0, 0, 0);          \
   state->squish_nil_stack_top.v = 0;                                \
   state->hover_cursor_nil_stack_top.v = WM_Cursor_Pointer;      \
   state->font_nil_stack_top.v = Core_FNT_TagZero();                      \
   state->font_size_nil_stack_top.v = 24.f;                          \
   state->text_raster_flags_nil_stack_top.v = FNT_RasterFlag_Hinted; \
   state->tab_size_nil_stack_top.v = 24.f * 4.f;                     \
   state->corner_radius_00_nil_stack_top.v = 0;                      \
   state->corner_radius_01_nil_stack_top.v = 0;                      \
   state->corner_radius_10_nil_stack_top.v = 0;                      \
   state->corner_radius_11_nil_stack_top.v = 0;                      \
   state->blur_size_nil_stack_top.v = 0;                             \
   state->text_padding_nil_stack_top.v = 0;                          \
   state->text_alignment_nil_stack_top.v = UI_TextAlign_Left;

#define UI_DeclStacks                          \
   struct                                      \
   {                                           \
      struct {                                 \
         UI_ParentNode* top;                   \
         UI_Box*        bottom_val;            \
         UI_ParentNode* free;                  \
         U64            gen;                   \
         B32            auto_pop;              \
      } parent_stack;                          \
      struct {                                 \
         UI_ChildLayoutAxisNode* top;          \
         Axis2                   bottom_val;   \
         UI_ChildLayoutAxisNode* free;         \
         U64                     gen;          \
         B32                     auto_pop;     \
      } child_layout_axis_stack;               \
      struct {                                 \
         UI_FixedXNode* top;                   \
         F32            bottom_val;            \
         UI_FixedXNode* free;                  \
         U64            gen;                   \
         B32            auto_pop;              \
      } fixed_x_stack;                         \
      struct {                                 \
         UI_FixedYNode* top;                   \
         F32            bottom_val;            \
         UI_FixedYNode* free;                  \
         U64            gen;                   \
         B32            auto_pop;              \
      } fixed_y_stack;                         \
      struct {                                 \
         UI_FixedWidthNode* top;               \
         F32                bottom_val;        \
         UI_FixedWidthNode* free;              \
         U64                gen;               \
         B32                auto_pop;          \
      } fixed_width_stack;                     \
      struct {                                 \
         UI_FixedHeightNode* top;              \
         F32                 bottom_val;       \
         UI_FixedHeightNode* free;             \
         U64                 gen;              \
         B32                 auto_pop;         \
      } fixed_height_stack;                    \
      struct {                                 \
         UI_PrefWidthNode* top;                \
         UI_Size           bottom_val;         \
         UI_PrefWidthNode* free;               \
         U64               gen;                \
         B32               auto_pop;           \
      } pref_width_stack;                      \
      struct {                                 \
         UI_PrefHeightNode* top;               \
         UI_Size            bottom_val;        \
         UI_PrefHeightNode* free;              \
         U64                gen;               \
         B32                auto_pop;          \
      } pref_height_stack;                     \
      struct {                                 \
         UI_MinWidthNode* top;                 \
         F32              bottom_val;          \
         UI_MinWidthNode* free;                \
         U64              gen;                 \
         B32              auto_pop;            \
      } min_width_stack;                       \
      struct {                                 \
         UI_MinHeightNode* top;                \
         F32               bottom_val;         \
         UI_MinHeightNode* free;               \
         U64               gen;                \
         B32               auto_pop;           \
      } min_height_stack;                      \
      struct {                                 \
         UI_PermissionFlagsNode* top;          \
         UI_PermissionFlags      bottom_val;   \
         UI_PermissionFlagsNode* free;         \
         U64                     gen;          \
         B32                     auto_pop;     \
      } permission_flags_stack;                \
      struct {                                 \
         UI_FlagsNode* top;                    \
         UI_BoxFlags   bottom_val;             \
         UI_FlagsNode* free;                   \
         U64           gen;                    \
         B32           auto_pop;               \
      } flags_stack;                           \
      struct {                                 \
         UI_OmitFlagsNode* top;                \
         UI_BoxFlags       bottom_val;         \
         UI_OmitFlagsNode* free;               \
         U64               gen;                \
         B32               auto_pop;           \
      } omit_flags_stack;                      \
      struct {                                 \
         UI_FocusHotNode* top;                 \
         UI_FocusKind     bottom_val;          \
         UI_FocusHotNode* free;                \
         U64              gen;                 \
         B32              auto_pop;            \
      } focus_hot_stack;                       \
      struct {                                 \
         UI_FocusActiveNode* top;              \
         UI_FocusKind        bottom_val;       \
         UI_FocusActiveNode* free;             \
         U64                 gen;              \
         B32                 auto_pop;         \
      } focus_active_stack;                    \
      struct {                                 \
         UI_FastpathCodepointNode* top;        \
         U32                       bottom_val; \
         UI_FastpathCodepointNode* free;       \
         U64                       gen;        \
         B32                       auto_pop;   \
      } fastpath_codepoint_stack;              \
      struct {                                 \
         UI_GroupKeyNode* top;                 \
         UI_Key           bottom_val;          \
         UI_GroupKeyNode* free;                \
         U64              gen;                 \
         B32              auto_pop;            \
      } group_key_stack;                       \
      struct {                                 \
         UI_TransparencyNode* top;             \
         F32                  bottom_val;      \
         UI_TransparencyNode* free;            \
         U64                  gen;             \
         B32                  auto_pop;        \
      } transparency_stack;                    \
      struct {                                 \
         UI_TagNode* top;                      \
         String8     bottom_val;               \
         UI_TagNode* free;                     \
         U64         gen;                      \
         B32         auto_pop;                 \
      } tag_stack;                             \
      struct {                                 \
         UI_BackgroundColorNode* top;          \
         Vec4F32                 bottom_val;   \
         UI_BackgroundColorNode* free;         \
         U64                     gen;          \
         B32                     auto_pop;     \
      } background_color_stack;                \
      struct {                                 \
         UI_TextColorNode* top;                \
         Vec4F32           bottom_val;         \
         UI_TextColorNode* free;               \
         U64               gen;                \
         B32               auto_pop;           \
      } text_color_stack;                      \
      struct {                                 \
         UI_BorderColorNode* top;              \
         Vec4F32             bottom_val;       \
         UI_BorderColorNode* free;             \
         U64                 gen;              \
         B32                 auto_pop;         \
      } border_color_stack;                    \
      struct {                                 \
         UI_SquishNode* top;                   \
         F32            bottom_val;            \
         UI_SquishNode* free;                  \
         U64            gen;                   \
         B32            auto_pop;              \
      } squish_stack;                          \
      struct {                                 \
         UI_HoverCursorNode* top;              \
         WM_Cursor       bottom_val;       \
         UI_HoverCursorNode* free;             \
         U64                 gen;              \
         B32                 auto_pop;         \
      } hover_cursor_stack;                    \
      struct {                                 \
         UI_FontNode* top;                     \
         FNT_Tag      bottom_val;              \
         UI_FontNode* free;                    \
         U64          gen;                     \
         B32          auto_pop;                \
      } font_stack;                            \
      struct {                                 \
         UI_FontSizeNode* top;                 \
         F32              bottom_val;          \
         UI_FontSizeNode* free;                \
         U64              gen;                 \
         B32              auto_pop;            \
      } font_size_stack;                       \
      struct {                                 \
         UI_TextRasterFlagsNode* top;          \
         FNT_RasterFlags         bottom_val;   \
         UI_TextRasterFlagsNode* free;         \
         U64                     gen;          \
         B32                     auto_pop;     \
      } text_raster_flags_stack;               \
      struct {                                 \
         UI_TabSizeNode* top;                  \
         F32             bottom_val;           \
         UI_TabSizeNode* free;                 \
         U64             gen;                  \
         B32             auto_pop;             \
      } tab_size_stack;                        \
      struct {                                 \
         UI_CornerRadius00Node* top;           \
         F32                    bottom_val;    \
         UI_CornerRadius00Node* free;          \
         U64                    gen;           \
         B32                    auto_pop;      \
      } corner_radius_00_stack;                \
      struct {                                 \
         UI_CornerRadius01Node* top;           \
         F32                    bottom_val;    \
         UI_CornerRadius01Node* free;          \
         U64                    gen;           \
         B32                    auto_pop;      \
      } corner_radius_01_stack;                \
      struct {                                 \
         UI_CornerRadius10Node* top;           \
         F32                    bottom_val;    \
         UI_CornerRadius10Node* free;          \
         U64                    gen;           \
         B32                    auto_pop;      \
      } corner_radius_10_stack;                \
      struct {                                 \
         UI_CornerRadius11Node* top;           \
         F32                    bottom_val;    \
         UI_CornerRadius11Node* free;          \
         U64                    gen;           \
         B32                    auto_pop;      \
      } corner_radius_11_stack;                \
      struct {                                 \
         UI_BlurSizeNode* top;                 \
         F32              bottom_val;          \
         UI_BlurSizeNode* free;                \
         U64              gen;                 \
         B32              auto_pop;            \
      } blur_size_stack;                       \
      struct {                                 \
         UI_TextPaddingNode* top;              \
         F32                 bottom_val;       \
         UI_TextPaddingNode* free;             \
         U64                 gen;              \
         B32                 auto_pop;         \
      } text_padding_stack;                    \
      struct {                                 \
         UI_TextAlignmentNode* top;            \
         UI_TextAlign          bottom_val;     \
         UI_TextAlignmentNode* free;           \
         U64                   gen;            \
         B32                   auto_pop;       \
      } text_alignment_stack;                  \
   }
#define UI_InitStacks(state)                                                       \
   state->parent_stack.top = &state->parent_nil_stack_top;                         \
   state->parent_stack.bottom_val = &ui_nil_box;                                   \
   state->parent_stack.free = 0;                                                   \
   state->parent_stack.auto_pop = 0;                                               \
   state->child_layout_axis_stack.top = &state->child_layout_axis_nil_stack_top;   \
   state->child_layout_axis_stack.bottom_val = Axis2_X;                            \
   state->child_layout_axis_stack.free = 0;                                        \
   state->child_layout_axis_stack.auto_pop = 0;                                    \
   state->fixed_x_stack.top = &state->fixed_x_nil_stack_top;                       \
   state->fixed_x_stack.bottom_val = 0;                                            \
   state->fixed_x_stack.free = 0;                                                  \
   state->fixed_x_stack.auto_pop = 0;                                              \
   state->fixed_y_stack.top = &state->fixed_y_nil_stack_top;                       \
   state->fixed_y_stack.bottom_val = 0;                                            \
   state->fixed_y_stack.free = 0;                                                  \
   state->fixed_y_stack.auto_pop = 0;                                              \
   state->fixed_width_stack.top = &state->fixed_width_nil_stack_top;               \
   state->fixed_width_stack.bottom_val = 0;                                        \
   state->fixed_width_stack.free = 0;                                              \
   state->fixed_width_stack.auto_pop = 0;                                          \
   state->fixed_height_stack.top = &state->fixed_height_nil_stack_top;             \
   state->fixed_height_stack.bottom_val = 0;                                       \
   state->fixed_height_stack.free = 0;                                             \
   state->fixed_height_stack.auto_pop = 0;                                         \
   state->pref_width_stack.top = &state->pref_width_nil_stack_top;                 \
   state->pref_width_stack.bottom_val = UI_Px(250.f, 1.f);                         \
   state->pref_width_stack.free = 0;                                               \
   state->pref_width_stack.auto_pop = 0;                                           \
   state->pref_height_stack.top = &state->pref_height_nil_stack_top;               \
   state->pref_height_stack.bottom_val = UI_Px(30.f, 1.f);                         \
   state->pref_height_stack.free = 0;                                              \
   state->pref_height_stack.auto_pop = 0;                                          \
   state->min_width_stack.top = &state->min_width_nil_stack_top;                   \
   state->min_width_stack.bottom_val = 0;                                          \
   state->min_width_stack.free = 0;                                                \
   state->min_width_stack.auto_pop = 0;                                            \
   state->min_height_stack.top = &state->min_height_nil_stack_top;                 \
   state->min_height_stack.bottom_val = 0;                                         \
   state->min_height_stack.free = 0;                                               \
   state->min_height_stack.auto_pop = 0;                                           \
   state->permission_flags_stack.top = &state->permission_flags_nil_stack_top;     \
   state->permission_flags_stack.bottom_val = UI_PermissionFlag_All;               \
   state->permission_flags_stack.free = 0;                                         \
   state->permission_flags_stack.auto_pop = 0;                                     \
   state->flags_stack.top = &state->flags_nil_stack_top;                           \
   state->flags_stack.bottom_val = 0;                                              \
   state->flags_stack.free = 0;                                                    \
   state->flags_stack.auto_pop = 0;                                                \
   state->omit_flags_stack.top = &state->omit_flags_nil_stack_top;                 \
   state->omit_flags_stack.bottom_val = 0;                                         \
   state->omit_flags_stack.free = 0;                                               \
   state->omit_flags_stack.auto_pop = 0;                                           \
   state->focus_hot_stack.top = &state->focus_hot_nil_stack_top;                   \
   state->focus_hot_stack.bottom_val = UI_FocusKind_Null;                          \
   state->focus_hot_stack.free = 0;                                                \
   state->focus_hot_stack.auto_pop = 0;                                            \
   state->focus_active_stack.top = &state->focus_active_nil_stack_top;             \
   state->focus_active_stack.bottom_val = UI_FocusKind_Null;                       \
   state->focus_active_stack.free = 0;                                             \
   state->focus_active_stack.auto_pop = 0;                                         \
   state->fastpath_codepoint_stack.top = &state->fastpath_codepoint_nil_stack_top; \
   state->fastpath_codepoint_stack.bottom_val = 0;                                 \
   state->fastpath_codepoint_stack.free = 0;                                       \
   state->fastpath_codepoint_stack.auto_pop = 0;                                   \
   state->group_key_stack.top = &state->group_key_nil_stack_top;                   \
   state->group_key_stack.bottom_val = UI_KeyZero();                               \
   state->group_key_stack.free = 0;                                                \
   state->group_key_stack.auto_pop = 0;                                            \
   state->transparency_stack.top = &state->transparency_nil_stack_top;             \
   state->transparency_stack.bottom_val = 0;                                       \
   state->transparency_stack.free = 0;                                             \
   state->transparency_stack.auto_pop = 0;                                         \
   state->tag_stack.top = &state->tag_nil_stack_top;                               \
   state->tag_stack.bottom_val = Str8Lit("");                                      \
   state->tag_stack.free = 0;                                                      \
   state->tag_stack.auto_pop = 0;                                                  \
   state->background_color_stack.top = &state->background_color_nil_stack_top;     \
   state->background_color_stack.bottom_val = V4F32(0, 0, 0, 0);                   \
   state->background_color_stack.free = 0;                                         \
   state->background_color_stack.auto_pop = 0;                                     \
   state->text_color_stack.top = &state->text_color_nil_stack_top;                 \
   state->text_color_stack.bottom_val = V4F32(0, 0, 0, 0);                         \
   state->text_color_stack.free = 0;                                               \
   state->text_color_stack.auto_pop = 0;                                           \
   state->border_color_stack.top = &state->border_color_nil_stack_top;             \
   state->border_color_stack.bottom_val = V4F32(0, 0, 0, 0);                       \
   state->border_color_stack.free = 0;                                             \
   state->border_color_stack.auto_pop = 0;                                         \
   state->squish_stack.top = &state->squish_nil_stack_top;                         \
   state->squish_stack.bottom_val = 0;                                             \
   state->squish_stack.free = 0;                                                   \
   state->squish_stack.auto_pop = 0;                                               \
   state->hover_cursor_stack.top = &state->hover_cursor_nil_stack_top;             \
   state->hover_cursor_stack.bottom_val = WM_Cursor_Pointer;                   \
   state->hover_cursor_stack.free = 0;                                             \
   state->hover_cursor_stack.auto_pop = 0;                                         \
   state->font_stack.top = &state->font_nil_stack_top;                             \
   state->font_stack.bottom_val = Core_FNT_TagZero();                                   \
   state->font_stack.free = 0;                                                     \
   state->font_stack.auto_pop = 0;                                                 \
   state->font_size_stack.top = &state->font_size_nil_stack_top;                   \
   state->font_size_stack.bottom_val = 24.f;                                       \
   state->font_size_stack.free = 0;                                                \
   state->font_size_stack.auto_pop = 0;                                            \
   state->text_raster_flags_stack.top = &state->text_raster_flags_nil_stack_top;   \
   state->text_raster_flags_stack.bottom_val = FNT_RasterFlag_Hinted;              \
   state->text_raster_flags_stack.free = 0;                                        \
   state->text_raster_flags_stack.auto_pop = 0;                                    \
   state->tab_size_stack.top = &state->tab_size_nil_stack_top;                     \
   state->tab_size_stack.bottom_val = 24.f * 4.f;                                  \
   state->tab_size_stack.free = 0;                                                 \
   state->tab_size_stack.auto_pop = 0;                                             \
   state->corner_radius_00_stack.top = &state->corner_radius_00_nil_stack_top;     \
   state->corner_radius_00_stack.bottom_val = 0;                                   \
   state->corner_radius_00_stack.free = 0;                                         \
   state->corner_radius_00_stack.auto_pop = 0;                                     \
   state->corner_radius_01_stack.top = &state->corner_radius_01_nil_stack_top;     \
   state->corner_radius_01_stack.bottom_val = 0;                                   \
   state->corner_radius_01_stack.free = 0;                                         \
   state->corner_radius_01_stack.auto_pop = 0;                                     \
   state->corner_radius_10_stack.top = &state->corner_radius_10_nil_stack_top;     \
   state->corner_radius_10_stack.bottom_val = 0;                                   \
   state->corner_radius_10_stack.free = 0;                                         \
   state->corner_radius_10_stack.auto_pop = 0;                                     \
   state->corner_radius_11_stack.top = &state->corner_radius_11_nil_stack_top;     \
   state->corner_radius_11_stack.bottom_val = 0;                                   \
   state->corner_radius_11_stack.free = 0;                                         \
   state->corner_radius_11_stack.auto_pop = 0;                                     \
   state->blur_size_stack.top = &state->blur_size_nil_stack_top;                   \
   state->blur_size_stack.bottom_val = 0;                                          \
   state->blur_size_stack.free = 0;                                                \
   state->blur_size_stack.auto_pop = 0;                                            \
   state->text_padding_stack.top = &state->text_padding_nil_stack_top;             \
   state->text_padding_stack.bottom_val = 0;                                       \
   state->text_padding_stack.free = 0;                                             \
   state->text_padding_stack.auto_pop = 0;                                         \
   state->text_alignment_stack.top = &state->text_alignment_nil_stack_top;         \
   state->text_alignment_stack.bottom_val = UI_TextAlign_Left;                     \
   state->text_alignment_stack.free = 0;                                           \
   state->text_alignment_stack.auto_pop = 0;

#define UI_AutoPopStacks(state)                     \
   if (state->parent_stack.auto_pop) {              \
      UI_PopParent();                               \
      state->parent_stack.auto_pop = 0;             \
   }                                                \
   if (state->child_layout_axis_stack.auto_pop) {   \
      UI_PopChildLayoutAxis();                      \
      state->child_layout_axis_stack.auto_pop = 0;  \
   }                                                \
   if (state->fixed_x_stack.auto_pop) {             \
      UI_PopFixedX();                               \
      state->fixed_x_stack.auto_pop = 0;            \
   }                                                \
   if (state->fixed_y_stack.auto_pop) {             \
      UI_PopFixedY();                               \
      state->fixed_y_stack.auto_pop = 0;            \
   }                                                \
   if (state->fixed_width_stack.auto_pop) {         \
      UI_PopFixedWidth();                           \
      state->fixed_width_stack.auto_pop = 0;        \
   }                                                \
   if (state->fixed_height_stack.auto_pop) {        \
      UI_PopFixedHeight();                          \
      state->fixed_height_stack.auto_pop = 0;       \
   }                                                \
   if (state->pref_width_stack.auto_pop) {          \
      UI_PopPrefWidth();                            \
      state->pref_width_stack.auto_pop = 0;         \
   }                                                \
   if (state->pref_height_stack.auto_pop) {         \
      UI_PopPrefHeight();                           \
      state->pref_height_stack.auto_pop = 0;        \
   }                                                \
   if (state->min_width_stack.auto_pop) {           \
      UI_PopMinWidth();                             \
      state->min_width_stack.auto_pop = 0;          \
   }                                                \
   if (state->min_height_stack.auto_pop) {          \
      UI_PopMinHeight();                            \
      state->min_height_stack.auto_pop = 0;         \
   }                                                \
   if (state->permission_flags_stack.auto_pop) {    \
      UI_PopPermissionFlags();                      \
      state->permission_flags_stack.auto_pop = 0;   \
   }                                                \
   if (state->flags_stack.auto_pop) {               \
      UI_PopFlags();                                \
      state->flags_stack.auto_pop = 0;              \
   }                                                \
   if (state->omit_flags_stack.auto_pop) {          \
      UI_PopOmitFlags();                            \
      state->omit_flags_stack.auto_pop = 0;         \
   }                                                \
   if (state->focus_hot_stack.auto_pop) {           \
      UI_PopFocusHot();                             \
      state->focus_hot_stack.auto_pop = 0;          \
   }                                                \
   if (state->focus_active_stack.auto_pop) {        \
      UI_PopFocusActive();                          \
      state->focus_active_stack.auto_pop = 0;       \
   }                                                \
   if (state->fastpath_codepoint_stack.auto_pop) {  \
      UI_PopFastpathCodepoint();                    \
      state->fastpath_codepoint_stack.auto_pop = 0; \
   }                                                \
   if (state->group_key_stack.auto_pop) {           \
      UI_PopGroupKey();                             \
      state->group_key_stack.auto_pop = 0;          \
   }                                                \
   if (state->transparency_stack.auto_pop) {        \
      UI_PopTransparency();                         \
      state->transparency_stack.auto_pop = 0;       \
   }                                                \
   if (state->tag_stack.auto_pop) {                 \
      UI_PopColorTag();                                  \
      state->tag_stack.auto_pop = 0;                \
   }                                                \
   if (state->background_color_stack.auto_pop) {    \
      UI_PopBackgroundColor();                      \
      state->background_color_stack.auto_pop = 0;   \
   }                                                \
   if (state->text_color_stack.auto_pop) {          \
      UI_PopTextColor();                            \
      state->text_color_stack.auto_pop = 0;         \
   }                                                \
   if (state->border_color_stack.auto_pop) {        \
      UI_PopBorderColor();                          \
      state->border_color_stack.auto_pop = 0;       \
   }                                                \
   if (state->squish_stack.auto_pop) {              \
      UI_PopSquish();                               \
      state->squish_stack.auto_pop = 0;             \
   }                                                \
   if (state->hover_cursor_stack.auto_pop) {        \
      UI_PopHoverCursor();                          \
      state->hover_cursor_stack.auto_pop = 0;       \
   }                                                \
   if (state->font_stack.auto_pop) {                \
      UI_PopFont();                                 \
      state->font_stack.auto_pop = 0;               \
   }                                                \
   if (state->font_size_stack.auto_pop) {           \
      UI_PopFontSize();                             \
      state->font_size_stack.auto_pop = 0;          \
   }                                                \
   if (state->text_raster_flags_stack.auto_pop) {   \
      UI_PopTextRasterFlags();                      \
      state->text_raster_flags_stack.auto_pop = 0;  \
   }                                                \
   if (state->tab_size_stack.auto_pop) {            \
      UI_PopTabSize();                              \
      state->tab_size_stack.auto_pop = 0;           \
   }                                                \
   if (state->corner_radius_00_stack.auto_pop) {    \
      UI_PopCornerRadius00();                       \
      state->corner_radius_00_stack.auto_pop = 0;   \
   }                                                \
   if (state->corner_radius_01_stack.auto_pop) {    \
      UI_PopCornerRadius01();                       \
      state->corner_radius_01_stack.auto_pop = 0;   \
   }                                                \
   if (state->corner_radius_10_stack.auto_pop) {    \
      UI_PopCornerRadius10();                       \
      state->corner_radius_10_stack.auto_pop = 0;   \
   }                                                \
   if (state->corner_radius_11_stack.auto_pop) {    \
      UI_PopCornerRadius11();                       \
      state->corner_radius_11_stack.auto_pop = 0;   \
   }                                                \
   if (state->blur_size_stack.auto_pop) {           \
      UI_PopBlurSize();                             \
      state->blur_size_stack.auto_pop = 0;          \
   }                                                \
   if (state->text_padding_stack.auto_pop) {        \
      UI_PopTextPadding();                          \
      state->text_padding_stack.auto_pop = 0;       \
   }                                                \
   if (state->text_alignment_stack.auto_pop) {      \
      UI_PopTextAlignment();                        \
      state->text_alignment_stack.auto_pop = 0;     \
   }

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
#endif // UI_META_H
