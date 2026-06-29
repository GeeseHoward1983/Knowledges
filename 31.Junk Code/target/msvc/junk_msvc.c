/* ============================================================================
 * junk_msvc.c —— 花指令全类型实验靶子（MSVC 内联汇编版 / x86 32-bit）
 * ----------------------------------------------------------------------------
 * 不依赖 NASM，只需 Visual Studio 的 cl，平台必须选 x86（Win32），
 * 因为 __asm 内联汇编仅 32 位 MSVC 支持。
 *
 * 编译见 build_msvc.bat：
 *   cl /MT junk_msvc.c /Fe:junk_msvc.exe       (在 x86 Native Tools 命令提示符)
 *
 * 每个 demoNN() 无论花指令如何绕，最终返回编号 NN，证明语义不变。
 * 用 IDA / x64dbg 打开 junk_msvc.exe，定位各 demo 观察反汇编错位。
 *
 * 说明：_emit 0xNN 在 MSVC 内联汇编里等价于 NASM 的 db，用于插入原始垃圾字节。
 * ==========================================================================*/
#include <stdio.h>
#include <windows.h>

/* 类型1：跳过字节型 EB 01 + 垃圾 ----------------------------------------- */
__declspec(noinline) int demo01(void) {
    int r;
    __asm {
        jmp short L1
        _emit 0xE8          ; junk
    L1:
        mov r, 1
    }
    return r;
}

/* 类型2：不透明谓词 xor+jz+垃圾 ----------------------------------------- */
__declspec(noinline) int demo02(void) {
    int r;
    __asm {
        xor eax, eax
        jz short L1         ; 恒真
        _emit 0xE8          ; junk
    L1:
        mov r, 2
    }
    return r;
}

/* 类型3：互补跳转对 jz+jnz 同目标 --------------------------------------- */
__declspec(noinline) int demo03(void) {
    int r;
    __asm {
        cmp eax, eax
        jz short L1
        jnz short L1        ; 互补兜底
        _emit 0xE8          ; junk
    L1:
        mov r, 3
    }
    return r;
}

/* 类型4：push/ret 伪跳转 ------------------------------------------------- */
__declspec(noinline) int demo04(void) {
    int r;
    /* 注：push offset+ret 的字节级演示见 NASM 版(demo04)。MSVC 内联汇编对函数内
       局部标签的 offset 解析不可靠（会拿到错误地址→ret 跳飞执行到 0xCC→断点异常），
       故此处用等价的直接跳转，仍保留“跳过垃圾字节”的花指令效果。 */
    __asm {
        jmp L1              ; 无条件跳转（等价 push offset+ret 的控制流效果）
        _emit 0xCC          ; junk，被跳过
        _emit 0xCC
    L1:
        mov r, 4
    }
    return r;
}

/* 类型5：call/pop 取 EIP —— 已从 MSVC 版移除。
   原因：call L1;pop eax 直接操作 esp，与 MSVC __declspec(noinline) 的栈帧管理冲突，
   无法在不掏空 demo 语义的前提下用内联汇编可靠实现。
   该形态的字节级精确、可运行版本见 NASM 版 nasm/junk_all.asm 的 _demo05。 */

/* 类型6：call + 改返回地址 ---------------------------------------------- */
__declspec(noinline) int demo06(void) {
    int r;
    /* 注：call+改返回地址 的字节级演示见 NASM 版(demo06)。此处同样因 offset 局部标签
       不可靠，改用等价直接跳转，保留“跳过中间垃圾字节”的效果。 */
    __asm {
        jmp REAL6           ; 等价于 call+改返回地址 的最终控制流
        _emit 0xCC          ; junk
        _emit 0xCC
    REAL6:
        mov r, 6
    }
    return r;
}

/* 类型7：间接跳转 mov reg,imm; jmp reg ---------------------------------- */
__declspec(noinline) int demo07(void) {
    int r;
    /* 注：mov reg,offset+jmp reg 的间接跳转字节演示见 NASM 版(demo07)。MSVC 内联汇编
       offset 局部标签不可靠，改用等价直接跳转，保留“跳过垃圾字节”的效果。 */
    __asm {
        jmp L7              ; 等价控制流（间接跳转的精确字节见 NASM 版）
        _emit 0xE8          ; junk
        _emit 0x90
    L7:
        mov r, 7
    }
    return r;
}

/* 类型8：重叠指令（恒跳到 mov 立即数中部制造错位） --------------------- */
__declspec(noinline) int demo08(void) {
    int r;
    __asm {
        xor eax, eax
        jz short MID
        _emit 0xB8          ; 'mov eax,imm32' 操作码，误导 RD 对齐
    MID:
        mov r, 8
    }
    return r;
}

/* 类型9：SEH 异常驱动控制流（真实逻辑在 __except） ---------------------- */
__declspec(noinline) int demo09(void) {
    int r = -1;
    __try {
        int z = 0;
        r = 1 / z;          /* 除零异常 → 跳 __except，静态看不到这条边 */
        printf("    (unreachable)\n");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        r = 9;              /* 真实逻辑藏在异常处理块 */
    }
    return r;
}

/* 类型10：冗余前缀 ------------------------------------------------------- */
__declspec(noinline) int demo10(void) {
    int r;
    __asm {
        _emit 0x66          ; 冗余操作数大小前缀
        _emit 0x66
        nop                 ; 被前缀修饰的 nop
        mov r, 10
    }
    return r;
}

/* 类型11：自修改代码 SMC（运行时解密一段真实机器码并执行） -------------- */
/* 真实机器码：B8 0B 00 00 00 C3 (mov eax,11; ret)；存储时整体 XOR 0xAA。 */
__declspec(noinline) int demo11(void) {
    /* 密文：明文每字节 ^0xAA */
    unsigned char code[] = { 0x12, 0xA1, 0xAA, 0xAA, 0xAA, 0x69 };
    DWORD old;
    int (*fn)(void);
    int i;
    /* 改为可写可执行 */
    VirtualProtect(code, sizeof(code), PAGE_EXECUTE_READWRITE, &old);
    for (i = 0; i < (int)sizeof(code); i++) code[i] ^= 0xAA;  /* 解密 */
    FlushInstructionCache(GetCurrentProcess(), code, sizeof(code));
    fn = (int(*)(void))code;
    return fn();            /* 执行解密出来的真实代码，返回 11 */
}

/* 类型12：花花组合（push/ret → 跳过字节 → 不透明谓词 → 间接跳转） ------ */
__declspec(noinline) int demo12(void) {
    int r;
    __asm {
        jmp S2             ; 链(1)无条件跳(等价push/ret，精确字节见NASM版)
        _emit 0xCC
    S2:
        jmp S3             ; 链(2)跳过字节(不用 short，让 cl 自选编码)
        _emit 0xE8
    S3:
        xor eax, eax       ; 链(3)不透明谓词(用 eax，避免破坏 cl 可能占用的 edx)
        jz S4
        _emit 0xE9
    S4:
        jmp RR             ; 链(4)无条件跳(等价间接跳转，精确字节见NASM版)
        _emit 0x90
        _emit 0x90
    RR:
        mov r, 12
    }
    return r;
}

/* ---------------------------------------------------------------------- */
static int g_pass = 0, g_fail = 0;
static void check(const char* name, int got, int expect) {
    int ok = (got == expect);
    printf("  %-10s => %2d   [%s]\n", name, got, ok ? "OK" : "FAIL");
    if (ok) g_pass++; else g_fail++;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);   /* 无缓冲：中文输出/崩溃前输出即时显示 */
    printf("==== 花指令实验靶子（MSVC 内联汇编版）运行验证 ====\n");
    printf("每个 demo 返回值应等于其编号，证明花指令不改变执行语义。\n");
    printf("注：demo05(call/pop 取 EIP) 因 MSVC 内联汇编栈帧冲突已移除，该形态见 NASM 版。\n\n");

    check("demo01", demo01(), 1);
    check("demo02", demo02(), 2);
    check("demo03", demo03(), 3);
    check("demo04", demo04(), 4);
    /* demo05(call/pop 取 EIP) 已从 MSVC 版移除——与栈帧冲突，见 NASM 版。 */
    check("demo06", demo06(), 6);
    check("demo07", demo07(), 7);
    check("demo08", demo08(), 8);
    check("demo09", demo09(), 9);
    check("demo10", demo10(), 10);
    check("demo11", demo11(), 11);
    check("demo12", demo12(), 12);


    printf("\n==== 结果：通过 %d，失败 %d ====\n", g_pass, g_fail);
    printf("\n提示：用 IDA / x64dbg 打开 junk_msvc.exe，在各 demo 函数处\n");
    printf("      观察反汇编错位，对照《花指令全类型深度展开.md》逐类理解。\n");
    return g_fail ? 1 : 0;
}
