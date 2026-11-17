@echo off
setlocal
pushd "%~dp0..\_bin"
clang-cl /nologo /W4 /Zi /arch:AVX /utf-8 /D_CRT_SECURE_NO_WARNINGS -Wno-missing-field-initializers -Wno-unused-function /I.. ..\tests\process_tests.c bgz.lib /Fe:process_tests.exe /link kernel32.lib
if errorlevel 1 (
   popd
   exit /b 1
)
process_tests.exe
set result=%errorlevel%
popd
exit /b %result%
