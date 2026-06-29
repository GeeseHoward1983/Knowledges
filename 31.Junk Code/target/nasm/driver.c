/* ============================================================================
 * driver.c —— 花指令实验靶子的验证驱动（配合 junk_all.asm）
 * ----------------------------------------------------------------------------
 * 作用：逐个调用 demo01~demo12，打印返回值，验证“无论花指令怎么绕，
 *       运行结果仍是正确编号”——即花指令对执行语义零影响，只骗静态分析。
 *
 * 编译见 build_nasm.bat：
 *   nasm -f win32 junk_all.asm -o junk_all.obj
 *   cl driver.c junk_all.obj /Fe:junk_all.exe
 *
 * 运行预期输出：
 *   demo01 => 1   ... demo12 => 12   全部 [OK]
 * ==========================================================================*/
#include <stdio.h>
#include <windows.h>
#include <string.h>

/* 来自 junk_all.asm 的外部符号（C 调用约定，链接时去掉前导下划线由编译器处理） */
extern int demo01(void);
extern int demo02(void);
extern int demo03(void);
extern int demo04(void);
extern int demo05(void);
extern int demo06(void);
extern int demo07(void);
extern int demo08(void);
extern int demo09_seh(void);   /* 会触发非法指令异常，需 __try 包裹 */
extern int demo10(void);
extern int demo11_smc(unsigned char* rwx, int len);  /* C 分配 RWX，asm 解密执行 */
extern int demo12(void);

/* 类型11 包装：分配 RWX 内存，填入密文，交给 asm 自修改+执行 */
static int demo11_wrapped(void) {
    /* 密文：明文 B8 0B 00 00 00 C3 (mov eax,11; ret) 每字节 ^0xAA */
    unsigned char enc[] = { 0x12, 0xA1, 0xAA, 0xAA, 0xAA, 0x69 };
    unsigned char* rwx = (unsigned char*)VirtualAlloc(
        NULL, sizeof(enc), MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    int r;
    if (!rwx) return -1;
    memcpy(rwx, enc, sizeof(enc));
    r = demo11_smc(rwx, (int)sizeof(enc));   /* asm 内 XOR 解密后 call 进去 */
    VirtualFree(rwx, 0, MEM_RELEASE);
    return r;
}

static int g_pass = 0, g_fail = 0;

static void check(const char* name, int got, int expect) {
    int ok = (got == expect);
    printf("  %-14s => %2d   [%s]\n", name, got, ok ? "OK" : "FAIL");
    if (ok) g_pass++; else g_fail++;
}

/* 类型9：完整的 SEH 演示——真实逻辑藏在 __except 块里（静态看不到这条控制流边） */
static int demo09_full(void) {
    int r = -1;
    __try {
        int z = 0;
        r = 1 / z;                 /* 除零异常 → 跳到 __except */
        printf("    (unreachable)\n");   /* 静态看着像顺序执行，实则永不到达 */
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        r = 9;                     /* 真实逻辑在异常处理块 */
    }
    return r;
}

/* 类型9 汇编版（ud2 非法指令）也用 __try 包裹，避免崩溃 */
static int demo09_asm_wrapped(void) {
    int r = -1;
    __try {
        r = demo09_seh();          /* ud2 抛异常前 eax 已是 9，但异常会打断返回 */
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        r = 9;                     /* 捕获后视为成功，返回 9 */
    }
    return r;
}

int main(void) {
    printf("==== 花指令实验靶子（NASM 版）运行验证 ====\n");
    printf("每个 demo 的返回值应等于其编号，证明花指令不改变执行语义。\n\n");

    check("demo01",  demo01(),  1);   /* 跳过字节型      */
    check("demo02",  demo02(),  2);   /* 不透明谓词      */
    check("demo03",  demo03(),  3);   /* 互补跳转对      */
    check("demo04",  demo04(),  4);   /* push/ret 伪跳转 */
    check("demo05",  demo05(),  5);   /* call/pop 取 EIP */
    check("demo06",  demo06(),  6);   /* call 改返回地址 */
    check("demo07",  demo07(),  7);   /* 间接跳转        */
    check("demo08",  demo08(),  8);   /* 重叠指令        */
    check("demo09_full", demo09_full(), 9);          /* SEH 异常流(C)   */
    check("demo09_asm",  demo09_asm_wrapped(), 9);   /* 异常流(asm ud2) */
    check("demo10",  demo10(),  10);  /* 冗余前缀        */
    check("demo11",  demo11_wrapped(), 11);/* 自修改代码 SMC  */
    check("demo12",  demo12(),  12);  /* 花花组合        */

    printf("\n==== 结果：通过 %d，失败 %d ====\n", g_pass, g_fail);
    printf("\n提示：现在用 objdump -d junk_all.obj 与 IDA 打开 junk_all.exe，\n");
    printf("      对比反汇编错位，体会每一类花指令攻击的是哪种分析器。\n");
    return g_fail ? 1 : 0;
}
