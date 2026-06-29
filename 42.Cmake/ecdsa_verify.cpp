/*
 * 使用Miracl库实现椭圆曲线数字签名验证
 */

#include <iostream>
#include "miracl.h"
#include <cstring>

int main() {
    // 初始化大整数变量
    big a, b, q, n, px, py, qx, qy, hash;
    a = mirvar(0);
    b = mirvar(0);
    q = mirvar(0);
    n = mirvar(0);
    px = mirvar(0);
    py = mirvar(0);
    qx = mirvar(0);
    qy = mirvar(0);
    hash = mirvar(0);

    // 初始化椭圆曲线参数
    cinstr(a, "-3");                                  // a = -3
    cinstr(b, "22996B9C33AEEFDB");                   // b = 0x22996B9C33AEEFDB
    cinstr(q, "C564EEF070E69193");                   // q = 0xC564EEF070E69193
    cinstr(n, "C564EEF19A080B07");                   // n = 0xC564EEF19A080B07
    cinstr(px, "2223A1D595845FA2");                 // px = 0x2223A1D595845FA2
    cinstr(py, "1C4EEE9222DDDA62");                 // py = 0x1C4EEE9222DDDA62
    cinstr(qx, "297A4A1E5B1FC99B");                 // qx = 0x297A4A1E5B1FC99B
    cinstr(qy, "880198E3724F9FEE");                 // qy = 0x880198E3724F9FEE
    cinstr(hash, "849abd70d3145e0c");               // 消息hash

    // 初始化椭圆曲线
    ecurve_init(a, b, q, MR_PROJECTIVE);  // 初始化椭圆曲线 y^2 = x^3 + ax + b (mod q)

    // 定义点
    epoint *G = epoint_init();  // 基点
    epoint *Q = epoint_init();  // 公钥点
    epoint *temp_point = epoint_init();
    epoint *point1 = epoint_init();
    epoint *point2 = epoint_init();

    // 设置基点G(px, py)
    if (!epoint_set(px, py, 0, G)) {
        std::cout << "无法设置基点G!" << std::endl;
        return 1;
    }

    // 设置公钥点Q(qx, qy)
    if (!epoint_set(qx, qy, 0, Q)) {
        std::cout << "无法设置公钥点Q!" << std::endl;
        return 1;
    }

    // 验证公钥 Q = d*G
    std::cout << "正在验证公钥..." << std::endl;
    big d_test;
    d_test = mirvar(0);
    cinstr(d_test, "54656E63656E7420");              // d = 0x54656E63656E7020 (私钥)
    ecurve_mult(d_test, G, temp_point);
    if (epoint_comp(Q, temp_point)) {
        std::cout << "公钥验证成功: Q = d*G" << std::endl;
    } else {
        std::cout << "公钥验证失败: Q ≠ d*G" << std::endl;
    }
    mirkill(d_test);

    // 输入签名 (假设我们有之前生成的签名)
    big r, s;
    r = mirvar(0);
    s = mirvar(0);
    
    // 这里我们使用示例签名值，实际使用时需要替换为真实签名
    // 假设r和s是通过签名过程得到的
    // 为了演示目的，我们使用一个示例值
    // 实际应用中，这些值应该从签名过程中获取
    cinstr(r, "4251D25B2AC22F51");  // 示例r值
    cinstr(s, "7C5F3020287A2BB5");  // 示例s值

    std::cout << "\n椭圆曲线参数:" << std::endl;
    std::cout << "a = -3" << std::endl;
    std::cout << "b = 0x" << std::hex; cotstr(b, stdout); std::cout << std::dec << std::endl;
    std::cout << "q = 0x" << std::hex; cotstr(q, stdout); std::cout << std::dec << std::endl;
    std::cout << "n = 0x" << std::hex; cotstr(n, stdout); std::cout << std::dec << std::endl;
    std::cout << "G = (0x" << std::hex; cotstr(px, stdout); std::cout << ", 0x"; cotstr(py, stdout); std::cout << ")" << std::dec << std::endl;
    std::cout << std::endl;

    std::cout << "公钥 Q = (0x" << std::hex; cotstr(qx, stdout); std::cout << ", 0x"; cotstr(qy, stdout); std::cout << ")" << std::dec << std::endl;
    std::cout << "消息哈希 = 0x" << std::hex; cotstr(hash, stdout); std::cout << std::dec << std::endl;
    std::cout << std::endl;

    std::cout << "待验证的签名:" << std::endl;
    std::cout << "r = 0x" << std::hex; cotstr(r, stdout); std::cout << std::dec << std::endl;
    std::cout << "s = 0x" << std::hex; cotstr(s, stdout); std::cout << std::dec << std::endl;
    std::cout << std::endl;

    // 验证签名步骤
    // 1. 检查 0 < r < n 和 0 < s < n
    if (compare(r, n) >= 0 || compare(s, n) >= 0 || size(r) <= 0 || size(s) <= 0) {
        std::cout << "签名无效：r或s不在有效范围内" << std::endl;
        return 1;
    }

    // 2. 计算 w = s^(-1) mod n
    big w = mirvar(0);
    xgcd(s, n, w, w, w);  // w = s^(-1) mod n
    if (size(w) < 0) {
        mad(w, w, w, n, n, w);  // 处理负数情况
    }

    // 3. 计算 u1 = hash * w mod n
    big u1 = mirvar(0);
    mad(hash, w, u1, n, n, u1);

    // 4. 计算 u2 = r * w mod n
    big u2 = mirvar(0);
    mad(r, w, u2, n, n, u2);

    // 5. 计算 (x1, y1) = u1*G + u2*Q
    ecurve_mult2(u1, G, u2, Q, temp_point);

    // 6. 计算 v = x1 mod n
    big x1 = mirvar(0);
    epoint_get(temp_point, x1, temp_point->Y);
    bigmod(x1, n, x1);

    // 7. 验证 v == r ?
    if (compare(x1, r) == 0) {
        std::cout << "签名验证成功！" << std::endl;
    } else {
        std::cout << "签名验证失败！" << std::endl;
    }

    // 输出中间计算结果
    std::cout << "\n中间计算结果:" << std::endl;
    std::cout << "w = s^(-1) mod n = 0x" << std::hex; cotstr(w, stdout); std::cout << std::dec << std::endl;
    std::cout << "u1 = hash*w mod n = 0x" << std::hex; cotstr(u1, stdout); std::cout << std::dec << std::endl;
    std::cout << "u2 = r*w mod n = 0x" << std::hex; cotstr(u2, stdout); std::cout << std::dec << std::endl;
    std::cout << "x1 mod n = 0x" << std::hex; cotstr(x1, stdout); std::cout << std::dec << std::endl;
    std::cout << "r = 0x" << std::hex; cotstr(r, stdout); std::cout << std::dec << std::endl;

    // 清理内存
    mirkill(a); mirkill(b); mirkill(q); mirkill(n);
    mirkill(px); mirkill(py); mirkill(qx); mirkill(qy);
    mirkill(hash);
    mirkill(r); mirkill(s);
    mirkill(w); mirkill(u1); mirkill(u2); mirkill(x1);
    
    epoint_free(G);
    epoint_free(Q);
    epoint_free(temp_point);
    epoint_free(point1);
    epoint_free(point2);

    return 0;
}