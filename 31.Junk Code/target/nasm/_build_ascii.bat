@echo off
setlocal
set "NASM=G:\NASM\nasm.exe"
set "VCVARS=D:\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars32.bat"
cd /d "%~dp0"

if not exist "%NASM%" ( echo [ERROR] nasm not found: %NASM% & exit /b 1 )
if not exist "%VCVARS%" ( echo [ERROR] vcvars32 not found: %VCVARS% & exit /b 1 )

echo [0/3] init MSVC x86 env ...
call "%VCVARS%" >nul
if errorlevel 1 ( echo [ERROR] vcvars32 failed & exit /b 1 )

echo [1/3] nasm assemble junk_all.asm ...
"%NASM%" -f win32 junk_all.asm -o junk_all.obj
if errorlevel 1 ( echo [ERROR] nasm failed & exit /b 1 )

echo [2/3] cl compile+link driver.c + junk_all.obj ...
cl /nologo /MT driver.c junk_all.obj /Fe:junk_all.exe
if errorlevel 1 ( echo [ERROR] cl failed & exit /b 1 )

echo [3/3] done: junk_all.exe
endlocal
