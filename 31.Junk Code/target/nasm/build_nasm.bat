@echo off
REM ===========================================================================
REM build_nasm.bat —— 编译 NASM 版花指令实验靶子（本机绝对路径版，已验证）
REM 本机环境：
REM   NASM : G:\NASM\nasm.exe (2.14)
REM   cl   : MSVC 14.44 x86，经 vcvars32.bat 建立 32 位编译环境
REM 用法：直接双击或在 cmd 里运行本 bat（无需手动进 Native Tools 提示符）。
REM ===========================================================================
setlocal

set "NASM=G:\NASM\nasm.exe"
set "VCVARS=D:\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars32.bat"

if not exist "%NASM%" ( echo [ERROR] 未找到 nasm: %NASM% & exit /b 1 )
if not exist "%VCVARS%" ( echo [ERROR] 未找到 vcvars32: %VCVARS% & exit /b 1 )

echo [0/3] 建立 MSVC x86 编译环境 ...
call "%VCVARS%" >nul
if errorlevel 1 ( echo [ERROR] vcvars32 失败 & exit /b 1 )

echo [1/3] 汇编 junk_all.asm  ^(NASM, win32^) ...
"%NASM%" -f win32 junk_all.asm -o junk_all.obj
if errorlevel 1 ( echo [ERROR] nasm 失败 & exit /b 1 )

echo [2/3] 编译并链接 driver.c + junk_all.obj ...
cl /nologo /MT driver.c junk_all.obj /Fe:junk_all.exe
if errorlevel 1 ( echo [ERROR] cl 失败 & exit /b 1 )

echo [3/3] 完成。生成 junk_all.exe
echo.
echo 运行验证：     junk_all.exe
echo 线性扫描视角： objdump -d junk_all.obj
echo 递归下降视角： 用 IDA / Ghidra 打开 junk_all.exe
endlocal
