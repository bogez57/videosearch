////////////////////////////////
//~ bgz: text
//
// Drawing and measuring text, on top of bgz's font cache and draw layers.
//
// Sizes here are in pixels: the height of the font's em square on screen.
// bgz's font cache takes points, and whole ones, so a size is rounded down to
// the nearest three quarters of a pixel's worth - TXT_Draw and the measuring
// functions all round the same way, so they agree with each other.
//
// Positions are the top left corner of the line: the top of the box that is
// TXT_LineHeight tall and holds everything from the tallest accent to the
// lowest descender.
//
// A font that could not be opened is the zero tag. Text in it measures as
// nothing and draws nothing.

#ifndef TEXT_H
#define TEXT_H

typedef struct TXT_Fonts TXT_Fonts;
struct TXT_Fonts {
   FNT_Tag regular;
   FNT_Tag bold;
   FNT_Tag mono; // every character the same width: the editor's
};

// data\fonts\regular.ttf, bold.ttf and mono.ttf if the program has them, else
// Segoe UI and Consolas from Windows' own fonts, else Arial and Courier New.
internal TXT_Fonts TXT_OpenFonts(void);

internal F32 TXT_LineHeight(FNT_Tag font, F32 px);
internal F32 TXT_Width(FNT_Tag font, F32 px, String8 string);
// How far below the position TXT_Draw is given the letters stand: their
// baseline, in whole pixels, as the font cache rasterizes them. Two fonts or
// sizes on one line line up when their positions differ by the difference of
// this.
internal F32 TXT_Baseline(FNT_Tag font, F32 px);
// The height of a capital letter above the baseline.
internal F32 TXT_CapHeight(FNT_Tag font, F32 px);
internal void TXT_Draw(FNT_Tag font, F32 px, Vec2F32 top_left, Vec4F32 color, String8 string);

// The string cut into lines no wider than max_width, broken at spaces. Each
// line is a piece of the string itself, not a copy. A single word wider than
// max_width gets a line to itself and sticks out. An empty string is no lines.
internal String8List TXT_Wrap(Arena* arena, FNT_Tag font, F32 px, String8 string, F32 max_width);

#endif // TEXT_H
