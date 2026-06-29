@echo off
setlocal
set "VCVARS=D:\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars32.bat"
cd /d "%~dp0"
if not exist "%VCVARS%" ( echo [ERROR] vcvars32 not found & exit /b 1 )
echo [0/1] init MSVC x86 env ...
call "%VCVARS%" >nul
echo [1/1] cl compile junk_msvc.c ...
cl /nologo /MT junk_msvc.c /Fe:junk_msvc.exe
if errorlevel 1 ( echo [ERROR] cl failed & exit /b 1 )
echo done: junk_msvc.exe
endlocal
