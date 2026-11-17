# raddebugger's layers

Everything in this folder (bar this file and `LICENSE`) is an exact copy of
raddebugger's `src/`, from https://github.com/EpicGamesExt/raddebugger, branch
`master`, commit `b6d8c3fd9eaf7b55960d80738365742a8fba9e29` (2026-10-07). Nothing in it
is edited, and it has the same layout as raddebugger's `src/`, because
raddebugger's files include each other by paths from there
(`"win32/base/win32_base.h"`, `"third_party/stb/stb_sprintf.h"`) - the builds
put this folder first on the include path for that.

What is here:

- `base/`, `window_manager/`, `shell/`, `socket/`, `http/`, `lib_raddbg_markup/`
- `font_provider/` - with its `dwrite/` backend (built) and `freetype/` (not
  built). Added later from the same commit.
- `win32/` - the win32 halves of base, window_manager, shell, socket and http
- `socket/` and `http/` (and their win32 halves) are copied but not built:
  nothing uses them. See `src/core/bgz/build.c`.
- `linux/` - the linux halves of base, window_manager, shell and socket (not built)
- `third_party/` - `martins_hash/`, `martins_memfun/`, `radsort/`, `xxHash/`,
  `stb/stb_sprintf.h`, `stb/stb_image.h`

bgz's own code is in `src/core/bgz/bgz/`, beside this folder. What bgz adds
to raddebugger's base is in `bgz/base/` and `bgz/win32/base/`.

## License

raddebugger is MIT licensed. `LICENSE` here is raddebugger's own, copied from
the root of its repository (outside `src/`, so not part of the copy above).
Every raddebugger file also carries the two-line header "Copyright (c) Epic
Games Tools / Licensed under the MIT license" - keep it, including in bgz's
forks of raddebugger files.

`third_party/` has its own licenses: xxHash's `LICENSE` (BSD-2-Clause) is in
its folder, and `stb_image.h` / `stb_sprintf.h` state theirs at the end of each
file (public domain or MIT).

`update_rad.bat` stops if a newer commit's `LICENSE` differs from this one. A
license change would apply to raddebugger's later versions, not to this copy.

## Updating

With a clone of raddebugger, from the repo root:

1. `update_rad <clone>` shows what changed in what is copied here.
2. `update_rad <clone> apply` replaces it and records the commit in this file.
   A folder or file added to what is copied goes in the list in `update_rad.bat`.
3. Build core, then `build tests` and `bin\editor_tests.exe`. Errors come from
   three places:
   - bgz's base and window manager (`bgz/base/`, `bgz/window_manager/`): a
     raddebugger function bgz's PascalCase one calls changed.
   - bgz's other layers (`render/`, `font_*`, `draw/`, ...) and the
     foundation (`text_buffer/`, `parser/`, `ir/`): they call raddebugger's
     names directly.
   - zig only: `src/core/zig_compat.h`, for what MinGW's headers lack.

To check the copy is still exact: `update_rad <clone> check`.

## Not touched by metagen

The core build runs bgz's metagen over `src/core` and over `src/core/bgz`,
and both runs skip any folder named `rad` - this one: raddebugger's `.mdesk`
files and their `generated/` output are left as copied.
