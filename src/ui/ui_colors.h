#ifndef UI_COLORS_H
#define UI_COLORS_H

////////////////////////////////
//~ bgz: base colours
//
// What a colour looks like. The theme says what each is for (Themes in
// ui_core.h); use a base colour directly only for a one-off. Braces, two
// deep for Vec4F32's first member, so the theme's table can use them; a call site casts:
// UI_SetNextBackgroundColor((Vec4F32)UI_NAVY).

#define UI_WHITE       {{1.00f, 1.00f, 1.00f, 1.00f}}
#define UI_GREY_10     {{0.10f, 0.10f, 0.11f, 1.00f}}
#define UI_GREY_12     {{0.12f, 0.12f, 0.13f, 1.00f}}
#define UI_GREY_15     {{0.15f, 0.15f, 0.16f, 1.00f}}
#define UI_GREY_55     {{0.55f, 0.55f, 0.55f, 1.00f}}
#define UI_GREY_85     {{0.85f, 0.85f, 0.85f, 1.00f}}
#define UI_SLATE       {{0.20f, 0.22f, 0.27f, 1.00f}}
#define UI_NAVY        {{0.10f, 0.22f, 0.42f, 1.00f}}
#define UI_BLUE        {{0.22f, 0.32f, 0.55f, 1.00f}}
#define UI_SKY         {{0.45f, 0.62f, 0.85f, 1.00f}}
#define UI_LAVENDER    {{0.60f, 0.65f, 1.00f, 1.00f}}
#define UI_GREEN       {{0.45f, 0.85f, 0.55f, 1.00f}}
#define UI_ORANGE      {{1.00f, 0.72f, 0.35f, 1.00f}}
#define UI_AMBER       {{1.00f, 0.82f, 0.35f, 1.00f}}
#define UI_GOLD        {{1.00f, 0.85f, 0.45f, 1.00f}}
#define UI_RED         {{1.00f, 0.45f, 0.40f, 1.00f}}
#define UI_DARK_RED    {{0.77f, 0.17f, 0.17f, 1.00f}}
#define UI_BLACK       {{0.00f, 0.00f, 0.00f, 1.00f}}
#define UI_TRANSPARENT {{0.00f, 0.00f, 0.00f, 0.00f}}

////////////////////////////////
//~ bgz: the editor's palette
//
// The few colours the theme table (frame.c) is built from. Backgrounds go
// from the window's edges in (0) to what floats over it (4); text from full
// to faint. Change one here and everything that uses it follows.

#define UI_BG_0        {{0.082f, 0.086f, 0.094f, 1.00f}} // title bar, status bar, the strip behind tabs
#define UI_BG_1        {{0.106f, 0.110f, 0.122f, 1.00f}} // side views: file tree, console, lists
#define UI_BG_2        {{0.122f, 0.125f, 0.137f, 1.00f}} // the code, and the tab of the panel showing it
#define UI_BG_3        {{0.153f, 0.161f, 0.176f, 1.00f}} // what floats: palette, menus, auto complete, the alert
#define UI_BG_4        {{0.200f, 0.212f, 0.235f, 1.00f}} // hover, buttons, tags
#define UI_LINE        {{0.180f, 0.188f, 0.208f, 1.00f}} // borders and dividers
#define UI_FG          {{0.835f, 0.835f, 0.845f, 1.00f}} // text
#define UI_FG_SOFT     {{0.725f, 0.733f, 0.757f, 1.00f}} // body text in lists
#define UI_FG_DIM      {{0.525f, 0.541f, 0.573f, 1.00f}} // hints, unfocused tabs, line numbers
#define UI_FG_WEAK     {{0.373f, 0.388f, 0.420f, 1.00f}} // greyed out
#define UI_ACCENT      {{0.310f, 0.560f, 0.880f, 1.00f}} // focus, the active tab's line
#define UI_ACCENT_BG   {{0.141f, 0.212f, 0.310f, 1.00f}} // the picked row of a list, selected text
#define UI_ERROR       {{0.940f, 0.392f, 0.353f, 1.00f}}
#define UI_WARNING     {{0.910f, 0.714f, 0.298f, 1.00f}}
#define UI_SUCCESS     {{0.486f, 0.812f, 0.541f, 1.00f}}
#define UI_HIGHLIGHT   {{0.898f, 0.753f, 0.482f, 1.00f}} // the active file, palette sections

#endif // UI_COLORS_H