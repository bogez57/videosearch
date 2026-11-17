////////////////////////////////
//~ bgz: Font Provider

exported void
Core_FP_Init(void) {
   fp_init();
}

exported FP_Handle
Core_FP_HandleZero(void) {
   FP_Handle result = fp_handle_zero();
   return result;
}

exported B32
Core_FP_HandleMatch(FP_Handle a, FP_Handle b) {
   B32 result = fp_handle_match(a, b);
   return result;
}

exported FP_Handle
Core_FP_FontOpen(String8 path) {
   FP_Handle result = fp_font_open(path);
   return result;
}

exported FP_Handle
Core_FP_FontOpenFromStaticDataString(String8* data_ptr) {
   FP_Handle result = fp_font_open_from_static_data_string(data_ptr);
   return result;
}

exported void
Core_FP_FontClose(FP_Handle handle) {
   fp_font_close(handle);
}

exported FP_Metrics
Core_FP_MetricsFromFont(FP_Handle font) {
   FP_Metrics result = fp_metrics_from_font(font);
   return result;
}

exported FP_RasterResult
Core_FP_Raster(Arena* arena, FP_Handle font, F32 size, FP_RasterFlags flags, String8 string) {
   FP_RasterResult result = fp_raster(arena, font, size, flags, string);
   return result;
}
