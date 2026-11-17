// The Digital Grove Codebase
// Copyright (c) 2024 Ryan Fleury. All rights reserved.

//- rjf: main thread entry point

root_function void
BaseMainThreadEntry(void (*entry)(CmdLine *cmdln), U64 argument_count, char **arguments)
{
#if BUILD_TELEMETRY
 local_persist U8 tm_data[Megabytes(64)];
 tmLoadLibrary(TM_RELEASE);
 tmSetMaxThreadCount(256);
 tmInitialize(sizeof(tm_data), (char *)tm_data);
#endif
 TCTX tctx = TCTXAlloc();
 tctx.is_main_thread = 1;
 SetTCTX(&tctx);
 SetThreadNameF("MainThread");
 String8List args_list = {0};
 for(U64 argument_idx = 1; argument_idx < argument_count; argument_idx += 1)
 {
  Str8ListPush(tctx.scratch_arenas[0], &args_list, Str8C(arguments[argument_idx]));
 }
 CmdLine cmdline = CmdLineFromStringList(tctx.scratch_arenas[0], args_list);
 B32 capture = CmdLineOptB32(&cmdline, Str8Lit("capture"));
 if(capture)
 {
  ProfBeginCapture(arguments[0]);
 }
#if defined(OS_CORE_H)
 OS_Init();
#endif
#if defined(CONTENT_H)
 C_Init();
#endif
#if defined(FILE_STREAM_H)
 FS_Init();
#endif
#if defined(OS_GFX_H)
 OS_InitGfx();
#endif
#if defined(OS_NET_H)
 OS_InitNet();
#endif
#if defined(HTTP_H)
 HTTP_Init();
#endif
#if defined(RENDER_CORE_H)
 R_Init();
#endif
#if defined(TEXTURE_CACHE_H)
 TEX_Init();
#endif
#if defined(FONT_PROVIDER_CORE_H)
 FP_Init();
#endif
#if defined(FONT_CACHE_H)
 F_Init();
#endif
#if defined(DRAW_H)
 D_Init();
#endif
 entry(&cmdline);
 TCTXRelease(&tctx);
 if(capture)
 {
  ProfEndCapture();
 }
}

//- rjf: non-main-thread entry point

root_function void
BaseThreadEntry(void (*entry)(void *p), void *params)
{
 TCTX tctx = TCTXAlloc();
 SetTCTX(&tctx);
#if defined(ANIMATION_CURVE_H)
 AC_InitThread();
#endif
 entry(params);
 TCTXRelease(&tctx);
}
