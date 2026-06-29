#!/usr/bin/env bash
# ===========================================================================
# build_gcc.sh —— 编译 GCC/x86-64 版花指令实验靶子（本机已验证可用）
# 适用：cygwin gcc 13.x (Windows) / Linux gcc。
# 用法：  bash build_gcc.sh        然后 ./junk_gcc(.exe)
# ===========================================================================
set -e
SRC=junk_gcc.c
OUT=junk_gcc

if ! command -v gcc >/dev/null 2>&1; then
    echo "[ERROR] 未找到 gcc"; exit 1
fi

# -O0 关优化，避免编译器把花指令优化掉；-fno-pie -no-pie 简化地址处理
echo "[1/2] 编译 $SRC ..."
gcc -O0 -fno-pie -no-pie "$SRC" -o "$OUT" 2>&1

echo "[2/2] 完成。生成 $OUT"
echo
echo "运行验证：     ./$OUT"
echo "线性扫描视角： objdump -d -M intel $OUT | less"
echo "递归下降视角： 用 IDA / Ghidra 打开 $OUT"
echo
echo "重点观察：在 <demo01>/<demo02>/<demo08> 处，objdump 会把垃圾字节"
echo "          (0xE8/0xB8) 误解成 call/mov，并连环错位后续真实指令——"
echo "          而程序实际运行（上面的验证）返回值完全正确。"
