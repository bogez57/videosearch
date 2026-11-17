// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

////////////////////////////////
//~ bgz: Async Lane State

global U64                    async_lanes_count = 0;
global Thread                *async_lanes_threads = 0;
global Barrier                async_lanes_barrier = {0};
global U64                    async_lanes_broadcast = 0;
global CondVar                async_lanes_start_cv = {0};
global Mutex                  async_lanes_start_mutex = {0};
global U64                    async_lanes_loop_again = 0;
global U64                    async_lanes_exit = 0;
global AsyncTickFunctionType *async_lanes_app_tick = 0;

// Core's own (base_entry_point.c), so declared here rather than in the header
// the app includes.
internal void AsyncLanesBegin(CmdLine *cmdline);
internal void AsyncLanesEnd(void);
internal void AsyncThreadEntryPoint(void *params);

////////////////////////////////
//~ bgz: Async Lanes

internal void
AsyncLanesBegin(CmdLine *cmdline)
{
  async_lanes_start_cv = cond_var_alloc();
  async_lanes_start_mutex = mutex_alloc();
  
  U64 count = get_system_info()->logical_processor_count;
  count -= Min(count, 1); // the main thread
  String8 count_string = cmd_line_string(cmdline, str8_lit("async_thread_count"));
  if(count_string.size != 0)
  {
    try_u64_from_str8_c_rules(count_string, &count);
  }
  count = Max(1, count);
  
  // Lives as long as the threads do, which is longer than any scratch arena.
  Arena *arena = arena_alloc();
  async_lanes_barrier = barrier_alloc(count);
  LaneCtx *lane_ctxs = push_array(arena, LaneCtx, count);
  async_lanes_threads = push_array(arena, Thread, count);
  async_lanes_count = count;
  for EachIndex(idx, count)
  {
    lane_ctxs[idx].lane_idx = idx;
    lane_ctxs[idx].lane_count = count;
    lane_ctxs[idx].barrier = async_lanes_barrier;
    lane_ctxs[idx].broadcast_memory = &async_lanes_broadcast;
    async_lanes_threads[idx] = ThreadLaunch(AsyncThreadEntryPoint, &lane_ctxs[idx]); // @Changed(CrashDebugger): bgz's, for the crash filter's room
  }
}

internal void
AsyncLanesEnd(void)
{
  ins_atomic_u64_eval_assign(&async_lanes_exit, 1);
  AsyncWake();
  for EachIndex(idx, async_lanes_count)
  {
    thread_join(async_lanes_threads[idx], max_U64);
  }
  barrier_release(async_lanes_barrier);
}

internal void
AsyncThreadEntryPoint(void *params)
{
  LaneCtx lctx = *(LaneCtx *)params;
  lane_ctx(lctx);
  SetThreadNameF("lane_%I64u", lane_idx()); // the lane it is for the life of the editor: lane_0, lane_1, ...
  for(;;)
  {
    // Lane 0 sleeps until there is work or a second passes. The flag is
    // checked under the mutex AsyncWake broadcasts under, so a wake can't slip
    // in between the check and the wait - raddebugger checks outside it and
    // lets the 1 s timeout cover that.
    if(lane_idx() == 0)
    {
      MutexScope(async_lanes_start_mutex)
      {
        if(!ins_atomic_u64_eval(&async_lanes_loop_again))
        {
          cond_var_wait(async_lanes_start_cv, async_lanes_start_mutex, now_time_us() + 1000000);
        }
      }
      ins_atomic_u64_eval_assign(&async_lanes_loop_again, 0);
    }
    lane_sync();
    
    // The layers raddebugger's own loop would tick.
#if defined(SOCKET_H)
    sock_async_tick();
#endif
#if defined(HTTP_H)
    http_async_tick();
#endif
    
    // Lane 0 reads the app's tick and hands it to the rest, so every lane runs
    // the same one even if it is set mid-loop. A tick syncs its lanes; lanes
    // disagreeing on whether to call it would wait on each other forever.
    U64 tick = 0;
    if(lane_idx() == 0)
    {
      tick = (U64)async_lanes_app_tick;
    }
    lane_sync_u64(&tick, 0);
    if(tick != 0)
    {
      ((AsyncTickFunctionType *)tick)();
    }
    
    lane_sync();
    U64 need_exit = 0;
    if(lane_idx() == 0)
    {
      need_exit = ins_atomic_u64_eval(&async_lanes_exit);
    }
    lane_sync_u64(&need_exit, 0);
    if(need_exit)
    {
      break;
    }
  }
}

exported void
AsyncSetAppTick(AsyncTickFunctionType *tick)
{
  async_lanes_app_tick = tick;
  AsyncWake();
}

exported void
AsyncWake(void)
{
  ins_atomic_u64_eval_assign(&async_lanes_loop_again, 1);
  MutexScope(async_lanes_start_mutex)
  {
    cond_var_broadcast(async_lanes_start_cv);
  }
}

exported U64
AsyncThreadCount(void)
{
  U64 result = async_lanes_count;
  return result;
}
