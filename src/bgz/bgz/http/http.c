////////////////////////////////
//~ bgz: HTTP
//
// All of it needs the OS: one file per OS.

#if OS_WINDOWS
# include "../win32/http/win32_http.c" // relative: through -I it would find rad's
#else
# error bgz http not implemented for this OS.
#endif
