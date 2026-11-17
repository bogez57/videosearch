////////////////////////////////
//~ bgz: Window Manager, Windows
//
// bgz's own window manager functions that need Windows. The rest of
// bgz/window_manager/window_manager.c calls raddebugger's, which has its own
// per-OS files.

exported B32
Core_WM_RectIsOnAMonitor(Rng2F32 rect) {
   RECT r = {(LONG)rect.x0, (LONG)rect.y0, (LONG)rect.x1, (LONG)rect.y1};
   B32  result = MonitorFromRect(&r, MONITOR_DEFAULTTONULL) != 0;
   return result;
}
