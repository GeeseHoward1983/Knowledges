/* ============================================================================
 * junk_gcc.c —— 花指令实验靶子（GCC / x86-64 / 本机可直接编译运行）
 * ----------------------------------------------------------------------------
 * 适配本机环境：cygwin gcc 13.x，默认 64 位（-m32 不可用）。
 * 用 GCC 扩展内联汇编 + .byte 插入原始垃圾字节，覆盖 12 类花指令的 64 位形态，
 * 并额外补充 Linux/GNU 生态特有的花指令手法（见 demo13~demo16）。
 *
 * 编译（本机已验证）：
 *   gcc -O0 -fno-pie -no-pie junk_gcc.c -o junk_gcc.exe        (cygwin/Windows)
 *   gcc -O0 -fno-pie -no-pie junk_gcc.c -o junk_gcc            (Linux)
 *   说明：-fno-pie -no-pie 让标签地址可直接 lea 取绝对地址，简化 push/ret 演示。
 *
 * 反汇编对比：
 *   objdump -d junk_gcc.exe                 ; 线性扫描视角(LS)，垃圾字节处错位
 *   gdb / IDA / Ghidra 打开                 ; 递归下降视角(RD)
 *
 * 设计：每个 demoNN() 无论花指令如何绕，最终返回编号 NN；驱动逐个校验。
 *
 * x86-64 注意点（与 32 位的区别）：
 *   - 返回值在 eax/rax；本文用 "mov eax, imm" 设返回值。
 *   - call/ret 压入 8 字节返回地址（rsp），改返回地址用 qword [rsp]。
 *   - 间接跳转用 64 位寄存器（如 rax）。
 *   - GCC 内联汇编默认 AT&T 语法；这里统一用 .intel_syntax 提升可读性。
 * ==========================================================================*/
#include <stdio.h>
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <string.h>
#endif

/* 统一用 Intel 语法的内联汇编宏封装：进入/退出 intel 语法 */
#define ASM_INTEL_BEGIN ".intel_syntax noprefix\n"
#define ASM_ATT_END     ".att_syntax prefix\n"

/* ---- 类型1：跳过字节型 EB 02 + 2 字节垃圾 ----------------------------- */
__attribute__((noinline)) int demo01(void) {
    int r;
    __asm__ volatile (
        ASM_INTEL_BEGIN
        "   jmp 1f\n"
        "   .byte 0xE8, 0x90\n"      /* junk: E8(call头) + 90 */
        "1: mov eax, 1\n"
        "   mov %0, eax\n"
        ASM_ATT_END
        : "=r"(r) :: "eax");
    return r;
}

/* ---- 类型2：不透明谓词 xor+jz+垃圾 ------------------------------------ */
__attribute__((noinline)) int demo02(void) {
    int r;
    __asm__ volatile (
        ASM_INTEL_BEGIN
        "   xor eax, eax\n"
        "   jz 1f\n"                  /* 恒真 */
        "   .byte 0xE8\n"             /* junk */
        "1: mov eax, 2\n"
        "   mov %0, eax\n"
        ASM_ATT_END
        : "=r"(r) :: "eax", "cc");
    return r;
}

/* ---- 类型3：互补跳转对 jz+jnz 同目标 ---------------------------------- */
__attribute__((noinline)) int demo03(void) {
    int r;
    __asm__ volatile (
        ASM_INTEL_BEGIN
        "   cmp eax, eax\n"
        "   jz 1f\n"
        "   jnz 1f\n"                 /* 互补兜底 */
        "   .byte 0xE8\n"
        "1: mov eax, 3\n"
        "   mov %0, eax\n"
        ASM_ATT_END
        : "=r"(r) :: "eax", "cc");
    return r;
}

/* ---- 类型4：push/ret 伪跳转（64 位：lea 取地址再 push） --------------- */
__attribute__((noinline)) int demo04(void) {
    int r;
    __asm__ volatile (
        ASM_INTEL_BEGIN
        "   lea rax, [rip + 1f]\n"    /* 取真实代码地址（位置无关写法） */
        "   push rax\n"
        "   ret\n"                    /* = jmp 1f */
        "   .byte 0xCC, 0xCC\n"       /* junk */
        "1: mov eax, 4\n"
        "   mov %0, eax\n"
        ASM_ATT_END
        : "=r"(r) :: "rax", "memory");
    return r;
}

/* ---- 类型5：call/pop 取 RIP（GetPC） ---------------------------------- */
__attribute__((noinline)) int demo05(void) {
    int r;
    __asm__ volatile (
        ASM_INTEL_BEGIN
        "   call 1f\n"
        "1: pop rax\n"                /* rax = 1: 的地址 */
        "   mov eax, 5\n"
        "   mov %0, eax\n"
        ASM_ATT_END
        : "=r"(r) :: "rax", "memory");
    return r;
}

/* ---- 类型6：call + 运行时改返回地址（64 位 qword [rsp]） -------------- */
__attribute__((noinline)) int demo06(void) {
    int r;
    __asm__ volatile (
        ASM_INTEL_BEGIN
        "   call 2f\n"
        "2: add qword ptr [rsp], (1f - 2b)\n"  /* 返回地址 += (1f-2b) */
        "   ret\n"                              /* ret 到 1f */
        "   .byte 0xCC, 0xCC\n"                 /* junk，被跳过 */
        "1: mov eax, 6\n"
        "   mov %0, eax\n"
        ASM_ATT_END
        : "=r"(r) :: "memory");
    return r;
}

/* ---- 类型7：间接跳转 lea reg; jmp reg --------------------------------- */
__attribute__((noinline)) int demo07(void) {
    int r;
    __asm__ volatile (
        ASM_INTEL_BEGIN
        "   lea rax, [rip + 1f]\n"
        "   jmp rax\n"                /* 间接跳转，目标静态悬空 */
        "   .byte 0xE8, 0x90\n"
        "1: mov eax, 7\n"
        "   mov %0, eax\n"
        ASM_ATT_END
        : "=r"(r) :: "rax");
    return r;
}

/* ---- 类型8：重叠指令（恒跳到 mov 立即数中部制造错位） ----------------- */
__attribute__((noinline)) int demo08(void) {
    int r;
    __asm__ volatile (
        ASM_INTEL_BEGIN
        "   xor eax, eax\n"
        "   jz 1f\n"
        "   .byte 0xB8\n"             /* 'mov eax,imm32' 操作码，误导 RD 对齐 */
        "1: mov eax, 8\n"
        "   mov %0, eax\n"
        ASM_ATT_END
        : "=r"(r) :: "eax", "cc");
    return r;
}

/* ---- 类型9：异常驱动（Linux 信号 / Windows SEH，见 driver） ----------- */
/* 这里给汇编版的“触发非法指令 ud2”，由驱动用信号/异常捕获后视为成功。 */
__attribute__((noinline)) int demo09_trap(void) {
    __asm__ volatile (
        ASM_INTEL_BEGIN
        "   ud2\n"                    /* 0F 0B：非法指令异常 */
        "   .byte 0xE8, 0xE8\n"       /* junk，不可达 */
        ASM_ATT_END
        ::: "memory");
    return 9; /* 实际到不了，靠驱动捕获 */
}

/* ---- 类型10：冗余前缀 ------------------------------------------------- */
__attribute__((noinline)) int demo10(void) {
    int r;
    __asm__ volatile (
        ASM_INTEL_BEGIN
        "   .byte 0x66, 0x66\n"       /* 冗余操作数大小前缀 */
        "   nop\n"
        "   mov eax, 10\n"
        "   mov %0, eax\n"
        ASM_ATT_END
        : "=r"(r) :: "eax");
    return r;
}

/* ---- 类型11：自修改代码 SMC（运行时解密真实机器码并执行） ------------- */
/* 64 位真实机器码：B8 0B 00 00 00 C3  (mov eax,11; ret)；存储时 XOR 0xAA。 */
__attribute__((noinline)) int demo11(void) {
    unsigned char code[] = { 0x12, 0xA1, 0xAA, 0xAA, 0xAA, 0x69 }; /* 密文 */
    int i;
#ifdef _WIN32
    DWORD old;
    /* cygwin 下也走 Win32 API 改页属性 */
    VirtualProtect(code, sizeof(code), PAGE_EXECUTE_READWRITE, &old);
    for (i = 0; i < (int)sizeof(code); i++) code[i] ^= 0xAA;
    FlushInstructionCache(GetCurrentProcess(), code, sizeof(code));
    return ((int(*)(void))(void*)code)();
#else
    /* Linux：栈不可执行，需 mmap 一块可执行内存 */
    void *m = mmap(NULL, sizeof(code), PROT_READ|PROT_WRITE|PROT_EXEC,
                   MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (m == MAP_FAILED) return -1;
    memcpy(m, code, sizeof(code));
    for (i = 0; i < (int)sizeof(code); i++) ((unsigned char*)m)[i] ^= 0xAA;
    int ret = ((int(*)(void))m)();
    munmap(m, sizeof(code));
    return ret;
#endif
}

/* ---- 类型12：花花组合（push/ret → 跳过字节 → 谓词 → 间接跳转） ------- */
__attribute__((noinline)) int demo12(void) {
    int r;
    __asm__ volatile (
        ASM_INTEL_BEGIN
        "   lea rax, [rip + 2f]\n"
        "   push rax\n"
        "   ret\n"                    /* push/ret */
        "   .byte 0xCC\n"
        "2: jmp 3f\n"                 /* 跳过字节 */
        "   .byte 0xE8\n"
        "3: xor edx, edx\n"           /* 不透明谓词（轮换用 edx） */
        "   jz 4f\n"
        "   .byte 0xE9\n"
        "4: lea rax, [rip + 5f]\n"
        "   jmp rax\n"                /* 间接跳转 */
        "   .byte 0x90, 0x90\n"
        "5: mov eax, 12\n"
        "   mov %0, eax\n"
        ASM_ATT_END
        : "=r"(r) :: "rax", "rdx", "cc");
    return r;
}

/* =====================================================================
 *  Linux / GNU 生态特有的花指令补充（demo13~demo16）
 * ===================================================================== */

/* ---- 类型13：jmp/call 进入 endbr64 之后（CET 间接分支地标错位） ------
 * x86-64 CET 要求间接跳转目标处有 endbr64 (F3 0F 1E FA)。
 * 攻击者可在 endbr64 前后放垃圾，或让直接跳转落到 endbr64 之后，
 * 使工具对“合法间接分支目标”的识别错位。这里演示把 endbr64 当锚点。 */
__attribute__((noinline)) int demo13_endbr(void) {
    int r;
    __asm__ volatile (
        ASM_INTEL_BEGIN
        "   jmp 1f\n"
        "   .byte 0xF3, 0x0F, 0x1E, 0xFA\n" /* 裸 endbr64 字节，作干扰锚点 */
        "1: mov eax, 13\n"
        "   mov %0, eax\n"
        ASM_ATT_END
        : "=r"(r) :: "eax");
    return r;
}

/* ---- 类型14：RIP 相对寻址 + 嵌垃圾（位置无关代码的花指令形态） -------
 * 64 位常用 lea reg,[rip+disp] 取数据地址。把 disp 算到“数据”里，
 * 数据其实是下一段真实代码，绕过对“代码/数据”分界的静态判断。 */
__attribute__((noinline)) int demo14_riprel(void) {
    int r;
    __asm__ volatile (
        ASM_INTEL_BEGIN
        "   lea rax, [rip + 1f]\n"     /* 取“数据”地址，实为真实代码 */
        "   jmp rax\n"
        "   .quad 0x9090909090909090\n"/* 看着像 8 字节数据，其实不可达 */
        "1: mov eax, 14\n"
        "   mov %0, eax\n"
        ASM_ATT_END
        : "=r"(r) :: "rax");
    return r;
}

/* ---- 类型15：用 leave/ret 组合伪造栈帧后跳转 ------------------------- */
/* leave = mov rsp,rbp; pop rbp。可被用来在花指令里搬运/伪造控制流。
 * 这里仅演示一个不影响结果的冗余栈帧操作 + 正常返回。 */
__attribute__((noinline)) int demo15_leave(void) {
    int r;
    __asm__ volatile (
        ASM_INTEL_BEGIN
        "   push rbp\n"
        "   mov rbp, rsp\n"
        "   xor ecx, ecx\n"
        "   jecxz 1f\n"               /* ecx=0 → 恒跳（jecxz 较少见，迷惑性强） */
        "   .byte 0xE8\n"
        "1: mov eax, 15\n"
        "   mov rsp, rbp\n"           /* 手动还原（替代 leave，避免破坏 GCC 帧） */
        "   pop rbp\n"
        "   mov %0, eax\n"
        ASM_ATT_END
        : "=r"(r) :: "rax", "rcx", "memory");
    return r;
}

/* ---- 类型16：loop 指令做恒定循环 + 内嵌垃圾 -------------------------- */
/* loop 在 x86-64 仍存在但编译器几乎不用，出现即可疑；用它构造恒定一次循环。 */
__attribute__((noinline)) int demo16_loop(void) {
    int r;
    __asm__ volatile (
        ASM_INTEL_BEGIN
        "   mov ecx, 1\n"
        "1: dec ecx\n"
        "   jecxz 2f\n"               /* ecx 归零即跳出（恒成立一次） */
        "   .byte 0xE8\n"             /* junk，永不执行 */
        "   jmp 1b\n"
        "2: mov eax, 16\n"
        "   mov %0, eax\n"
        ASM_ATT_END
        : "=r"(r) :: "eax", "ecx", "cc");
    return r;
}

/* ===================================================================== */
#ifdef _WIN32
/* Windows/cygwin：用 SEH 风格的 __try 不可移植到 gcc，改用信号或简单跳过。
 * cygwin 提供 POSIX 信号，可用 signal/sigsetjmp 捕获 SIGILL。 */
#endif
#include <signal.h>
#include <setjmp.h>
static sigjmp_buf g_jb;
static void on_sigill(int sig) { (void)sig; siglongjmp(g_jb, 1); }

/* 类型9 的安全包装：捕获非法指令异常，视为“真实逻辑在异常处理里”返回 9 */
static int demo09_wrapped(void) {
    void (*old)(int) = signal(SIGILL, on_sigill);
    int r;
    if (sigsetjmp(g_jb, 1) == 0) {
        demo09_trap();   /* 触发 ud2 → SIGILL → longjmp 回来 */
        r = -1;          /* 到不了 */
    } else {
        r = 9;           /* 异常处理路径 = 真实逻辑 */
    }
    signal(SIGILL, old);
    return r;
}

static int g_pass = 0, g_fail = 0;
static void check(const char* name, int got, int expect) {
    int ok = (got == expect);
    printf("  %-14s => %2d   [%s]\n", name, got, ok ? "OK" : "FAIL");
    if (ok) g_pass++; else g_fail++;
}

int main(void) {
    printf("==== 花指令实验靶子（GCC / x86-64）运行验证 ====\n");
    printf("每个 demo 返回值应等于其编号，证明花指令不改变执行语义。\n\n");

    printf("[通用 12 类]\n");
    check("demo01",       demo01(),        1);
    check("demo02",       demo02(),        2);
    check("demo03",       demo03(),        3);
    check("demo04",       demo04(),        4);
    check("demo05",       demo05(),        5);
    check("demo06",       demo06(),        6);
    check("demo07",       demo07(),        7);
    check("demo08",       demo08(),        8);
    check("demo09(trap)", demo09_wrapped(), 9);
    check("demo10",       demo10(),        10);
    check("demo11(smc)",  demo11(),        11);
    check("demo12",       demo12(),        12);

    printf("\n[Linux/GNU 特有补充]\n");
    check("demo13_endbr", demo13_endbr(),  13);
    check("demo14_riprel",demo14_riprel(), 14);
    check("demo15_leave", demo15_leave(),  15);
    check("demo16_loop",  demo16_loop(),   16);

    printf("\n==== 结果：通过 %d，失败 %d ====\n", g_pass, g_fail);
    printf("\n提示：objdump -d 看线性扫描错位；IDA/Ghidra 看递归下降错位。\n");
    return g_fail ? 1 : 0;
}
