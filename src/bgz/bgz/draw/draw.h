#pragma once

////////////////////////////////
//~ rjf: Generated Code

// #include "generated/draw.meta.h"

////////////////////////////////
//~ rjf: Fancy String Types

typedef struct DR_FStrParams DR_FStrParams;
struct DR_FStrParams {
   FNT_Tag         font;
   FNT_RasterFlags raster_flags;
   Vec4F32         color;
   F32             size;
   F32             underline_thickness;
   F32             strikethrough_thickness;
};

typedef struct DR_FStr DR_FStr;
struct DR_FStr {
   String8       string;
   DR_FStrParams params;
};

typedef struct DR_FStrNode DR_FStrNode;
struct DR_FStrNode {
   DR_FStrNode* next;
   DR_FStr      v;
};

typedef struct DR_FStrList DR_FStrList;
struct DR_FStrList {
   DR_FStrNode* first;
   DR_FStrNode* last;
   U64          node_count;
   U64          total_size;
};

typedef struct DR_FRun DR_FRun;
struct DR_FRun {
   FNT_Run run;
   Vec4F32 color;
   F32     underline_thickness;
   F32     strikethrough_thickness;
   B32     icon;
};

typedef struct DR_FRunNode DR_FRunNode;
struct DR_FRunNode {
   DR_FRunNode* next;
   DR_FRun      v;
};

typedef struct DR_FRunList DR_FRunList;
struct DR_FRunList {
   DR_FRunNode* first;
   DR_FRunNode* last;
   U64          node_count;
   Vec2F32      dim;
};

////////////////////////////////
//~ rjf: Draw Bucket Types

// @Added(clip stack) A bucket's clip rects, innermost on top.
typedef struct D_ClipNode D_ClipNode;
struct D_ClipNode {
   D_ClipNode* next;
   Rng2F32     v;
};

typedef struct D_Bucket D_Bucket;
struct D_Bucket {
   R_PassList  passes;
   U64         stack_gen;
   U64         last_cmd_stack_gen;
   D_ClipNode* top_clip; // @Added(clip stack)
   // D_BucketStackDecls;
};

////////////////////////////////
//~ rjf: Thread Context

typedef struct D_BucketSelectionNode D_BucketSelectionNode;
struct D_BucketSelectionNode {
   D_BucketSelectionNode* next;
   D_Bucket*              bucket;
};

typedef struct D_ThreadCtx D_ThreadCtx;
struct D_ThreadCtx {
   Arena*                 arena;
   U64                    arena_frame_start_pos;
   D_BucketSelectionNode* top_bucket;
   D_BucketSelectionNode* free_bucket_selection;
   Log*                   draw_log;
};

////////////////////////////////
//~ rjf: Globals

thread_static D_ThreadCtx* d_thread_ctx = 0;

////////////////////////////////
//~ rjf: Top-Level API
//
// (Frame boundaries & bucket submission)

exported void Core_D_BeginFrame(void);
exported void Core_D_SubmitBucket(WM_Window window, R_Handle window_equip, D_Bucket* bucket);

////////////////////////////////
//~ rjf: Bucket Construction & Selection API
//
// (Bucket: Handle to sequence of many render passes, constructed by this layer)

exported D_Bucket* Core_D_BucketMake(void);
exported void      Core_D_PushBucket(D_Bucket* bucket);
exported void      Core_D_PopBucket(void);
exported D_Bucket* Core_D_TopBucket(void);
#define Core_D_BucketScope(b) DeferLoop(Core_D_PushBucket(b), Core_D_PopBucket())

////////////////////////////////
//~ bgz: Clip Stack @Added(clip stack)
//
// Rects and images land only inside the selected bucket's top clip. The clip
// rides on the batch group (R_BatchGroup2DParams.clip), which the renderer turns
// into a scissor rect; a change of clip starts a new group. A zero rect means
// no clip - the whole window - as the renderer reads it.

exported void    Core_D_PushClip(Rng2F32 clip);
exported void    Core_D_PopClip(void);
exported Rng2F32 Core_D_TopClip(void);

////////////////////////////////
//~ rjf: Core Draw Calls
//
// (Apply to the calling thread's currently selected bucket)

//- rjf: rectangles
exported R_Rect2DInst* Core_D_Rect(Rng2F32 dst, Rng2F32 tex_uv, Vec4F32 color);
exported R_Rect2DInst* Core_D_Img(Rng2F32 dst, Rng2F32 src, R_Handle texture, Vec4F32 color);
exported void          Core_D_Text(Vec2F32 pos, Vec4F32 color, FNT_Run run);

////////////////////////////////
// Used for testing path to create a sort of sandbox for render testing
//

exported void Core_D_Test_Rect(Rng2F32 dst, Rng2F32 tex_uv, Vec4F32 color);
exported void Core_D_Test_Img(Rng2F32 dst, Rng2F32 uv, R_Handle texture, Vec4F32 color);
exported void Core_D_Test_Text(FNT_Tag font, F32 size, Vec2F32 p, String8 string);
exported void Core_D_TruncatedFancyRunList(Vec2F32 p, DR_FRunList* list, F32 max_x, FNT_Run trailer_run);
exported void Core_D_TextRun(Vec2F32 p, Vec4F32 color, FNT_Run run);

////////////////////////////////
//~ rjf: Fancy String Type Functions

exported DR_FStrList Core_D_FStrs_Copy(Arena* arena, DR_FStrList* src);
exported void        Core_D_FStrs_PushFront(Arena* arena, DR_FStrList* list, DR_FStr* str);
exported void        Core_D_FStrs_Push(Arena* arena, DR_FStrList* list, DR_FStr* str);
exported void        Core_D_FStrs_PushNew(Arena* arena, DR_FStrList* list, DR_FStrParams* params, DR_FStrParams* overrides, String8 string);
exported String8     Core_D_FStrs_String(Arena* arena, DR_FStrList* list);
exported DR_FRunList Core_D_FStrs_Fruns(Arena* arena, F32 tab_size_px, DR_FStrList* strs);

#if 0
////////////////////////////////
//~ rjf: Bucket Stacks
//
// (Pushing/popping implicit draw parameters)

internal R_Tex2DSampleKind          d_push_tex2d_sample_kind(R_Tex2DSampleKind v);
internal Mat3x3F32                  d_push_xform2d(Mat3x3F32 v);
internal Rng2F32                    d_push_clip(Rng2F32 v);
internal F32                        d_push_transparency(F32 v);
internal R_Tex2DSampleKind          d_pop_tex2d_sample_kind(void);
internal Mat3x3F32                  d_pop_xform2d(void);
internal Rng2F32                    d_pop_clip(void);
internal F32                        d_pop_transparency(void);
internal R_Tex2DSampleKind          d_top_tex2d_sample_kind(void);
internal Mat3x3F32                  d_top_xform2d(void);
internal Rng2F32                    d_top_clip(void);
internal F32                        d_top_transparency(void);

#define Core_D_Tex2DSampleKindScope(v) DeferLoop(d_push_tex2d_sample_kind(v), d_pop_tex2d_sample_kind())
#define Core_D_XForm2DScope(v) DeferLoop(d_push_xform2d(v), d_pop_xform2d())
#define Core_D_ClipScope(v) DeferLoop(d_push_clip(v), d_pop_clip())
#define Core_D_TransparencyScope(v) DeferLoop(d_push_transparency(v), d_pop_transparency())

////////////////////////////////
//~ rjf: Core Draw Calls
//
// (Apply to the calling thread's currently selected bucket)

//- rjf: blurs
internal R_PassParams_Blur *d_blur(Rng2F32 rect, F32 blur_size, F32 corner_radius);

//- rjf: 3d rendering pass params
internal R_PassParams_Geo3D *d_geo3d_begin(Rng2F32 viewport, Mat4x4F32 view, Mat4x4F32 projection);

//- rjf: meshes
internal R_Mesh3DInst *d_mesh(R_Handle mesh_vertices, R_Handle mesh_indices, R_GeoTopologyKind mesh_geo_topology, R_GeoVertexFlags mesh_geo_vertex_flags, R_Handle albedo_tex, Mat4x4F32 inst_xform);

//- rjf: collating one pre-prepped bucket into parent bucket
internal void d_sub_bucket(D_Bucket *bucket);

////////////////////////////////
//~ rjf: Draw Call Helpers

//- rjf: text
internal void d_truncated_fancy_run_list(Vec2F32 p, D_FancyRunList *list, F32 max_x, F_Run trailer_run);
internal void d_text_run(Vec2F32 p, Vec4F32 color, F_Run run);
internal void d_truncated_text_run(Vec2F32 p, Vec4F32 color, F32 max_x, F_Run text_run, F_Run trailer_run);
internal void Core_D_TruncatedText(FC_Tag font, F32 size, Vec2F32 p, Vec4F32 color, F32 max_x, String8 string);
internal void d_TruncatedTextF(FC_Tag font, F32 size, Vec2F32 p, Vec4F32 color, F32 max_x, char *fmt, ...);
#endif