// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef RENDER_CORE_H
#define RENDER_CORE_H

////////////////////////////////
//~ rjf: Generated Code

#include "generated/render.meta.h"

////////////////////////////////
//~ rjf: Enums

typedef U8 R_ChannelCode; // 3 bits
typedef enum R_ChannelCodeEnum
{
	R_ChannelCode_Null,
	R_ChannelCode_R,
	R_ChannelCode_G,
	R_ChannelCode_B,
	R_ChannelCode_A,
}
R_ChannelCodeEnum;

typedef U8 R_ChannelSizeKind; // 3 bits
typedef enum R_ChannelSizeKindEnum
{
	R_ChannelSizeKind_Null,
	R_ChannelSizeKind_2,
	R_ChannelSizeKind_8,
	R_ChannelSizeKind_10,
	R_ChannelSizeKind_11,
	R_ChannelSizeKind_16,
	R_ChannelSizeKind_24,
	R_ChannelSizeKind_32,
}
R_ChannelSizeKindEnum;

typedef U8 R_ChannelTypeKind; // 3 bits
typedef enum R_ChannelTypeKindEnum
{
	R_ChannelTypeKind_Null,
	R_ChannelTypeKind_UInt,
	R_ChannelTypeKind_SInt,
	R_ChannelTypeKind_UNorm,
	R_ChannelTypeKind_SNorm,
	R_ChannelTypeKind_Float,
}
R_ChannelTypeKindEnum;

typedef U64 R_Tex2DFmt;
//
// set of channels, each channel including {code, size, type kind}, 3 bits each:
//   [0, 3) -> channel code
//   [3, 6) -> channel size
//   [6, 9) -> channel type kind
//
// 9 bits per channel, * number of channels, e.g. 4 channels -> 36 bits

#define R_Channel(channel_idx, code_name, size_kind_name, type_kind_name) ((((U64)(R_ChannelCode_##code_name & 0x7)) | ((U64)(R_ChannelSizeKind_##size_kind_name & 0x7) << 3) | ((U64)(R_ChannelTypeKind_##type_kind_name & 0x7) << 6)) << ((channel_idx)*9))
#define r_code_from_tex2dfmt_channel(fmt, channel_idx) ((R_ChannelCode)(((fmt) & (0x7<<((channel_idx)*9))) >> ((channel_idx)*9)))
#define r_size_kind_from_tex2dfmt_channel(fmt, channel_idx) ((R_ChannelSizeKind)(((fmt) & (0x38<<((channel_idx)*9))) >> ((channel_idx)*9 + 3)))
#define r_type_kind_from_tex2dfmt_channel(fmt, channel_idx) ((R_ChannelTypeKind)(((fmt) & (0x1c0<<((channel_idx)*9))) >> ((channel_idx)*9 + 6)))

#define R_Tex2DFmt_R8    (R_Channel(0, R, 8, UInt))
#define R_Tex2DFmt_RG8   (R_Channel(0, R, 8, UInt) | R_Channel(1, G, 8, UInt))
#define R_Tex2DFmt_RGB8  (R_Channel(0, R, 8, UInt) | R_Channel(1, G, 8, UInt) | R_Channel(2, B, 8, UInt))
#define R_Tex2DFmt_RGBA8 (R_Channel(0, R, 8, UInt) | R_Channel(1, G, 8, UInt) | R_Channel(2, B, 8, UInt) | R_Channel(3, A, 8, UInt))

typedef U32 R_GeoVertexFlags;
enum
{
	R_GeoVertexFlag_TexCoord = (1<<0),
	R_GeoVertexFlag_Normals  = (1<<1),
	R_GeoVertexFlag_RGB      = (1<<2),
	R_GeoVertexFlag_RGBA     = (1<<3),
};

////////////////////////////////
//~ rjf: Handle Type

typedef union R_Handle R_Handle;
union R_Handle
{
	U64 u64[1];
	U32 u32[2];
	U16 u16[4];
};

////////////////////////////////
//~ rjf: Instance Types

typedef struct R_Rect2DInst R_Rect2DInst;
struct R_Rect2DInst
{
	Rng2F32 dst_px;
	Rng2F32 src_px;
	Vec4F32 color;//@Override(colors) - he had colors[Cornder_COUNT] but I'm not worried about rect corners yet
	F32 corner_radii[Corner_COUNT];
	F32 border_thickness;
	F32 edge_softness;
	F32 white_texture_override;
	F32 _unused_[1];
};

typedef struct R_Mesh3DInst R_Mesh3DInst;
struct R_Mesh3DInst
{
	Mat4x4F32 xform;
};

////////////////////////////////
//~ rjf: Batch Types

typedef struct R_Batch R_Batch;
struct R_Batch
{
	U8 *v;
	U64 byte_count;
	U64 byte_cap;
};

typedef struct R_BatchNode R_BatchNode;
struct R_BatchNode
{
	R_BatchNode *next;
	R_Batch v;
};

typedef struct R_BatchList R_BatchList;
struct R_BatchList
{
	R_BatchNode *first;
	R_BatchNode *last;
	U64 batch_count;
	U64 byte_count;
	U64 bytes_per_inst;
};

typedef struct R_BatchGroup2DParams R_BatchGroup2DParams;
struct R_BatchGroup2DParams
{
	R_Handle tex;
	R_Tex2DSampleKind tex_sample_kind;
	Mat3x3F32 xform;
	Rng2F32 clip;
	F32 transparency;
};

typedef struct R_BatchGroup2DNode R_BatchGroup2DNode;
struct R_BatchGroup2DNode
{
	R_BatchGroup2DNode *next;
	R_BatchList batches;
	R_BatchGroup2DParams params;
};

typedef struct R_BatchGroup2DList R_BatchGroup2DList;
struct R_BatchGroup2DList
{
	R_BatchGroup2DNode *first;
	R_BatchGroup2DNode *last;
	U64 count;
};

typedef struct R_BatchGroup3DParams R_BatchGroup3DParams;
struct R_BatchGroup3DParams
{
	R_Handle mesh_vertices;
	R_Handle mesh_indices;
	R_GeoTopologyKind mesh_geo_topology;
	R_GeoVertexFlags mesh_geo_vertex_flags;
	R_Handle albedo_tex;
	R_Tex2DSampleKind albedo_tex_sample_kind;
	Mat4x4F32 xform;
};

typedef struct R_BatchGroup3DMapNode R_BatchGroup3DMapNode;
struct R_BatchGroup3DMapNode
{
	R_BatchGroup3DMapNode *next;
	U64 hash;
	R_BatchList batches;
	R_BatchGroup3DParams params;
};

typedef struct R_BatchGroup3DMap R_BatchGroup3DMap;
struct R_BatchGroup3DMap
{
	R_BatchGroup3DMapNode **slots;
	U64 slots_count;
};

////////////////////////////////
//~ rjf: Pass Types

typedef struct R_PassParams_UI R_PassParams_UI;
struct R_PassParams_UI
{
	R_BatchGroup2DList rects;
};

typedef struct R_PassParams_Blur R_PassParams_Blur;
struct R_PassParams_Blur
{
	Rng2F32 rect;
	Rng2F32 clip;
	F32 blur_size;
	F32 corner_radii[Corner_COUNT];
};

typedef struct R_PassParams_Geo3D R_PassParams_Geo3D;
struct R_PassParams_Geo3D
{
	Rng2F32 viewport;
	Rng2F32 clip;
	Mat4x4F32 view;
	Mat4x4F32 projection;
	R_BatchGroup3DMap mesh_batches;
};

// @Added(R_PassParams_Test)
typedef struct R_PassParams_Test R_PassParams_Test;
struct R_PassParams_Test {
	R_Batch rects;
	int num_rects;
	R_Handle texture;	
	R_Tex2DSampleKind tex_sample_kind;
	Rng2F32 clip;
	F32 transparency;
};

// @Override(R_Pass)
typedef struct R_Pass R_Pass;
struct R_Pass
{
	R_PassKind kind;
	union
	{
		void *params;
		R_PassParams_Test *params_test;//Added this
		R_PassParams_UI *params_ui;
		R_PassParams_Blur *params_blur;
		R_PassParams_Geo3D *params_geo3d;
	};
};

typedef struct R_PassNode R_PassNode;
struct R_PassNode
{
	R_PassNode *next;
	R_Pass v;
};

typedef struct R_PassList R_PassList;
struct R_PassList
{
	R_PassNode *first;
	R_PassNode *last;
	U64 count;
};

////////////////////////////////
//~ bgz: Frame Stats
//
// What the renderer did for one frame, counted as it drew it. Core_R_LastFrameStats
// gives the last finished frame's: read during a frame, that is the one before.
// Frames Windows asks for while the window is being resized count too.

typedef struct R_FrameStats R_FrameStats;
struct R_FrameStats
{
	U64     frame_index;  // from 0, one per Core_R_WindowEndFrame
	Vec2S32 window_size;  // the client area drawn to, in pixels
	B32     resized;      // the size changed: swapchain and stage image were made again this frame
	U32     pass_count;   // render passes submitted (Core_R_WindowSubmit's R_PassList entries)
	U32     draw_count;   // draw calls, the final copy to the window included
	U64     rect_count;   // rectangles drawn - the instances of the UI and test passes' draws
	U64     upload_bytes; // instance data sent to the GPU
	F32     build_ms;     // Core_R_WindowBeginFrame to Present: the editor's frame and the renderer's submission
	F32     present_ms;   // Present alone - mostly waiting for vsync
	F32     frame_ms;     // end of the previous frame to the end of this one
};

exported R_FrameStats Core_R_LastFrameStats(void);

////////////////////////////////
//~ bgz: Messages
//
// Sokol's warnings and errors, and the D3D11 debug layer's, kept until
// Core_R_TakeMessages takes them. Kept rather than logged: sokol also reports
// during Core_R_Init and around Present, where no log scope is open.

typedef enum R_MessageLevel
{
	R_MessageLevel_Panic, // sokol aborts right after, so this one never gets taken
	R_MessageLevel_Error,
	R_MessageLevel_Warning,
	R_MessageLevel_Info,
} R_MessageLevel;

typedef struct R_Message R_Message;
struct R_Message
{
	R_Message*     next;
	R_MessageLevel level;
	String8        tag;         // "sokol", or "d3d11" for the debug layer
	U32            id;          // SG_LOGITEM_* or D3D11_MESSAGE_ID
	String8        text;
	String8        file;        // where sokol reported it, when it says
	U32            line;
	U64            frame_index; // the frame it first came in
	U32            count;       // times it came in before it was taken, 1 or more
};

typedef struct R_MessageList R_MessageList;
struct R_MessageList
{
	R_Message* first;
	R_Message* last;
	U64        count;
	U64        dropped_count; // came in past the renderer's cap, not kept
};

// Copies the waiting messages onto arena and clears them.
exported R_MessageList Core_R_TakeMessages(Arena* arena);


////////////////////////////////
//~ rjf: Helpers

exported U64 	   Core_R_BytesPerPixelFromTex2DFmt(R_Tex2DFmt fmt);
exported Mat4x4F32 Core_R_SampleChannelMapFromTex2DFmt(R_Tex2DFmt fmt);
exported Mat4x4F32 Core_R_SampleChannelMapFromTex2DFormat(R_Tex2DFormat fmt);

////////////////////////////////
//~ rjf: Handle Type Functions

exported R_Handle Core_R_HandleZero(void);
exported B32 	  Core_R_HandleMatch(R_Handle a, R_Handle b);

////////////////////////////////
//~ rjf: Batch Type Functions

exported R_BatchList Core_R_BatchListMake(U64 instance_size);
exported void* 		 Core_R_BatchListPushInst(Arena *arena, R_BatchList *list, U64 batch_inst_cap);

////////////////////////////////
//~ rjf: Pass Type Functions

exported R_Pass* Core_R_PassFromKind(Arena *arena, R_PassList *list, R_PassKind kind); // @Override(Core_R_PassFromKind)

////////////////////////////////
//~ rjf: Per Backend Hooks
////////////////////////////////

//- rjf: top-level layer initialization
exported void Core_R_Init(CmdLine *cmdln);

//- rjf: window setup/teardown
exported R_Handle  Core_R_WindowEquip(WM_Window window);
exported void      Core_R_WindowUnequip(WM_Window window, R_Handle window_equip);

//- rjf: textures
exported R_Handle       Core_R_Tex2DAlloc(R_ResourceKind kind, Vec2S32 size, R_Tex2DFormat format, void *data);
exported void           Core_R_Tex2DRelease(R_Handle texture);
exported R_ResourceKind Core_R_KindFromTex2D(R_Handle texture);
exported Vec2S32        Core_R_SizeFromTex2D(R_Handle texture);
exported R_Tex2DFormat  Core_R_FormatFromTex2D(R_Handle texture);
exported void           Core_R_FillTex2DRegion(R_Handle texture, Rng2S32 subrect, void *data);

//- rjf: buffers
exported R_Handle  Core_R_BufferAlloc(R_ResourceKind kind, U64 size, void *data);
exported void      Core_R_BufferRelease(R_Handle buffer);

//- rjf: frame markers
exported void Core_R_BeginFrame(void);
exported void Core_R_EndFrame(void);
// Waits until a frame started now will be shown at the next vblank. Call it
// before reading the frame's input (main.c's loop).
exported void Core_R_WaitForFrame(void);
exported void Core_R_WindowBeginFrame(WM_Window window, R_Handle window_equip);
exported void Core_R_WindowEndFrame(WM_Window window, R_Handle window_equip);

//- rjf: render pass submission
exported void Core_R_WindowSubmit(WM_Window window, R_Handle window_equip, R_PassList *passes);

exported void Core_R_Shutdown(void);

#endif // RENDER_CORE_H
