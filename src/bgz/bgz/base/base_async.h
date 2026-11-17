// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

#ifndef BGZ_BASE_ASYNC_H
#define BGZ_BASE_ASYNC_H

////////////////////////////////
//~ bgz: Async Lanes
//
// raddebugger's async thread group (rad/base/base_entry_point.c), run by bgz
// instead. raddebugger ticks each layer by name from its own loop
// (sock_async_tick, http_async_tick, ...), and the app's layers are in
// another binary, so that loop cannot reach them. NEED_ASYNC is 0 in
// src/core/build_config.h so raddebugger's group never starts, and this one
// ticks the same layers raddebugger's would plus the one tick the app hands
// in with AsyncSetAppTick.
//
// AsyncLanesBegin launches (logical cores - 1) threads, overridable with
// -async_thread_count:N, as ONE lane group. The main thread is not in it. Lane
// 0 sleeps until AsyncWake (or 1 s passes), all lanes sync, every lane runs
// the ticks, repeat until AsyncLanesEnd. A tick runs on every lane at once:
// use lane_idx/lane_sync/lane_range inside it, like a raddebugger
// *_async_tick.
//
// If socket/http are built again: they wake raddebugger's loop, not this one,
// so call AsyncWake after starting a request.

typedef void AsyncTickFunctionType(void);

exported void AsyncSetAppTick(AsyncTickFunctionType *tick);
exported void AsyncWake(void);
exported U64  AsyncThreadCount(void);

#endif // BGZ_BASE_ASYNC_H
