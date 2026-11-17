@echo off
setlocal

:: The program: the video search, built on bgz. Run from the repo root.
::
:: bgz has its own build, src\bgz\build.bat, which makes bgz.dll, bgz.lib and
:: bgz.pdb in src\bgz\_bin. This one calls it first, on every build, copies
:: those three into bin, then builds src\main.c - the program's one unit - and
:: links it against bgz.lib.
::
::   build            bgz, then the program
::   build no_bgz     the program only, against the bgz already in bin
::   build o2         optimized
::   build meta       rebuild metagen.exe, not only run it
::   build no_meta    do not run metagen
::
:: o2, meta and no_meta are handed on to bgz's build.

:: --- unpack args -----------------------------------
set "no_bgz=0"
set "o2=0"

for %%a in (%*) do (
    if /I "%%~a"=="no_bgz" set "no_bgz=1"
    if /I "%%~a"=="o2" set "o2=1"
)

set "bin=bin"
if not exist %bin% mkdir %bin%

::--- (begin) bgz ------------------------------------------
:: bgz's build checks for the compiler itself, so this script does not.
:: /Y overwrites what is already in bin. A running videosearch has bgz.dll
:: loaded, and then the copy fails: close it and build again.
if "%no_bgz%"=="1" goto bgz_done
call src\bgz\build.bat %*
if errorlevel 1 exit /b 1
copy /Y src\bgz\_bin\bgz.dll %bin%\bgz.dll >NUL || echo Failed to copy bgz.dll
copy /Y src\bgz\_bin\bgz.lib %bin%\bgz.lib >NUL || echo Failed to copy bgz.lib
copy /Y src\bgz\_bin\bgz.pdb %bin%\bgz.pdb >NUL || echo Failed to copy bgz.pdb
:bgz_done
if not exist %bin%\bgz.lib (
	echo [ERROR] bin\bgz.lib not found - build without no_bgz once to make it
	exit /b 1
)
::--- (end) bgz --------------------------------------------

::--- (begin) flags ----------------------------------------
set "opt=/Od"
if "%o2%"=="1" set "opt=/O2"

set "compile=clang-cl /nologo /Z7 %opt% /W4 /arch:AVX /diagnostics:caret /clang:-Wincompatible-pointer-types /clang:-Werror=incompatible-pointer-types"

:: src\bgz\rad first, because raddebugger's headers include each other from
:: there ("base/...", "win32/..."); then src\bgz\bgz, the same for bgz's;
:: src\bgz so that main.c's "rad/..." and "bgz/..." are found; src for the
:: program's own layers.
set "includes=-Isrc\bgz\rad -Isrc\bgz\bgz -Isrc\bgz -Isrc"
:: The defines raddebugger's headers expect - see src\bgz\build.bat.
set "defines=-D_USE_MATH_DEFINES -Dstrdup=_strdup -Dgnu_printf=printf"
:: The one warning turned off: an initializer that leaves out trailing fields,
:: which C zeroes.
set "warnings=-Wno-missing-field-initializers"
::--- (end) flags ------------------------------------------

::--- (begin) videosearch ----------------------------------
echo [---building videosearch]
%compile% %warnings% %defines% %includes% src\main.c %bin%\bgz.lib /Fo%bin%\ /Fe%bin%\videosearch.exe /link /DEBUG:FULL /INCREMENTAL:NO /PDB:%bin%\videosearch.pdb
if errorlevel 1 exit /b 1
::--- (end) videosearch ------------------------------------

exit /b 0
