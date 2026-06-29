#!/usr/bin/env bash
# ===========================================================================
# build_linux.sh —— 用 x86_64-pc-linux-gnu-gcc 交叉编译出【Linux 下运行】的 ELF
# 本机已验证：x86_64-pc-linux-gnu-gcc.exe (GCC 4.9.4) 在 PATH 中，目标三元组
#             x86_64-pc-linux-gnu，产物为 ELF 64-bit，可拷到 Linux 直接运行。
# 用法：  bash build_linux.sh        产物：junk_gcc_linux (ELF)
# ===========================================================================
set -e
CC=x86_64-pc-linux-gnu-gcc.exe       # 已在 PATH，直接用短名
SRC=junk_gcc.c
OUT=junk_gcc_linux

if ! command -v "$CC" >/dev/null 2>&1; then
    echo "[ERROR] 未找到 $CC（请确认交叉编译器在 PATH 中）"; exit 1
fi

# 注意：GCC 4.9.4 不支持 -no-pie/-fcf-protection，该版本默认即 no-PIE，无需显式指定。
echo "[1/2] 交叉编译 $SRC  ($($CC -dumpmachine)) ..."
"$CC" -O0 "$SRC" -o "$OUT"

echo "[2/2] 完成。产物：$OUT"
file "$OUT" 2>/dev/null || true
echo
echo "本机是 Windows，无法直接运行 ELF。请："
echo "  - 拷到 Linux/WSL 运行：   ./$OUT       （应输出 16/16 OK）"
echo "  - 或就地反汇编对比：       objdump -d -M intel $OUT | grep -A 9 '<demo01>:'"
echo
echo "Linux 特有点：交叉编译时 _WIN32 未定义，SMC(demo11) 走 mmap(PROT_EXEC)，"
echo "             异常(demo9) 走 POSIX signal(SIGILL)+sigsetjmp —— 即 Linux 正路径。"
