////////////////////////////////
//~ bgz: Thread Context

exported void
TCTXSelect(TCTX* tctx) {
   tctx_select(tctx);
}

exported Arena*
TCTXGetScratch(Arena** conflicts, U64 count) {
   Arena* result = tctx_get_scratch(conflicts, count);
   return result;
}

exported String8
TCTXGetThreadName(void) {
   String8 result = tctx_get_thread_name();
   return result;
}

exported LaneCtx
TCTXSetLaneCtx(LaneCtx lane_ctx) {
   LaneCtx result = tctx_set_lane_ctx(lane_ctx);
   return result;
}

exported void
TCTXLaneBarrierWait(void* broadcast_ptr, U64 broadcast_size, U64 broadcast_src_lane_idx) {
   tctx_lane_barrier_wait(broadcast_ptr, broadcast_size, broadcast_src_lane_idx);
}

////////////////////////////////
//~ bgz: Scratch Arenas

exported Temp
ScratchBegin(Arena** conflicts, U64 count) {
   Temp result = scratch_begin(conflicts, count);
   return result;
}

exported void
ScratchEnd(Temp temp) {
   scratch_end(temp);
}

////////////////////////////////
//~ bgz: Lanes

exported U64
LaneIdx(void) {
   U64 result = lane_idx();
   return result;
}

exported U64
LaneCount(void) {
   U64 result = lane_count();
   return result;
}

exported Rng1U64
LaneRange(U64 count) {
   Rng1U64 result = lane_range(count);
   return result;
}

exported void
LaneSync(void) {
   lane_sync();
}
