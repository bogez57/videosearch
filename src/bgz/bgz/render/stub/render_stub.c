// Copyright (c) Epic Games Tools
// Licensed under the MIT license (https://opensource.org/license/mit/)

//- rjf: top-level layer initialization

void R_Init(CmdLine *cmdln) {
}

R_Handle R_WindowEquip(WM_Window window) {
	R_Handle handle = {0};
	handle.u64[0] = 1;
	return handle;
}

void R_WindowUnEuqip(WM_Window window, R_Handle window_equip) {
}

R_Handle R_Tex2DAlloc(R_ResourceKind kind, Vec2S32 size, R_Tex2DFormat format, void *data) {
	R_Handle handle = {0};
	handle.u64[0] = 1;
	return handle;
}

void R_Tex2DRelease(R_Handle texture) {
}

R_ResourceKind R_KindFromTex2D(R_Handle texture) {
	return R_ResourceKind_Static;
}

Vec2S32 R_SizeFromTex2D(R_Handle texture) {
	return v2s32(1, 1);
}

R_Tex2DFormat R_FormatFromTex2D(R_Handle texture) {
	return R_Tex2DFormat_RGBA8;
}

void R_FillTex2DRegion(R_Handle texture, Rng2S32 subrect, void *data) {
}

R_Handle R_BufferAlloc(R_ResourceKind kind, U64 size, void *data) {
	R_Handle handle = {0};
	handle.u64[0] = 1;
	return handle;
}

void R_BufferRelease(R_Handle buffer) {
}

void R_BeginFrame(void) {
}

void R_EndFrame(void) {
}

void R_WindowBeginFrame(WM_Window window, R_Handle window_equip) {
}

void R_WindowEndFrame(WM_Window window, R_Handle window_equip) {
}

void R_WindowSubmit(WM_Window window, R_Handle window_equip, R_PassList *passes) {
}

void R_Shutdown(void) {
}
