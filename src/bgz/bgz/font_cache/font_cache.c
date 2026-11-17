// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

// @Changed(rad_font_provider) calls raddebugger's font provider through bgz's
// (Core_FP_*, bgz/font_provider/font_provider.h).

////////////////////////////////
//~ rjf: Basic Functions

#if !defined(XXH_IMPLEMENTATION)
#define XXH_IMPLEMENTATION
#define XXH_STATIC_LINKING_ONLY
#include "third_party/xxHash/xxhash.h"
#endif

U128 Core_FNT_HashFromString(String8 string) {
   union {
      XXH128_hash_t xxhash;
      U128          u128;
   } hash;
   hash.xxhash = XXH3_128bits(string.str, string.size);
   return hash.u128;
}

U64 Core_FNT_LittleHashFromString(U64 seed, String8 string) {
   U64 result = XXH3_64bits_withSeed(string.str, string.size, seed);
   return result;
}

Vec2S32
Core_FNT_VertexFromCorner(Corner corner) {
   Vec2S32 result = {0};
   switch (corner) {
   default:
      break;
   case Corner_00: {
      result = v2s32(0, 0);
   } break;
   case Corner_01: {
      result = v2s32(0, 1);
   } break;
   case Corner_10: {
      result = v2s32(1, 0);
   } break;
   case Corner_11: {
      result = v2s32(1, 1);
   } break;
   }
   return result;
}

////////////////////////////////
//~ rjf: Font Tags

FNT_Tag
Core_FNT_TagZero(void) {
   FNT_Tag result = {0};
   return result;
}

B32 Core_FNT_TagMatch(FNT_Tag a, FNT_Tag b) {
   return a.u64[0] == b.u64[0] && a.u64[1] == b.u64[1];
}

FP_Handle
Core_FNT_HandleFromTag(FNT_Tag tag) {
   ProfBeginFunction();
   U64               slot_idx = tag.u64[1] % fnt_state->font_hash_table_size;
   FNT_FontHashNode* existing_node = 0;
   {
      for (FNT_FontHashNode* n = fnt_state->font_hash_table[slot_idx].first; n != 0; n = n->hash_next) {
         if (MemoryMatchStruct(&tag, &n->tag)) {
            existing_node = n;
            break;
         }
      }
   }
   FP_Handle result = {0};
   if (existing_node != 0) {
      result = existing_node->handle;
   }
   ProfEnd();
   return result;
}

FP_Metrics
Core_FNT_FpMetricsFromTag(FNT_Tag tag) {
   ProfBeginFunction();
   U64               slot_idx = tag.u64[1] % fnt_state->font_hash_table_size;
   FNT_FontHashNode* existing_node = 0;
   {
      for (FNT_FontHashNode* n = fnt_state->font_hash_table[slot_idx].first; n != 0; n = n->hash_next) {
         if (MemoryMatchStruct(&tag, &n->tag)) {
            existing_node = n;
            break;
         }
      }
   }
   FP_Metrics result = {0};
   if (existing_node != 0) {
      result = existing_node->metrics;
   }
   ProfEnd();
   return result;
}


FNT_Tag
Core_FNT_TagFromPath(String8 path) {
   ProfBeginFunction();

   //- rjf: produce tag from hash of path
   FNT_Tag result = {0};
   {
      U128 hash = Core_FNT_HashFromString(path);
      MemoryCopy(&result, &hash, sizeof(result));
      result.u64[1] |= bit64;
   }

   //- rjf: tag -> slot index
   U64 slot_idx = result.u64[1] % fnt_state->font_hash_table_size;

   //- rjf: slot * tag -> existing node
   FNT_FontHashNode* existing_node = 0;
   {
      for (FNT_FontHashNode* n = fnt_state->font_hash_table[slot_idx].first; n != 0; n = n->hash_next) {
         if (MemoryMatchStruct(&result, &n->tag)) {
            existing_node = n;
            break;
         }
      }
   }

   //- rjf: allocate & push new node if we don't have an existing one
   if (existing_node == 0) {
      FP_Handle         handle = Core_FP_FontOpen(path);
      FNT_FontHashSlot* slot = &fnt_state->font_hash_table[slot_idx];
      existing_node = push_array(fnt_state->permanent_arena, FNT_FontHashNode, 1);
      existing_node->tag = result;
      existing_node->handle = handle;
      existing_node->metrics = Core_FP_MetricsFromFont(existing_node->handle);
      existing_node->path = push_str8_copy(fnt_state->permanent_arena, path);
      SLLQueuePush_N(slot->first, slot->last, existing_node, hash_next);
   }

   //- rjf: tag result must be zero if this is not a valid font
   if (Core_FP_HandleMatch(existing_node->handle, Core_FP_HandleZero())) {
      MemoryZeroStruct(&result);
   }

   //- rjf: return
   ProfEnd();
   return result;
}

FNT_Tag
Core_FNT_TagFromStaticDataString(String8* data_ptr) {
   ProfBeginFunction();

   //- rjf: produce tag hash of ptr
   FNT_Tag result = {0};
   {
      U128 hash = Core_FNT_HashFromString(str8((U8*)&data_ptr, sizeof(String8*)));
      MemoryCopy(&result, &hash, sizeof(result));
      result.u64[1] &= ~bit64;
   }

   //- rjf: tag -> slot index
   U64 slot_idx = result.u64[1] % fnt_state->font_hash_table_size;

   //- rjf: slot * tag -> existing node
   FNT_FontHashNode* existing_node = 0;
   {
      for (FNT_FontHashNode* n = fnt_state->font_hash_table[slot_idx].first; n != 0; n = n->hash_next) {
         if (MemoryMatchStruct(&result, &n->tag)) {
            existing_node = n;
            break;
         }
      }
   }

   //- rjf: allocate & push new node if we don't have an existing one
   FNT_FontHashNode* new_node = 0;
   if (existing_node == 0) {
      FNT_FontHashSlot* slot = &fnt_state->font_hash_table[slot_idx];
      new_node = push_array(fnt_state->permanent_arena, FNT_FontHashNode, 1);
      new_node->tag = result;
      new_node->handle = Core_FP_FontOpenFromStaticDataString(data_ptr);
      new_node->metrics = Core_FP_MetricsFromFont(new_node->handle);
      new_node->path = str8_lit("");
      SLLQueuePush_N(slot->first, slot->last, new_node, hash_next);
   }

   //- rjf: return
   ProfEnd();
   return result;
}

String8
Core_FNT_PathFromTag(FNT_Tag tag) {
   //- rjf: tag -> slot index
   U64 slot_idx = tag.u64[1] % fnt_state->font_hash_table_size;

   //- rjf: slot * tag -> existing node
   FNT_FontHashNode* existing_node = 0;
   {
      for (FNT_FontHashNode* n = fnt_state->font_hash_table[slot_idx].first; n != 0; n = n->hash_next) {
         if (MemoryMatchStruct(&tag, &n->tag)) {
            existing_node = n;
            break;
         }
      }
   }

   //- rjf: existing node -> path
   String8 result = {0};
   if (existing_node != 0) {
      result = existing_node->path;
   }

   return result;
}

////////////////////////////////
//~ rjf: Atlas

Rng2S16
Core_FNT_AtlasRegionAlloc(Arena* arena, FNT_Atlas* atlas, Vec2S16 needed_size) {
   ProfBeginFunction();

   //- rjf: find node with best-fit size
   Vec2S16              region_p0 = {0};
   Vec2S16              region_sz = {0};
   Corner               node_corner = Corner_Invalid;
   FNT_AtlasRegionNode* node = 0;
   {
      Vec2S16 n_supported_size = atlas->root_dim;
      for (FNT_AtlasRegionNode *n = atlas->root, *next = 0; n != 0; n = next, next = 0) {
         // rjf: we've traversed to a taken node.
         if (n->flags & FNT_AtlasRegionNodeFlag_Taken) {
            break;
         }

         // rjf: calculate if this node can be allocated (all children are non-allocated)
         B32 n_can_be_allocated = (n->num_allocated_descendants == 0);

         // rjf: fill size
         if (n_can_be_allocated) {
            region_sz = n_supported_size;
         }

         // rjf: calculate size of this node's children
         Vec2S16 child_size = v2s16(n_supported_size.x / 2, n_supported_size.y / 2);

         // rjf: find best next child
         FNT_AtlasRegionNode* best_child = 0;
         if (child_size.x >= needed_size.x && child_size.y >= needed_size.y) {
            for (Corner corner = (Corner)0; corner < Corner_COUNT; corner = (Corner)(corner + 1)) {
               if (n->children[corner] == 0) {
                  n->children[corner] = push_array(arena, FNT_AtlasRegionNode, 1);
                  n->children[corner]->parent = n;
                  n->children[corner]->max_free_size[Corner_00] =
                      n->children[corner]->max_free_size[Corner_01] =
                          n->children[corner]->max_free_size[Corner_10] =
                              n->children[corner]->max_free_size[Corner_11] = v2s16(child_size.x / 2, child_size.y / 2);
               }
               if (n->max_free_size[corner].x >= needed_size.x &&
                   n->max_free_size[corner].y >= needed_size.y) {
                  best_child = n->children[corner];
                  node_corner = corner;
                  Vec2S32 side_vertex = Core_FNT_VertexFromCorner(corner);
                  region_p0.x += side_vertex.x * child_size.x;
                  region_p0.y += side_vertex.y * child_size.y;
                  break;
               }
            }
         }

         // rjf: resolve node to this node if it can be allocated and children
         // don't fit, or keep going to the next best child
         if (n_can_be_allocated && best_child == 0) {
            node = n;
         } else {
            next = best_child;
            n_supported_size = child_size;
         }
      }
   }

   //- rjf: we're taking the subtree rooted by `node`. mark up all parents
   if (node != 0 && node_corner != Corner_Invalid) {
      node->flags |= FNT_AtlasRegionNodeFlag_Taken;
      if (node->parent != 0) {
         MemoryZeroStruct(&node->parent->max_free_size[node_corner]);
      }
      for (FNT_AtlasRegionNode* p = node->parent; p != 0; p = p->parent) {
         p->num_allocated_descendants += 1;
         FNT_AtlasRegionNode* parent = p->parent;
         if (parent != 0) {
            Corner p_corner = (p == parent->children[Corner_00] ? Corner_00 : p == parent->children[Corner_01] ? Corner_01
                                                                          : p == parent->children[Corner_10]   ? Corner_10
                                                                          : p == parent->children[Corner_11]   ? Corner_11
                                                                                                               : Corner_Invalid);
            if (p_corner == Corner_Invalid) {
               InvalidPath;
            }
            parent->max_free_size[p_corner].x = Max(Max(p->max_free_size[Corner_00].x,
                                                        p->max_free_size[Corner_01].x),
                                                    Max(p->max_free_size[Corner_10].x,
                                                        p->max_free_size[Corner_11].x));
            parent->max_free_size[p_corner].y = Max(Max(p->max_free_size[Corner_00].y,
                                                        p->max_free_size[Corner_01].y),
                                                    Max(p->max_free_size[Corner_10].y,
                                                        p->max_free_size[Corner_11].y));
         }
      }
   }

   //- rjf: fill rectangular region & return
   Rng2S16 result = {0};
   result.p0 = region_p0;
   result.p1 = add_2s16(region_p0, region_sz);
   ProfEnd();
   return result;
}

void Core_FNT_AtlasRegionRelease(FNT_Atlas* atlas, Rng2S16 region) {
   ProfBeginFunction();

   //- rjf: extract region size
   Vec2S16 region_size = v2s16(region.x1 - region.x0, region.y1 - region.y0);

   //- rjf: map region to associated node
   Vec2S16              calc_region_size = {0};
   FNT_AtlasRegionNode* node = 0;
   Corner               node_corner = Corner_Invalid;
   {
      Vec2S16 n_p0 = v2s16(0, 0);
      Vec2S16 n_sz = atlas->root_dim;
      for (FNT_AtlasRegionNode *n = atlas->root, *next = 0; n != 0; n = next) {
         // rjf: is the region within this node's boundaries? (either this node, or a descendant)
         if (n_p0.x <= region.p0.x && region.p0.x < n_p0.x + n_sz.x &&
             n_p0.y <= region.p0.y && region.p0.y < n_p0.y + n_sz.y) {
            // rjf: check the region against this node
            if (region.p0.x == n_p0.x && region.p0.y == n_p0.y &&
                region_size.x == n_sz.x && region_size.y == n_sz.y) {
               node = n;
               calc_region_size = n_sz;
               break;
            }
            // rjf: check the region against children & iterate
            else {
               Vec2S16 r_midpoint = v2s16(region.p0.x + region_size.x / 2,
                                          region.p0.y + region_size.y / 2);
               Vec2S16 n_midpoint = v2s16(n_p0.x + n_sz.x / 2,
                                          n_p0.y + n_sz.y / 2);
               Corner  next_corner = Corner_Invalid;
               if (r_midpoint.x <= n_midpoint.x && r_midpoint.y <= n_midpoint.y) {
                  next_corner = Corner_00;
               } else if (r_midpoint.x <= n_midpoint.x && n_midpoint.y <= r_midpoint.y) {
                  next_corner = Corner_01;
               } else if (n_midpoint.x <= r_midpoint.x && r_midpoint.y <= n_midpoint.y) {
                  next_corner = Corner_10;
               } else if (n_midpoint.x <= r_midpoint.x && n_midpoint.y <= r_midpoint.y) {
                  next_corner = Corner_11;
               }
               next = n->children[next_corner];
               node_corner = next_corner;
               n_sz.x /= 2;
               n_sz.y /= 2;
               Vec2S32 side_vertex = Core_FNT_VertexFromCorner(node_corner);
               n_p0.x += side_vertex.x * n_sz.x;
               n_p0.y += side_vertex.y * n_sz.y;
            }
         } else {
            break;
         }
      }
   }

   //- rjf: free node
   if (node != 0 && node_corner != Corner_Invalid) {
      node->flags &= ~FNT_AtlasRegionNodeFlag_Taken;
      if (node->parent != 0) {
         node->parent->max_free_size[node_corner] = calc_region_size;
      }
      for (FNT_AtlasRegionNode* p = node->parent; p != 0; p = p->parent) {
         p->num_allocated_descendants -= 1;
         FNT_AtlasRegionNode* parent = p->parent;
         if (parent != 0) {
            Corner p_corner = (p == parent->children[Corner_00] ? Corner_00 : p == parent->children[Corner_01] ? Corner_01
                                                                          : p == parent->children[Corner_10]   ? Corner_10
                                                                          : p == parent->children[Corner_11]   ? Corner_11
                                                                                                               : Corner_Invalid);
            if (p_corner == Corner_Invalid) {
               InvalidPath;
            }
            parent->max_free_size[p_corner].x = Max(Max(p->max_free_size[Corner_00].x,
                                                        p->max_free_size[Corner_01].x),
                                                    Max(p->max_free_size[Corner_10].x,
                                                        p->max_free_size[Corner_11].x));
            parent->max_free_size[p_corner].y = Max(Max(p->max_free_size[Corner_00].y,
                                                        p->max_free_size[Corner_01].y),
                                                    Max(p->max_free_size[Corner_10].y,
                                                        p->max_free_size[Corner_11].y));
         }
      }
   }
   ProfEnd();
}

////////////////////////////////
//~ rjf: Piece Type Functions

FNT_Piece*
Core_FNT_PieceChunkListPushNew(Arena* arena, FNT_PieceChunkList* list, U64 cap) {
   FNT_PieceChunkNode* node = list->last;
   if (node == 0 || node->count >= node->cap) {
      node = push_array(arena, FNT_PieceChunkNode, 1);
      node->v = push_array_no_zero(arena, FNT_Piece, cap);
      node->cap = cap;
      SLLQueuePush(list->first, list->last, node);
      list->node_count += 1;
   }
   FNT_Piece* result = node->v + node->count;
   node->count += 1;
   list->total_piece_count += 1;
   return result;
}

void Core_FNT_PieceChunkListPush(Arena* arena, FNT_PieceChunkList* list, U64 cap, FNT_Piece* piece) {
   FNT_Piece* new_piece = Core_FNT_PieceChunkListPushNew(arena, list, cap);
   MemoryCopyStruct(new_piece, piece);
}

FNT_PieceArray
Core_FNT_PieceArrayFromChunkList(Arena* arena, FNT_PieceChunkList* list) {
   FNT_PieceArray array = {0};
   array.count = list->total_piece_count;
   array.v = push_array_no_zero(arena, FNT_Piece, array.count);
   U64 write_idx = 0;
   for (FNT_PieceChunkNode* node = list->first; node != 0; node = node->next) {
      MemoryCopy(array.v + write_idx, node->v, node->count * sizeof(FNT_Piece));
      write_idx += node->count;
   }
   return array;
}

FNT_PieceArray
Core_FNT_PieceArrayCopy(Arena* arena, FNT_PieceArray* src) {
   FNT_PieceArray dst = {0};
   dst.count = src->count;
   dst.v = push_array_no_zero(arena, FNT_Piece, dst.count);
   MemoryCopy(dst.v, src->v, sizeof(FNT_Piece) * dst.count);
   return dst;
}

////////////////////////////////
//~ rjf: Cache Usage

FNT_Hash2StyleRasterCacheNode*
Core_FNT_Hash2StyleFromTagSizeFlags(FNT_Tag tag, F32 size, FNT_RasterFlags flags) {
   //- rjf: tag * size -> style hash
   U64 style_hash = {0};
   {
      F64 size_f64 = size;
      U64 buffer[] =
          {
              tag.u64[0],
              tag.u64[1],
              *(U64*)(&size_f64),
              (U64)flags,
          };
      style_hash = Core_FNT_LittleHashFromString(5381, str8((U8*)buffer, sizeof(buffer)));
   }

   //- rjf: style hash -> style node
   FNT_Hash2StyleRasterCacheNode* hash2style_node = 0;
   {
      ProfBegin("style hash -> style node");
      U64                            slot_idx = style_hash % fnt_state->hash2style_slots_count;
      FNT_Hash2StyleRasterCacheSlot* slot = &fnt_state->hash2style_slots[slot_idx];
      for (FNT_Hash2StyleRasterCacheNode* n = slot->first;
           n != 0;
           n = n->hash_next) {
         if (n->style_hash == style_hash) {
            hash2style_node = n;
            break;
         }
      }
      if (Unlikely(hash2style_node == 0)) {
         FNT_Metrics metrics = Core_FNT_MetricsFromTagSize(tag, size);
         hash2style_node = push_array(fnt_state->raster_arena, FNT_Hash2StyleRasterCacheNode, 1);
         DLLPushBack_NP(slot->first, slot->last, hash2style_node, hash_next, hash_prev);
         hash2style_node->style_hash = style_hash;
         hash2style_node->ascent = metrics.ascent;
         hash2style_node->descent = metrics.descent;
         hash2style_node->utf8_class1_direct_map = push_array_no_zero(fnt_state->raster_arena, FNT_RasterCacheInfo, 256);
         hash2style_node->hash2info_slots_count = 1024;
         hash2style_node->hash2info_slots = push_array(fnt_state->raster_arena, FNT_Hash2InfoRasterCacheSlot, hash2style_node->hash2info_slots_count);
      }
      ProfEnd();
   }

   return hash2style_node;
}

Vec2F32
Core_FNT_DimFromTagSizeString(FNT_Tag tag, F32 size, F32 base_align_px, F32 tab_size_px, String8 string) {
   ProfBeginFunction();
   Temp    scratch = scratch_begin(0, 0);
   Vec2F32 result = {0};
   FNT_Run run = Core_FNT_RunFromString(tag, size, base_align_px, tab_size_px, 0, string);
   result = run.dim;
   scratch_end(scratch);
   ProfEnd();
   return result;
}

FNT_Run
Core_FNT_RunFromString(FNT_Tag tag, F32 size, F32 base_align_px, F32 tab_size_px, FNT_RasterFlags flags, String8 string) {
   ProfBeginFunction();

   //- rjf: map tag/size to style node
   FNT_Hash2StyleRasterCacheNode* hash2style_node = Core_FNT_Hash2StyleFromTagSizeFlags(tag, size, flags);

   //- rjf: set up this style's run cache if needed
   if (hash2style_node->run_slots_frame_index != fnt_state->frame_index) {
      hash2style_node->run_slots_count = 1024;
      hash2style_node->run_slots = push_array(fnt_state->frame_arena, FNT_RunCacheSlot, hash2style_node->run_slots_count);
      hash2style_node->run_slots_frame_index = fnt_state->frame_index;
   }

   //- rjf: unpack run params
   U64               run_hash = Core_FNT_LittleHashFromString(5381, string);
   U64               run_slot_idx = run_hash % hash2style_node->run_slots_count;
   FNT_RunCacheSlot* run_slot = &hash2style_node->run_slots[run_slot_idx];

   //- rjf: find existing run node for this string
   FNT_RunCacheNode* run_node = 0;
   {
      for (FNT_RunCacheNode* n = run_slot->first; n != 0; n = n->next) {
         if (str8_match(n->string, string, 0)) {
            run_node = n;
            break;
         }
      }
   }

   //- rjf: no run node? -> cache miss - compute & build & fill node if possible
   B32     run_is_cacheable = 1;
   FNT_Run run = {0};
   if (run_node) {
      run = run_node->run;
   } else {
      //- rjf: decode string & produce run pieces
      FNT_PieceChunkList piece_chunks = {0};
      Vec2F32            dim = {0};
      B32                font_handle_mapped_on_miss = 0;
      FP_Handle          font_handle = {0};
      U64                piece_substring_start_idx = 0;
      U64                piece_substring_end_idx = 0;
      for (U64 idx = 0; idx <= string.size;) {
         //- rjf: decode next codepoint & get piece substring, or continuation rule
         U8  byte = (idx < string.size ? string.str[idx] : 0);
         B32 need_another_codepoint = 0;
         if (byte == 0) {
            idx += 1;
         } else
            switch (utf8_class[byte >> 3]) {
            case 1: {
               idx += 1;
               piece_substring_end_idx += 1;
               need_another_codepoint = 0;
            } break;
            default: {
               UnicodeDecode decode = utf8_decode(string.str + idx, string.size - idx);
               idx += decode.inc;
               piece_substring_end_idx += decode.inc;
               need_another_codepoint = 0;
            } break;
            }

         //- rjf: need another codepoint, or have no substring? -> continue
         if (need_another_codepoint || piece_substring_end_idx == piece_substring_start_idx) {
            continue;
         }

         //- rjf: do not need another codepoint? -> grab substring, bump piece start idx
         String8 piece_substring = str8_substr(string, r1u64(piece_substring_start_idx, piece_substring_end_idx));
         piece_substring_start_idx = idx;
         piece_substring_end_idx = idx;

         //- rjf: determine if this piece is a tab - if so, use space info to draw
         B32 is_tab = (piece_substring.size == 1 && piece_substring.str[0] == '\t');
         if (is_tab) {
            run_is_cacheable = 0;
            piece_substring = str8_lit(" ");
         }

         //- rjf: piece substring -> raster cache info
         FNT_RasterCacheInfo* info = 0;
         U64                  piece_hash = 0;
         {
            // rjf: fast path for utf8 class 1 -> direct map
            if (piece_substring.size == 1 &&
                hash2style_node->utf8_class1_direct_map_mask[piece_substring.str[0] / 64] &
                    (1ull << (piece_substring.str[0] % 64))) {
               info = &hash2style_node->utf8_class1_direct_map[piece_substring.str[0]];
            }

            // rjf: more general, slower path for other glyphs
            if (piece_substring.size > 1) {
               piece_hash = Core_FNT_LittleHashFromString(5381, piece_substring);
               U64                           slot_idx = piece_hash % hash2style_node->hash2info_slots_count;
               FNT_Hash2InfoRasterCacheSlot* slot = &hash2style_node->hash2info_slots[slot_idx];
               for (FNT_Hash2InfoRasterCacheNode* node = slot->first; node != 0; node = node->hash_next) {
                  if (node->hash == piece_hash) {
                     info = &node->info;
                     break;
                  }
               }
            }
         }

         //- rjf: no info found -> miss... fill this hash in the cache
         if (info == 0) {
            ProfBegin("no info found -> miss... fill this hash in the cache");
            Temp scratch = scratch_begin(0, 0);

            // rjf: grab font handle for this tag if we don't have one already
            if (font_handle_mapped_on_miss == 0) {
               font_handle_mapped_on_miss = 1;

               // rjf: tag -> font slot index
               U64 font_slot_idx = tag.u64[1] % fnt_state->font_hash_table_size;

               // rjf: tag * slot -> existing node
               FNT_FontHashNode* existing_node = 0;
               {
                  for (FNT_FontHashNode* n = fnt_state->font_hash_table[font_slot_idx].first; n != 0; n = n->hash_next) {
                     if (MemoryMatchStruct(&n->tag, &tag)) {
                        existing_node = n;
                        break;
                     }
                  }
               }

               // rjf: existing node -> font handle
               if (existing_node != 0) {
                  font_handle = existing_node->handle;
               }
            }

            // rjf: call into font provider to rasterize this substring
            FP_RasterResult raster = {0};
            if (size > 0) {
               FP_RasterFlags fp_flags = 0;
               if (flags & FNT_RasterFlag_Smooth) {
                  fp_flags |= FP_RasterFlag_Smooth;
               }
               if (flags & FNT_RasterFlag_Hinted) {
                  fp_flags |= FP_RasterFlag_Hinted;
               }
               raster = Core_FP_Raster(scratch.arena, font_handle, floor_f32(size), fp_flags, piece_substring);
            }

            // rjf: allocate portion of an atlas to upload the rasterization
            S16        chosen_atlas_num = 0;
            FNT_Atlas* chosen_atlas = 0;
            Rng2S16    chosen_atlas_region = {0};
            if (raster.atlas_dim.x != 0 && raster.atlas_dim.y != 0) {
               U64 num_atlases = 0;
               for (FNT_Atlas* atlas = fnt_state->first_atlas;; atlas = atlas->next, num_atlases += 1) {
                  // rjf: create atlas if needed
                  if (atlas == 0 && num_atlases < 64) {
                     atlas = push_array(fnt_state->raster_arena, FNT_Atlas, 1);
                     DLLPushBack(fnt_state->first_atlas, fnt_state->last_atlas, atlas);
                     atlas->root_dim = v2s16(1024, 1024);
                     atlas->root = push_array(fnt_state->raster_arena, FNT_AtlasRegionNode, 1);
                     atlas->root->max_free_size[Corner_00] =
                         atlas->root->max_free_size[Corner_01] =
                             atlas->root->max_free_size[Corner_10] =
                                 atlas->root->max_free_size[Corner_11] = v2s16(atlas->root_dim.x / 2, atlas->root_dim.y / 2);
                     atlas->texture = Core_R_Tex2DAlloc(R_ResourceKind_Dynamic,
                                                   v2s32((S32)atlas->root_dim.x, (S32)atlas->root_dim.y),
                                                   R_Tex2DFormat_RGBA8, 0);
                  }

                  // rjf: allocate from atlas
                  if (atlas != 0) {
                     Vec2S16 needed_dimensions = v2s16(raster.atlas_dim.x + 2, raster.atlas_dim.y + 2);
                     chosen_atlas_region = Core_FNT_AtlasRegionAlloc(fnt_state->raster_arena, atlas, needed_dimensions);
                     if (chosen_atlas_region.x1 != chosen_atlas_region.x0) {
                        chosen_atlas = atlas;
                        chosen_atlas_num = (S32)num_atlases;
                        break;
                     }
                  } else {
                     break;
                  }
               }
            }

            // rjf: upload rasterization to allocated region of atlas texture memory
            if (chosen_atlas != 0) {
               Rng2S32 subregion =
                   {
                       {{{chosen_atlas_region.x0,
                          chosen_atlas_region.y0}},
                        {{chosen_atlas_region.x0 + raster.atlas_dim.x,
                          chosen_atlas_region.y0 + raster.atlas_dim.y}}}};
               Core_R_FillTex2DRegion(chosen_atlas->texture, subregion, raster.atlas);
            }

            // rjf: allocate & fill & push node
            {
               if (piece_substring.size == 1) {
                  info = &hash2style_node->utf8_class1_direct_map[piece_substring.str[0]];
                  hash2style_node->utf8_class1_direct_map_mask[piece_substring.str[0] / 64] |= (1ull << (piece_substring.str[0] % 64));
               } else {
                  U64                           slot_idx = piece_hash % hash2style_node->hash2info_slots_count;
                  FNT_Hash2InfoRasterCacheSlot* slot = &hash2style_node->hash2info_slots[slot_idx];
                  FNT_Hash2InfoRasterCacheNode* node = push_array_no_zero(fnt_state->raster_arena, FNT_Hash2InfoRasterCacheNode, 1);
                  DLLPushBack_NP(slot->first, slot->last, node, hash_next, hash_prev);
                  node->hash = piece_hash;
                  info = &node->info;
               }
               if (info != 0) {
                  info->subrect = chosen_atlas_region;
                  info->atlas_num = chosen_atlas_num;
                  info->raster_dim = raster.atlas_dim;
                  info->advance = raster.advance;
               }
            }

            scratch_end(scratch);
            ProfEnd();
         }

         //- rjf: push piece for this raster portion
         if (info != 0) {
            // rjf: find atlas
            FNT_Atlas* atlas = 0;
            {
               if (info->subrect.x1 != 0 && info->subrect.y1 != 0) {
                  S32 num = 0;
                  for (FNT_Atlas* a = fnt_state->first_atlas; a != 0; a = a->next, num += 1) {
                     if (info->atlas_num == num) {
                        atlas = a;
                        break;
                     }
                  }
               }
            }

            // rjf: on tabs -> expand advance
            F32 advance = info->advance;
            if (is_tab) {
               advance = floor_f32(tab_size_px) - mod_f32(floor_f32(base_align_px), floor_f32(tab_size_px));
            }

            // rjf: push piece
            {
               FNT_Piece* piece = Core_FNT_PieceChunkListPushNew(fnt_state->frame_arena, &piece_chunks, string.size);
               {
                  piece->texture = atlas ? atlas->texture : Core_R_HandleZero();
                  piece->subrect = r2s16p(info->subrect.x0,
                                          info->subrect.y0,
                                          info->subrect.x0 + info->raster_dim.x,
                                          info->subrect.y0 + info->raster_dim.y);
                  piece->advance = advance;
                  piece->decode_size = piece_substring.size;
                  piece->offset = v2s16(0, -(S16)(hash2style_node->ascent + hash2style_node->descent));
               }
               base_align_px += advance;
               dim.x += piece->advance;
               dim.y = Max(dim.y, info->raster_dim.y);
            }
         }
      }

      //- rjf: tighten & fill
      {
         if (piece_chunks.node_count == 1) {
            run.pieces.v = piece_chunks.first->v;
            run.pieces.count = piece_chunks.first->count;
         } else {
            run.pieces = Core_FNT_PieceArrayFromChunkList(fnt_state->frame_arena, &piece_chunks);
         }
         run.dim = dim;
         run.ascent = hash2style_node->ascent;
         run.descent = hash2style_node->descent;
      }
   }

   //- rjf: build node for cacheable runs
   if (run_is_cacheable) {
      run_node = push_array(fnt_state->frame_arena, FNT_RunCacheNode, 1);
      SLLQueuePush(run_slot->first, run_slot->last, run_node);
      run_node->string = push_str8_copy(fnt_state->frame_arena, string);
      run_node->run = run;
   }

   ProfEnd();
   return run;
}

Vec2F32
Core_FNT_DimFromTagSizeStringList(FNT_Tag tag, F32 size, F32 base_align_px, F32 tab_size_px, String8List list) {
   ProfBeginFunction();
   Vec2F32 sum = {0};
   for (String8Node* n = list.first; n != 0; n = n->next) {
      Vec2F32 str_dim = Core_FNT_DimFromTagSizeString(tag, size, base_align_px, tab_size_px, n->string);
      sum.x += str_dim.x;
      sum.y = Max(sum.y, str_dim.y);
   }
   ProfEnd();
   return sum;
}

F32 Core_FNT_ColumnSizeFromTagSize(FNT_Tag tag, F32 size) {
   F32 result = Core_FNT_DimFromTagSizeString(tag, size, 0, 0, str8_lit("H")).x;
   return result;
}


U64 Core_FNT_CharPosFromTagSizeStringP(FNT_Tag tag, F32 size, F32 base_align_px, F32 tab_size_px, String8 string, F32 p) {
   Temp    scratch = scratch_begin(0, 0);
   U64     best_offset_bytes = 0;
   F32     best_offset_px = inf32();
   U64     offset_bytes = 0;
   F32     offset_px = 0.f;
   FNT_Run run = Core_FNT_RunFromString(tag, size, base_align_px, tab_size_px, 0, string);
   for (U64 idx = 0; idx <= run.pieces.count; idx += 1) {
      F32 this_piece_offset_px = abs_f32(offset_px - p);
      if (this_piece_offset_px < best_offset_px) {
         best_offset_bytes = offset_bytes;
         best_offset_px = this_piece_offset_px;
      }
      if (idx < run.pieces.count) {
         FNT_Piece* piece = &run.pieces.v[idx];
         offset_px += piece->advance;
         offset_bytes += piece->decode_size;
      }
   }
   scratch_end(scratch);
   return best_offset_bytes;
}

////////////////////////////////
//~ rjf: Metrics

FNT_Metrics
Core_FNT_MetricsFromTagSize(FNT_Tag tag, F32 size) {
   ProfBeginFunction();
   FP_Metrics  metrics = Core_FNT_FpMetricsFromTag(tag);
   FNT_Metrics result = {0};
   {
      result.ascent = floor_f32(size) * metrics.ascent / metrics.design_units_per_em;
      result.descent = floor_f32(size) * metrics.descent / metrics.design_units_per_em;
      result.line_gap = floor_f32(size) * metrics.line_gap / metrics.design_units_per_em;
      result.capital_height = floor_f32(size) * metrics.capital_height / metrics.design_units_per_em;
   }
   ProfEnd();
   return result;
}

F32 Core_FNT_LineHeightFromMetrics(FNT_Metrics* metrics) {
   return metrics->ascent + metrics->descent + metrics->line_gap;
}

////////////////////////////////
//~ rjf: Main Calls

void Core_FNT_Init(void) {
   Arena* arena = arena_alloc();
   fnt_state = push_array(arena, FNT_State, 1);
   fnt_state->permanent_arena = arena;
   fnt_state->raster_arena = arena_alloc();
   fnt_state->frame_arena = arena_alloc();
   fnt_state->font_hash_table_size = 64;
   fnt_state->font_hash_table = push_array(fnt_state->permanent_arena, FNT_FontHashSlot, fnt_state->font_hash_table_size);
   Core_FNT_Reset();
}

void Core_FNT_Reset(void) {
   for (FNT_Atlas* a = fnt_state->first_atlas; a != 0; a = a->next) {
      Core_R_Tex2DRelease(a->texture);
   }
   fnt_state->first_atlas = fnt_state->last_atlas = 0;
   arena_clear(fnt_state->raster_arena);
   fnt_state->hash2style_slots_count = 1024;
   fnt_state->hash2style_slots = push_array(fnt_state->raster_arena, FNT_Hash2StyleRasterCacheSlot, fnt_state->hash2style_slots_count);
}

void Core_FNT_Frame(void) {
   fnt_state->frame_index += 1;
   arena_clear(fnt_state->frame_arena);
}