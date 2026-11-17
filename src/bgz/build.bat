@echo off
setlocal

:: bgz: raddebugger's layers (rad\) and bgz's on top (bgz\), built into ONE
:: dll that a program links against: bgz.dll, with bgz.lib to link with and
:: the headers in this folder to compile with. build.c is the whole of it as
:: one unit: raddebugger's functions are static, so everything that calls them
:: is compiled together (see build.c).
::
:: Nothing outside this folder is read or written: it all goes into _bin
:: here. The program's own build calls this one, then copies bgz.dll, bgz.lib
:: and bgz.pdb out of _bin to wherever its exe goes.
::
::   build.bat          clang-cl
::   build.bat o2       optimized
::   build.bat meta     rebuild metagen.exe, not only run it
::   build.bat no_meta  do not run metagen at all
::
:: Works from any folder: everything below is relative to this one.
cd /d "%~dp0"

:: --- unpack args -----------------------------------
set "o2=0"
set "meta=0"
set "no_meta=0"

for %%a in (%*) do (
    if /I "%%~a"=="o2" set "o2=1"
    if /I "%%~a"=="meta" set "meta=1"
    if /I "%%~a"=="no_meta" set "no_meta=1"
)

where /Q cl.exe
if errorlevel 1 (
  echo.
  echo [91m[ERROR] MSVC environment not initialized[0m
  echo [93m        cl.exe not found on PATH[0m
  echo [93m        Please run your MSVC setup script first[0m
  exit /b 1
)

::--- (begin) flags ----------------------------------------
::   compile    debug info, warnings, the optimization level
::   link_dll   links the object into a dll
::   libs       the Windows libraries bgz needs
::
:: No -Wno- flags: bgz is held to every warning the compile line turns on.
:: raddebugger's code is held to its own warnings, not ours, by the pragma
:: around it in build.c.
set "bin=_bin"

set "opt=/Od"
if "%o2%"=="1" set "opt=/O2"

set "compile=clang-cl /nologo /Z7 %opt% /W4 /arch:AVX /clang:-mcx16 /diagnostics:caret /clang:-Wincompatible-pointer-types /clang:-Werror=incompatible-pointer-types"
set "link_dll=link /nologo /DLL /DEBUG:FULL /INCREMENTAL:NO"
set "libs=kernel32.lib user32.lib UxTheme.lib dwmapi.lib winmm.lib ole32.lib dxgi.lib dxguid.lib advapi32.lib winhttp.lib"

:: rad first, because raddebugger's files include each other from there
:: ("base/...", "win32/...", "third_party/..."); then bgz, the same for bgz's
:: ("third_party/spall.h", "third_party/sokol/...").
set "includes=-Irad -Ibgz"

:: The defines raddebugger's code expects (raddebugger's build.bat,
:: clang_common). -mcx16 (cmpxchg16b, for its 128-bit atomics) is in compile,
:: since clang-cl takes it as /clang:-mcx16.
set "defines=-D_USE_MATH_DEFINES -Dstrdup=_strdup -Dgnu_printf=printf"
::--- (end) flags ------------------------------------------

if not exist %bin% mkdir %bin%

::--- (begin) metadesk ---------------------------------------
:: Every .mdesk under bgz becomes generated\<folder>.meta.h/.c beside it. rad
:: is not scanned: raddebugger's generated files come with the copy.
:: metagen.exe is built with cl when missing or "meta" is passed.
if "%no_meta%"=="1" goto metagen_done
set "metagen=%bin%\metagen.exe"
if not exist %metagen% set "meta=1"
if not "%meta%"=="1" goto run_metagen
echo [---building metagen]
cl /nologo /Zi bgz\metagen\metagen_main.c -Ibgz /Fo%bin%\metagen.obj /Fd%bin%\metagen_vc.pdb /Fe%metagen% /link /INCREMENTAL:NO /MANIFEST:EMBED
if errorlevel 1 exit /b 1
:run_metagen
echo [---running metagen over bgz]
%metagen% --scan:bgz
if errorlevel 1 exit /b 1
:metagen_done
::--- (end) metadesk -----------------------------------------

::--- (begin) bgz dll --------------------------------------
echo [---building bgz unit]
%compile% %defines% %includes% build.c /c /Fo%bin%\bgz.obj
if errorlevel 1 exit /b 1

echo [---linking bgz dll]
:: Hides the linker's "Creating library ..." line, which has no switch.
%link_dll% %bin%\bgz.obj %libs% /OUT:%bin%\bgz.dll /IMPLIB:%bin%\bgz.lib /PDB:%bin%\bgz.pdb >%bin%\bgz_link.txt 2>&1
set "link_result=%errorlevel%"
findstr /V /C:"Creating library" %bin%\bgz_link.txt
if not "%link_result%"=="0" exit /b 1
::--- (end) bgz dll ----------------------------------------

exit /b 0
