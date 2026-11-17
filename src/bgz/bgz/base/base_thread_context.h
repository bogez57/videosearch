////////////////////////////////
//~ bgz: Thread Context
//
// bgz's functions for raddebugger's thread context (rad/base/
// base_thread_context.h): scratch arenas, thread names and lanes. Each only
// calls raddebugger's function of the same name.

#ifndef BGZ_BASE_THREAD_CONTEXT_H
#define BGZ_BASE_THREAD_CONTEXT_H

exported void    TCTXSelect(TCTX* tctx);
exported Arena*  TCTXGetScratch(Arena** conflicts, U64 count);
exported String8 TCTXGetThreadName(void);
exported LaneCtx TCTXSetLaneCtx(LaneCtx lane_ctx);
exported void    TCTXLaneBarrierWait(void* broadcast_ptr, U64 broadcast_size, U64 broadcast_src_lane_idx);

// Functions here, macros in raddebugger.
exported Temp    ScratchBegin(Arena** conflicts, U64 count);
exported void    ScratchEnd(Temp temp);
exported U64     LaneIdx(void);
exported U64     LaneCount(void);
exported Rng1U64 LaneRange(U64 count);
exported void    LaneSync(void);

#define LaneSyncU64(ptr, src_lane_idx) TCTXLaneBarrierWait((ptr), sizeof(*(ptr)), (src_lane_idx))

#endif // BGZ_BASE_THREAD_CONTEXT_H
