@echo off
REM ===========================================================================
REM build_msvc.bat —— 编译 MSVC 内联汇编版花指令靶子（junk_msvc.c）
REM 需要：Visual Studio 的 cl，且必须在「x86」工具链下（__asm 仅 32 位支持）。
REM 用法：在 "x86 Native Tools Command Prompt for VS" 里 cd 到 ..\msvc 执行本脚本。
REM ===========================================================================
setlocal
where cl >nul 2>nul
if errorlevel 1 (
    echo [ERROR] 未找到 cl。请在 "x86 Native Tools Command Prompt for VS" 中运行。
    echo         注意：必须是 x86（32 位）工具链，因为内联 __asm 仅 32 位支持。
    exit /b 1
)
echo [1/1] 编译 junk_msvc.c ...
cl /nologo /MT junk_msvc.c /Fe:junk_msvc.exe
if errorlevel 1 ( echo [ERROR] cl 失败 & exit /b 1 )
echo 完成。生成 junk_msvc.exe
echo.
echo 运行验证： junk_msvc.exe
echo 反汇编：   用 IDA / x64dbg 打开 junk_msvc.exe，定位各 demo 函数观察错位。
endlocal
