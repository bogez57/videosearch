////////////////////////////////
//~ rjf: Top-Level API
//
// (Frame boundaries & bucket submission)

exported void Core_D_BeginFrame(void) {
   if (d_thread_ctx == 0) {
      Arena* arena = ArenaAlloc(.reserve_size = GB(1));
      d_thread_ctx = push_array(arena, D_ThreadCtx, 1);
      d_thread_ctx->arena = arena;
      d_thread_ctx->arena_frame_start_pos = arena_pos(arena);
   }

   arena_pop_to(d_thread_ctx->arena, d_thread_ctx->arena_frame_start_pos);
   d_thread_ctx->free_bucket_selection = 0;
   d_thread_ctx->top_bucket = 0;

   // d_thread_ctx->draw_log = LogAlloc();
}

exported void Core_D_SubmitBucket(WM_Window window, R_Handle window_equip, D_Bucket* bucket) {
   Core_R_WindowSubmit(window, window_equip, &bucket->passes);
}

////////////////////////////////
//~ rjf: Bucket Construction & Selection API
//
// (Bucket: Handle to sequence of many render passes, constructed by this layer)

exported D_Bucket* Core_D_BucketMake(void) {
   D_Bucket* bucket = push_array(d_thread_ctx->arena, D_Bucket, 1); // Crashing here? Have you called Core_D_BeginFrame?
   // Core_D_BucketStackInits(bucket);
   return bucket;
}

exported void Core_D_PushBucket(D_Bucket* bucket) {
   D_BucketSelectionNode* node = d_thread_ctx->free_bucket_selection;
   if (node) {
      SLLStackPop(d_thread_ctx->free_bucket_selection);
   } else {
      node = push_array(d_thread_ctx->arena, D_BucketSelectionNode, 1);
   }
   SLLStackPush(d_thread_ctx->top_bucket, node);
   node->bucket = bucket;
}

exported void Core_D_PopBucket(void) {
   D_BucketSelectionNode* node = d_thread_ctx->top_bucket;
   SLLStackPop(d_thread_ctx->top_bucket);
   SLLStackPush(d_thread_ctx->free_bucket_selection, node);
}

exported D_Bucket* Core_D_TopBucket(void) {
   D_Bucket* bucket = 0;
   if (d_thread_ctx->top_bucket != 0) {
      bucket = d_thread_ctx->top_bucket->bucket;
   }
   return bucket;
}

////////////////////////////////
//~ bgz: Clip Stack @Added(clip stack)

exported void Core_D_PushClip(Rng2F32 clip) {
   D_Bucket*   bucket = Core_D_TopBucket();
   D_ClipNode* node = push_array(d_thread_ctx->arena, D_ClipNode, 1);
   node->v = clip;
   SLLStackPush(bucket->top_clip, node);
}

exported void Core_D_PopClip(void) {
   D_Bucket* bucket = Core_D_TopBucket();
   SLLStackPop(bucket->top_clip);
}

exported Rng2F32 Core_D_TopClip(void) {
   D_Bucket* bucket = Core_D_TopBucket();
   Rng2F32   result = {0};
   if (bucket != 0 && bucket->top_clip != 0) {
      result = bucket->top_clip->v;
   }
   return result;
}

////////////////////////////////
//~ rjf: Core Draw Calls
//
// (Apply to the calling thread's currently selected bucket)

//- rjf: rectangles
R_Rect2DInst* Core_D_Rect(Rng2F32 dst, Rng2F32 tex_uv, Vec4F32 color) {
   UnusedVariable(tex_uv);
   Arena*              arena = d_thread_ctx->arena;
   D_Bucket*           bucket = Core_D_TopBucket();
   R_Pass*             pass = Core_R_PassFromKind(arena, &bucket->passes, R_PassKind_UI);
   R_PassParams_UI*    params = pass->params_ui;
   R_BatchGroup2DList* rects = &params->rects;
   R_BatchGroup2DNode* node = rects->last;
   Rng2F32             clip = Core_D_TopClip();
   if (node == 0 || !MemoryMatchStruct(&node->params.clip, &clip)) {
      node = push_array(arena, R_BatchGroup2DNode, 1);
      SLLQueuePush(rects->first, rects->last, node);
      rects->count += 1;
      node->batches = Core_R_BatchListMake(sizeof(R_Rect2DInst));
      node->params.tex = Core_R_HandleZero();
      node->params.clip = clip;
   }

   R_Rect2DInst* rect_inst = (R_Rect2DInst*)Core_R_BatchListPushInst(arena, &node->batches, 256);
   MemoryZeroStruct(rect_inst);
   rect_inst->dst_px = dst;
   rect_inst->src_px = r2f32(v2f32(0, 0), v2f32(0, 0)); // No src texture since this is just a colored rect
   rect_inst->color = color;
   rect_inst->white_texture_override = 1.f;

   return rect_inst;
}

R_Rect2DInst* Core_D_Img(Rng2F32 dst_px, Rng2F32 src_px, R_Handle texture, Vec4F32 color) {
   Arena*              arena = d_thread_ctx->arena;
   D_Bucket*           bucket = Core_D_TopBucket();
   R_Pass*             pass = Core_R_PassFromKind(arena, &bucket->passes, R_PassKind_UI);
   R_PassParams_UI*    params = pass->params_ui;
   R_BatchGroup2DList* rects = &params->rects;
   R_BatchGroup2DNode* node = rects->last;
   Rng2F32             clip = Core_D_TopClip();
   B32                 same_clip = node != 0 && MemoryMatchStruct(&node->params.clip, &clip);

   if (same_clip && Core_R_HandleMatch(node->params.tex, Core_R_HandleZero())) {
      node->params.tex = texture;
   } else if (!same_clip || !Core_R_HandleMatch(texture, node->params.tex)) {
      node = push_array(arena, R_BatchGroup2DNode, 1);
      SLLQueuePush(rects->first, rects->last, node);
      rects->count += 1;
      node->batches = Core_R_BatchListMake(sizeof(R_Rect2DInst));
      node->params.tex = texture;
      node->params.clip = clip;
   }

   R_Rect2DInst* rect_inst = (R_Rect2DInst*)Core_R_BatchListPushInst(arena, &node->batches, 256);
   MemoryZeroStruct(rect_inst);
   rect_inst->dst_px = dst_px;
   rect_inst->src_px = src_px;
   rect_inst->color = color;
   rect_inst->white_texture_override = 0.f;

   return rect_inst;
}

void Core_D_Text(Vec2F32 pos, Vec4F32 color, FNT_Run run) {
   Temp scratch = scratch_begin(0, 0);

   F32        advance = 0;
   FNT_Piece* piece_first = run.pieces.v;
   FNT_Piece* piece_opl = piece_first + run.pieces.count;
   for (FNT_Piece* piece = piece_first; piece < piece_opl; piece += 1) {
      R_Handle texture = piece->texture;

      Rng2F32 src = r2f32p((F32)piece->subrect.x0, (F32)piece->subrect.y0, (F32)piece->subrect.x1, (F32)piece->subrect.y1);
      Vec2F32 size = dim_2f32(src);
      Rng2F32 dst = r2f32p(pos.x + piece->offset.x + advance,
                           pos.y + piece->offset.y,
                           pos.x + piece->offset.x + advance + size.x,
                           pos.y + piece->offset.y + size.y);

      if (size.x != 0 && size.y != 0 && !Core_R_HandleMatch(texture, Core_R_HandleZero())) {
         Core_D_Img(dst, src, texture, color);
      }
      advance += piece->advance;
   }

   scratch_end(scratch);
}

exported void Core_D_Test_Rect(Rng2F32 dst, Rng2F32 tex_uv, Vec4F32 color) {
   Arena*             arena = d_thread_ctx->arena;
   D_Bucket*          bucket = Core_D_TopBucket();
   R_Pass*            pass = Core_R_PassFromKind(arena, &bucket->passes, R_PassKind_Test);
   R_PassParams_Test* params = pass->params_test;

   F32 rect_verts[12] = {
       dst.min.x, dst.max.y,
       dst.max.x, dst.min.y,
       tex_uv.min.x, tex_uv.min.y, // Top-left
       tex_uv.max.x, tex_uv.max.y, // bottom-right
       color.x, color.y, color.z, color.w};

   F32* verts = push_array(arena, F32, 12);
   MemoryCopy(verts, rect_verts, sizeof(rect_verts));

   R_Batch* rects = &pass->params_test->rects;
   rects->byte_count += sizeof(rect_verts);
   if (!rects->v) {
      rects->v = (U8*)verts;
   }

   params->num_rects += 1;
}

exported void Core_D_Test_Img(Rng2F32 dst, Rng2F32 tex_uv, R_Handle texture, Vec4F32 color) {
   Arena*             arena = d_thread_ctx->arena;
   D_Bucket*          bucket = Core_D_TopBucket();
   R_Pass*            pass = Core_R_PassFromKind(arena, &bucket->passes, R_PassKind_Test);
   R_PassParams_Test* params = pass->params_test;
   params->texture = texture;

   F32 rect_verts[12] = {
       dst.min.x, dst.max.y,
       dst.max.x, dst.min.y,
       tex_uv.min.x, tex_uv.min.y, // Top-left
       tex_uv.max.x, tex_uv.max.y, // bottom-right
       color.x, color.y, color.z, color.w};

   F32* verts = push_array(arena, F32, 12);
   MemoryCopy(verts, rect_verts, sizeof(rect_verts));

   R_Batch* rects = &pass->params_test->rects;
   rects->byte_count += sizeof(rect_verts);
   if (!rects->v) {
      rects->v = (U8*)verts;
   }

   params->num_rects += 1;
}

exported void Core_D_Test_Text(FNT_Tag font, F32 size, Vec2F32 pos, String8 string) {
   UnusedVariable(font);
   UnusedVariable(size);
   UnusedVariable(pos);
   UnusedVariable(string);
   Temp scratch = scratch_begin(0, 0);
#if 0
	FC_Run run = FC_RunFromFontSizeString(scratch.arena, font, size, string);

	for(FC_Piece *piece = run.first_piece; piece != 0; piece = piece->next) {
		Vec2F32 piece_size = dim_2f32(piece->src_rect);
		Vec2F32 p0 = add_2f32(pos, piece->offset);
		Vec2F32 p1 = add_2f32(p0, piece_size);
		Rng2F32 rect = r2f32(p0, p1);

		Vec2F32 min_uv = { piece->src_rect.x0 / piece->atlas->root_dim.x,  piece->src_rect.y0 / piece->atlas->root_dim.y};
		Vec2F32 max_uv = { piece->src_rect.x1 / piece->atlas->root_dim.x,  piece->src_rect.y1 / piece->atlas->root_dim.y};
		Rng2F32 tex_uv = { min_uv, max_uv };

		Core_D_Test_Img(rect, tex_uv, piece->texture, (Vec4F32){1, 1, 1, 1});//White text for now

		pos.x += piece->advance;
	}
#endif

   scratch_end(scratch);
}

void Core_D_TruncatedFancyRunList(Vec2F32 p, DR_FRunList* list, F32 max_x, FNT_Run trailer_run) {
   //- rjf: total advance > max? -> enable trailer
   B32 trailer_enabled = (list->dim.x > max_x && trailer_run.dim.x < max_x);

   //- rjf: draw runs
   F32     advance = 0;
   B32     trailer_found = 0;
   Vec4F32 last_color = {0};
   for (DR_FRunNode* n = list->first; n != 0; n = n->next) {
      DR_FRun* fr = &n->v;
      Rng1F32  pixel_range = {0};
      {
         pixel_range.min = 100000;
         pixel_range.max = 0;
      }
      FNT_Piece* piece_first = fr->run.pieces.v;
      FNT_Piece* piece_opl = piece_first + fr->run.pieces.count;
      F32        pre_advance = advance;
      last_color = fr->color;
      for (FNT_Piece* piece = piece_first;
           piece < piece_opl;
           piece += 1) {
         if (trailer_enabled && advance + piece->advance > (max_x - trailer_run.dim.x)) {
            trailer_found = 1;
            break;
         }
         if (!trailer_enabled && advance + piece->advance > max_x) {
            goto end_draw;
         }
         R_Handle texture = piece->texture;
         Rng2F32  src = r2f32p((F32)piece->subrect.x0, (F32)piece->subrect.y0, (F32)piece->subrect.x1, (F32)piece->subrect.y1);
         Vec2F32  size = dim_2f32(src);
         Rng2F32  dst = r2f32p(p.x + piece->offset.x + advance,
                               p.y + piece->offset.y,
                               p.x + piece->offset.x + advance + size.x,
                               p.y + piece->offset.y + size.y);
         if (!Core_R_HandleMatch(texture, Core_R_HandleZero())) {
            Core_D_Img(dst, src, texture, fr->color);
            // dr_rect(dst, v4f32(0, 1, 0, 0.5f), 0, 1.f, 0.f);
         }
         advance += piece->advance;
         pixel_range.min = Min(pre_advance, pixel_range.min);
         pixel_range.max = Max(advance, pixel_range.max);
      }
      if (fr->underline_thickness > 0) {
         Core_D_Rect(r2f32p(p.x + pixel_range.min, p.y + fr->run.descent + fr->run.descent / 8, p.x + pixel_range.max, p.y + fr->run.descent + fr->run.descent / 8 + fr->underline_thickness),
                r2f32(v2f32(0, 0), v2f32(1, 1)),
                fr->color);
      }
      if (fr->strikethrough_thickness > 0) {
         Core_D_Rect(r2f32p(p.x + pre_advance, p.y + fr->run.descent - fr->run.ascent / 2, p.x + advance, p.y + fr->run.descent - fr->run.ascent / 2 + fr->strikethrough_thickness), r2f32(v2f32(0, 0), v2f32(1, 1)), fr->color);
      }
      if (trailer_found) {
         break;
      }
   }
end_draw:;

   //- rjf: draw trailer
   if (trailer_found) {
      FNT_Piece* piece_first = trailer_run.pieces.v;
      FNT_Piece* piece_opl = piece_first + trailer_run.pieces.count;
      Vec4F32    trailer_piece_color = last_color;
      for (FNT_Piece* piece = piece_first;
           piece < piece_opl;
           piece += 1) {
         R_Handle texture = piece->texture;
         Rng2F32  src = r2f32p((F32)piece->subrect.x0, (F32)piece->subrect.y0, (F32)piece->subrect.x1, (F32)piece->subrect.y1);
         Vec2F32  size = dim_2f32(src);
         Rng2F32  dst = r2f32p(p.x + piece->offset.x + advance,
                               p.y + piece->offset.y,
                               p.x + piece->offset.x + advance + size.x,
                               p.y + piece->offset.y + size.y);
         if (!Core_R_HandleMatch(texture, Core_R_HandleZero())) {
            Core_D_Img(dst, src, texture, trailer_piece_color);
            trailer_piece_color.w *= 0.5f;
         }
         advance += piece->advance;
      }
   }
}

void Core_D_TextRun(Vec2F32 p, Vec4F32 color, FNT_Run run) {
   F32        advance = 0;
   FNT_Piece* piece_first = run.pieces.v;
   FNT_Piece* piece_opl = piece_first + run.pieces.count;
   for (FNT_Piece* piece = piece_first;
        piece < piece_opl;
        piece += 1) {
      R_Handle texture = piece->texture;
      Rng2F32  src = r2f32p((F32)piece->subrect.x0, (F32)piece->subrect.y0, (F32)piece->subrect.x1, (F32)piece->subrect.y1);
      Vec2F32  size = dim_2f32(src);
      Rng2F32  dst = r2f32p(p.x + piece->offset.x + advance,
                            p.y + piece->offset.y,
                            p.x + piece->offset.x + advance + size.x,
                            p.y + piece->offset.y + size.y);
      if (size.x != 0 && size.y != 0 && !Core_R_HandleMatch(texture, Core_R_HandleZero())) {
         Core_D_Img(dst, src, texture, color);
      }
      advance += piece->advance;
   }
}

DR_FStrList Core_D_FStrs_Copy(Arena* arena, DR_FStrList* src) {
   DR_FStrList dst = {0};
   for (DR_FStrNode* src_n = src->first; src_n != 0; src_n = src_n->next) {
      DR_FStr fstr = src_n->v;
      fstr.string = push_str8_copy(arena, fstr.string);
      Core_D_FStrs_Push(arena, &dst, &fstr);
   }
   return dst;
}

void Core_D_FStrs_PushFront(Arena* arena, DR_FStrList* list, DR_FStr* str) {
   DR_FStrNode* n = push_array_no_zero(arena, DR_FStrNode, 1);
   MemoryCopyStruct(&n->v, str);
   SLLQueuePushFront(list->first, list->last, n);
   list->node_count += 1;
   list->total_size += str->string.size;
}
void Core_D_FStrs_Push(Arena* arena, DR_FStrList* list, DR_FStr* str) {
   DR_FStrNode* n = push_array_no_zero(arena, DR_FStrNode, 1);
   MemoryCopyStruct(&n->v, str);
   SLLQueuePush(list->first, list->last, n);
   list->node_count += 1;
   list->total_size += str->string.size;
}

void Core_D_FStrs_PushNew(Arena* arena, DR_FStrList* list, DR_FStrParams* params, DR_FStrParams* overrides, String8 string) {
   DR_FStr fstr = {string, *params};
   if (!Core_FNT_TagMatch(Core_FNT_TagZero(), overrides->font)) {
      fstr.params.font = overrides->font;
   }
   if (overrides->raster_flags != 0) {
      fstr.params.raster_flags = overrides->raster_flags;
   }
   if (overrides->color.x != 0 || overrides->color.y != 0 || overrides->color.z != 0 || overrides->color.w != 0) {
      fstr.params.color = overrides->color;
   }
   if (overrides->size != 0) {
      fstr.params.size = overrides->size;
   }
   if (overrides->underline_thickness != 0) {
      fstr.params.underline_thickness = overrides->underline_thickness;
   }
   if (overrides->strikethrough_thickness != 0) {
      fstr.params.strikethrough_thickness = overrides->strikethrough_thickness;
   }
   Core_D_FStrs_Push(arena, list, &fstr);
}

String8 Core_D_FStrs_String(Arena* arena, DR_FStrList* list) {
   String8 result = {0};
   {
      Temp        scratch = scratch_begin(&arena, 1);
      String8List parts = {0};
      for (DR_FStrNode* n = list->first; n != 0; n = n->next) {
         str8_list_push(scratch.arena, &parts, n->v.string);
      }
      result = str8_list_join(arena, &parts, 0);
      result = str8_skip_chop_whitespace(result);
      scratch_end(scratch);
   }
   return result;
}

DR_FRunList Core_D_FStrs_Fruns(Arena* arena, F32 tab_size_px, DR_FStrList* strs) {
   DR_FRunList run_list = {0};
   F32         base_align_px = 0;
   for (DR_FStrNode* n = strs->first; n != 0; n = n->next) {
      DR_FRunNode* dst_n = push_array(arena, DR_FRunNode, 1);
      dst_n->v.run = Core_FNT_RunFromString(n->v.params.font, n->v.params.size, base_align_px, tab_size_px, n->v.params.raster_flags, n->v.string);
      dst_n->v.color = n->v.params.color;
      dst_n->v.underline_thickness = n->v.params.underline_thickness;
      dst_n->v.strikethrough_thickness = n->v.params.strikethrough_thickness;
      SLLQueuePush(run_list.first, run_list.last, dst_n);
      run_list.node_count += 1;
      run_list.dim.x += dst_n->v.run.dim.x;
      run_list.dim.y = Max(run_list.dim.y, dst_n->v.run.dim.y);
      base_align_px += dst_n->v.run.dim.x;
   }
   return run_list;
}
