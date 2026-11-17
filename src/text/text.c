////////////////////////////////
//~ bgz: text
//
// See text.h.

// Smooth: antialiased, and not snapped to the pixel grid as small interface
// text is - slide text is large.
#define _TXT_RASTER_FLAGS FNT_RasterFlag_Smooth

internal B32
_TXT_FontIsValid(FNT_Tag font) {
   B32 result = !Core_FNT_TagMatch(font, Core_FNT_TagZero());
   return result;
}

// The font cache's size for a size in pixels: points, 72 to an inch where
// pixels are 96, and no less than one - it rasterizes nothing at zero.
internal F32
_TXT_PointsFromPx(F32 px) {
   F32 points = (F32)(S32)(px * 0.75f);
   return Max(points, 1.0f);
}

// The first of the paths that is a file, as a font. The zero tag if none is.
internal FNT_Tag
_TXT_FontFromFirstPath(String8* paths, U64 path_count) {
   FNT_Tag result = Core_FNT_TagZero();

   for (U64 i = 0; i < path_count; i += 1) {
      if (FilePathExists(paths[i])) {
         result = Core_FNT_TagFromPath(paths[i]);
         if (_TXT_FontIsValid(result)) {
            break;
         }
      }
   }

   return result;
}

internal TXT_Fonts
TXT_OpenFonts(void) {
   TXT_Fonts result = {0};
   Temp      scratch = ScratchBegin(0, 0);

   // Where Windows is installed, which is not always C:\Windows.
   char* windows_dir = getenv("WINDIR");
   if (windows_dir == 0) {
      windows_dir = "C:\\Windows";
   }

   String8 regular_paths[] = {
       Str8Lit("data\\fonts\\regular.ttf"),
       PushStr8F(scratch.arena, "%s\\Fonts\\segoeui.ttf", windows_dir),
       PushStr8F(scratch.arena, "%s\\Fonts\\arial.ttf", windows_dir),
   };
   String8 bold_paths[] = {
       Str8Lit("data\\fonts\\bold.ttf"),
       PushStr8F(scratch.arena, "%s\\Fonts\\seguisb.ttf", windows_dir),
       PushStr8F(scratch.arena, "%s\\Fonts\\segoeuib.ttf", windows_dir),
       PushStr8F(scratch.arena, "%s\\Fonts\\arialbd.ttf", windows_dir),
   };

   String8 mono_paths[] = {
       Str8Lit("data\\fonts\\mono.ttf"),
       PushStr8F(scratch.arena, "%s\\Fonts\\consola.ttf", windows_dir),
       PushStr8F(scratch.arena, "%s\\Fonts\\cour.ttf", windows_dir),
   };

   result.regular = _TXT_FontFromFirstPath(regular_paths, ArrayCount(regular_paths));
   result.bold = _TXT_FontFromFirstPath(bold_paths, ArrayCount(bold_paths));

   result.mono = _TXT_FontFromFirstPath(mono_paths, ArrayCount(mono_paths));

   // One font is better than none, for either.
   if (!_TXT_FontIsValid(result.bold)) {
      result.bold = result.regular;
   }
   if (!_TXT_FontIsValid(result.regular)) {
      result.regular = result.bold;
   }
   if (!_TXT_FontIsValid(result.mono)) {
      result.mono = result.regular;
   }
   if (!_TXT_FontIsValid(result.regular)) {
      LogUserErrorF("No font could be opened: text will not be drawn\n");
   }

   ScratchEnd(scratch);
   return result;
}

// The font cache's metrics are in points, though what it rasterizes is 96/72
// of that, in pixels (rad\font_provider\dwrite\font_provider_dwrite.c). A
// glyph's bitmap is ascent + descent + line gap tall, in those pixels.
internal F32
TXT_LineHeight(FNT_Tag font, F32 px) {
   F32 result = 0;

   if (_TXT_FontIsValid(font)) {
      FNT_Metrics metrics = Core_FNT_MetricsFromTagSize(font, _TXT_PointsFromPx(px));
      result = (metrics.ascent + metrics.descent + metrics.line_gap) * (96.0f / 72.0f);
   }

   return result;
}

internal F32
TXT_Width(FNT_Tag font, F32 px, String8 string) {
   F32 result = 0;

   if (_TXT_FontIsValid(font) && string.size != 0) {
      // Measured as it is drawn, with the same flags: the font cache keeps
      // glyphs apart by flags, and would rasterize every one twice.
      FNT_Run run = Core_FNT_RunFromString(font, _TXT_PointsFromPx(px), 0, 0, _TXT_RASTER_FLAGS, string);
      result = run.dim.x;
   }

   return result;
}

// The font cache draws a run into a bitmap round(ascent + descent + line gap)
// + 1 pixels tall, with the baseline round(descent) + round(line gap) above
// its bottom (rad\font_provider\dwrite\font_provider_dwrite.c, fp_raster).
// TXT_Draw puts the top of that bitmap at the position it is given.
internal F32
TXT_Baseline(FNT_Tag font, F32 px) {
   F32 result = 0;

   if (_TXT_FontIsValid(font)) {
      FNT_Metrics metrics = Core_FNT_MetricsFromTagSize(font, _TXT_PointsFromPx(px));
      F32         k = 96.0f / 72.0f;
      F32         bitmap_h = (F32)(S32)(k * (metrics.ascent + metrics.descent + metrics.line_gap) + 0.5f) + 1.0f;
      result = bitmap_h - (F32)(S32)(k * metrics.descent + 0.5f) - (F32)(S32)(k * metrics.line_gap + 0.5f);
   }

   return result;
}

internal F32
TXT_CapHeight(FNT_Tag font, F32 px) {
   F32 result = 0;

   if (_TXT_FontIsValid(font)) {
      FNT_Metrics metrics = Core_FNT_MetricsFromTagSize(font, _TXT_PointsFromPx(px));
      result = metrics.capital_height * (96.0f / 72.0f);
   }

   return result;
}

internal void
TXT_Draw(FNT_Tag font, F32 px, Vec2F32 top_left, Vec4F32 color, String8 string) {
   if (_TXT_FontIsValid(font) && string.size != 0) {
      F32         points = _TXT_PointsFromPx(px);
      FNT_Metrics metrics = Core_FNT_MetricsFromTagSize(font, points);
      FNT_Run     run = Core_FNT_RunFromString(font, points, 0, 0, _TXT_RASTER_FLAGS, string);

      // Core_D_Text puts the top of each glyph's bitmap a whole number of
      // pixels above the position it is given: ascent + descent, cut to a
      // whole number (Core_FNT_RunFromString). The same is added here, to
      // give it the top it was asked for - and on a whole pixel, since a
      // bitmap drawn between pixels is blurred.
      F32 x = (F32)(S32)top_left.x;
      F32 y = (F32)(S32)top_left.y + (F32)(S16)(metrics.ascent + metrics.descent);
      Core_D_Text(V2F32(x, y), color, run);
   }
}

internal String8List
TXT_Wrap(Arena* arena, FNT_Tag font, F32 px, String8 string, F32 max_width) {
   String8List result = {0};

   F32 space_width = TXT_Width(font, px, Str8Lit(" "));

   U64 line_start = 0; // where the line being built begins
   U64 line_end = 0;   // the end of its last word. == line_start: no words yet
   F32 line_width = 0;

   U64 i = 0;
   while (i < string.size) {
      // The next word: everything up to the next space.
      while (i < string.size && string.str[i] == ' ') i += 1;
      U64 word_start = i;
      while (i < string.size && string.str[i] != ' ') i += 1;
      U64 word_end = i;
      if (word_end == word_start) {
         break;
      }

      F32 word_width = TXT_Width(font, px, Str8Substr(string, R1U64(word_start, word_end)));

      if (line_end == line_start) {
         // The first word of a line always goes on it.
         line_start = word_start;
         line_end = word_end;
         line_width = word_width;
      } else {
         // The spaces between this word and the last are measured as they are
         // drawn: all of them.
         F32 gap_width = space_width * (F32)(word_start - line_end);
         if (line_width + gap_width + word_width <= max_width) {
            line_end = word_end;
            line_width += gap_width + word_width;
         } else {
            Str8ListPush(arena, &result, Str8Substr(string, R1U64(line_start, line_end)));
            line_start = word_start;
            line_end = word_end;
            line_width = word_width;
         }
      }
   }

   if (line_end != line_start) {
      Str8ListPush(arena, &result, Str8Substr(string, R1U64(line_start, line_end)));
   }

   return result;
}
