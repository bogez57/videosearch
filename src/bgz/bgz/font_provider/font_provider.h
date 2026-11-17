////////////////////////////////
//~ bgz: Font Provider
//
// bgz's functions for raddebugger's font provider (rad/font_provider/
// font_provider.h): each only calls raddebugger's function of the same name.
// Its types (FP_Handle, FP_Metrics, ...) are used as they are.

#ifndef BGZ_FONT_PROVIDER_H
#define BGZ_FONT_PROVIDER_H

exported void            Core_FP_Init(void);

exported FP_Handle       Core_FP_HandleZero(void);
exported B32             Core_FP_HandleMatch(FP_Handle a, FP_Handle b);

exported FP_Handle       Core_FP_FontOpen(String8 path);
exported FP_Handle       Core_FP_FontOpenFromStaticDataString(String8* data_ptr);
exported void            Core_FP_FontClose(FP_Handle handle);
exported FP_Metrics      Core_FP_MetricsFromFont(FP_Handle font);
exported FP_RasterResult Core_FP_Raster(Arena* arena, FP_Handle font, F32 size, FP_RasterFlags flags, String8 string);

#endif // BGZ_FONT_PROVIDER_H
