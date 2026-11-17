// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

#ifndef FONT_CACHE_H
#define FONT_CACHE_H

////////////////////////////////
//~ rjf: Rasterization Flags

typedef U32 FNT_RasterFlags;
enum
{
  FNT_RasterFlag_Smooth = (1<<0),
  FNT_RasterFlag_Hinted = (1<<1),
};

////////////////////////////////
//~ rjf: Handles & Tags

typedef struct FNT_Tag FNT_Tag;
struct FNT_Tag
{
  U64 u64[2];
};

////////////////////////////////
//~ rjf: Draw Package Types (For Cache Queries)

typedef struct FNT_Piece FNT_Piece;
struct FNT_Piece
{
  R_Handle texture;
  Rng2S16 subrect;
  Vec2S16 offset;
  F32 advance;
  U16 decode_size;
};

typedef struct FNT_PieceChunkNode FNT_PieceChunkNode;
struct FNT_PieceChunkNode
{
  FNT_PieceChunkNode *next;
  FNT_Piece *v;
  U64 count;
  U64 cap;
};

typedef struct FNT_PieceChunkList FNT_PieceChunkList;
struct FNT_PieceChunkList
{
  FNT_PieceChunkNode *first;
  FNT_PieceChunkNode *last;
  U64 node_count;
  U64 total_piece_count;
};

typedef struct FNT_PieceArray FNT_PieceArray;
struct FNT_PieceArray
{
  FNT_Piece *v;
  U64 count;
};

typedef struct FNT_Run FNT_Run;
struct FNT_Run
{
  FNT_PieceArray pieces;
  Vec2F32 dim;
  F32 ascent;
  F32 descent;
};

////////////////////////////////
//~ rjf: Font Path -> Handle * Metrics * Path Cache Types

typedef struct FNT_FontHashNode FNT_FontHashNode;
struct FNT_FontHashNode
{
  FNT_FontHashNode *hash_next;
  FNT_Tag tag;
  FP_Handle handle;
  FP_Metrics metrics;
  String8 path;
};

typedef struct FNT_FontHashSlot FNT_FontHashSlot;
struct FNT_FontHashSlot
{
  FNT_FontHashNode *first;
  FNT_FontHashNode *last;
};

////////////////////////////////
//~ rjf: Rasterization Cache Types

//- rjf: base glyph rasterization / dimensions cache 

typedef struct FNT_RasterCacheInfo FNT_RasterCacheInfo;
struct FNT_RasterCacheInfo
{
  Rng2S16 subrect;
  Vec2S16 raster_dim;
  S16 atlas_num;
  F32 advance;
};

typedef struct FNT_Hash2InfoRasterCacheNode FNT_Hash2InfoRasterCacheNode;
struct FNT_Hash2InfoRasterCacheNode
{
  FNT_Hash2InfoRasterCacheNode *hash_next;
  FNT_Hash2InfoRasterCacheNode *hash_prev;
  U64 hash;
  FNT_RasterCacheInfo info;
};

typedef struct FNT_Hash2InfoRasterCacheSlot FNT_Hash2InfoRasterCacheSlot;
struct FNT_Hash2InfoRasterCacheSlot
{
  FNT_Hash2InfoRasterCacheNode *first;
  FNT_Hash2InfoRasterCacheNode *last;
};

//- rjf: run cache (arrangements of many glyphs to represent a full string)

typedef struct FNT_RunCacheNode FNT_RunCacheNode;
struct FNT_RunCacheNode
{
  FNT_RunCacheNode *next;
  String8 string;
  FNT_Run run;
};

typedef struct FNT_RunCacheSlot FNT_RunCacheSlot;
struct FNT_RunCacheSlot
{
  FNT_RunCacheNode *first;
  FNT_RunCacheNode *last;
};

//- rjf: style hash -> artifacts/metrics cache

typedef struct FNT_Hash2StyleRasterCacheNode FNT_Hash2StyleRasterCacheNode;
struct FNT_Hash2StyleRasterCacheNode
{
  FNT_Hash2StyleRasterCacheNode *hash_next;
  FNT_Hash2StyleRasterCacheNode *hash_prev;
  U64 style_hash;
  F32 ascent;
  F32 descent;
  F32 column_width;
  FNT_RasterCacheInfo *utf8_class1_direct_map;
  U64 utf8_class1_direct_map_mask[4];
  U64 hash2info_slots_count;
  FNT_Hash2InfoRasterCacheSlot *hash2info_slots;
  U64 run_slots_count;
  FNT_RunCacheSlot *run_slots;
  U64 run_slots_frame_index;
};

typedef struct FNT_Hash2StyleRasterCacheSlot FNT_Hash2StyleRasterCacheSlot;
struct FNT_Hash2StyleRasterCacheSlot
{
  FNT_Hash2StyleRasterCacheNode *first;
  FNT_Hash2StyleRasterCacheNode *last;
};

////////////////////////////////
//~ rjf: Atlas Types

typedef U32 FNT_AtlasRegionNodeFlags;
enum
{
  FNT_AtlasRegionNodeFlag_Taken = (1<<0),
};

typedef struct FNT_AtlasRegionNode FNT_AtlasRegionNode;
struct FNT_AtlasRegionNode
{
  FNT_AtlasRegionNode *parent;
  FNT_AtlasRegionNode *children[Corner_COUNT];
  Vec2S16 max_free_size[Corner_COUNT];
  FNT_AtlasRegionNodeFlags flags;
  U64 num_allocated_descendants;
};

typedef struct FNT_Atlas FNT_Atlas;
struct FNT_Atlas
{
  FNT_Atlas *next;
  FNT_Atlas *prev;
  R_Handle texture;
  Vec2S16 root_dim;
  FNT_AtlasRegionNode *root;
};

////////////////////////////////
//~ rjf: Metrics

typedef struct FNT_Metrics FNT_Metrics;
struct FNT_Metrics
{
  F32 ascent;
  F32 descent;
  F32 line_gap;
  F32 capital_height;
};

////////////////////////////////
//~ rjf: Main State Type

typedef struct FNT_State FNT_State;
struct FNT_State
{
  Arena *permanent_arena;
  Arena *raster_arena;
  Arena *frame_arena;
  U64 frame_index;
  
  // rjf: font table
  U64 font_hash_table_size;
  FNT_FontHashSlot *font_hash_table;
  
  // rjf: hash -> raster cache table
  U64 hash2style_slots_count;
  FNT_Hash2StyleRasterCacheSlot *hash2style_slots;
  
  // rjf: atlas list
  FNT_Atlas *first_atlas;
  FNT_Atlas *last_atlas;
};

////////////////////////////////
//~ rjf: Globals

global FNT_State *fnt_state = 0;

////////////////////////////////
//~ rjf: Basic Functions

exported U128 Core_FNT_HashFromString(String8 string);
exported U64 Core_FNT_LittleHashFromString(U64 seed, String8 string);
exported Vec2S32 Core_FNT_VertexFromCorner(Corner corner);

////////////////////////////////
//~ rjf: Font Tags

exported FNT_Tag Core_FNT_TagZero(void);
exported B32 Core_FNT_TagMatch(FNT_Tag a, FNT_Tag b);
exported FP_Handle Core_FNT_HandleFromTag(FNT_Tag tag);
exported FP_Metrics Core_FNT_FpMetricsFromTag(FNT_Tag tag);
exported FNT_Tag Core_FNT_TagFromPath(String8 path);
exported FNT_Tag Core_FNT_TagFromStaticDataString(String8 *data_ptr);
exported String8 Core_FNT_PathFromTag(FNT_Tag tag);

////////////////////////////////
//~ rjf: Atlas

exported Rng2S16 Core_FNT_AtlasRegionAlloc(Arena *arena, FNT_Atlas *atlas, Vec2S16 needed_size);
exported void Core_FNT_AtlasRegionRelease(FNT_Atlas *atlas, Rng2S16 region);

////////////////////////////////
//~ rjf: Piece Type Functions

exported FNT_Piece *Core_FNT_PieceChunkListPushNew(Arena *arena, FNT_PieceChunkList *list, U64 cap);
exported void Core_FNT_PieceChunkListPush(Arena *arena, FNT_PieceChunkList *list, U64 cap, FNT_Piece *piece);
exported FNT_PieceArray Core_FNT_PieceArrayFromChunkList(Arena *arena, FNT_PieceChunkList *list);
exported FNT_PieceArray Core_FNT_PieceArrayCopy(Arena *arena, FNT_PieceArray *src);

////////////////////////////////
//~ rjf: Cache Usage

//- rjf: base cache lookups
exported FNT_Hash2StyleRasterCacheNode *Core_FNT_Hash2StyleFromTagSizeFlags(FNT_Tag tag, F32 size, FNT_RasterFlags flags);
exported FNT_Run Core_FNT_RunFromString(FNT_Tag tag, F32 size, F32 base_align_px, F32 tab_size_px, FNT_RasterFlags flags, String8 string);

//- rjf: helpers
exported String8List Core_FNT_WrappedStringLinesFromFontSizeStringMax(Arena *arena, FNT_Tag font, F32 size, F32 base_align_px, F32 tab_size_px, String8 string, F32 max);
exported Vec2F32 Core_FNT_DimFromTagSizeString(FNT_Tag tag, F32 size, F32 base_align_px, F32 tab_size_px, String8 string);
exported Vec2F32 Core_FNT_DimFromTagSizeStringList(FNT_Tag tag, F32 size, F32 base_align_px, F32 tab_size_px, String8List list);
exported F32 Core_FNT_ColumnSizeFromTagSize(FNT_Tag tag, F32 size);
exported U64 Core_FNT_CharPosFromTagSizeStringP(FNT_Tag tag, F32 size, F32 base_align_px, F32 tab_size_px, String8 string, F32 p);

////////////////////////////////
//~ rjf: Metrics

exported FNT_Metrics Core_FNT_MetricsFromTagSize(FNT_Tag tag, F32 size);
exported F32 Core_FNT_LineHeightFromMetrics(FNT_Metrics *metrics);

////////////////////////////////
//~ rjf: Main Calls

exported void Core_FNT_Init(void);
exported void Core_FNT_Reset(void);
exported void Core_FNT_Frame(void);

#endif // FONT_CACHE_H
