/*
 * 展示如何从ECDSA签名信息中推导随机数k的关系
 * 注意：实际无法直接求解k，因为这相当于解决椭圆曲线离散对数问题
 */

#include <iostream>
#include "miracl.h"
#include <cstring>

int main() {
    // 初始化大整数变量
    big a, b, q, n, px, py, qx, qy, d, hash;
    a = mirvar(0);
    b = mirvar(0);
    q = mirvar(0);
    n = mirvar(0);
    px = mirvar(0);
    py = mirvar(0);
    qx = mirvar(0);
    qy = mirvar(0);
    d = mirvar(0);
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
    cinstr(d, "54656E63656E7020");                  // d = 0x54656E63656E7020 (私钥)
    cinstr(hash, "849abd70d3145e0c");               // 消息hash

    // 初始化椭圆曲线
    ecurve_init(a, b, q, MR_PROJECTIVE);  // 初始化椭圆曲线 y^2 = x^3 + ax + b (mod q)

    // 定义点
    epoint *G = epoint_init();  // 基点
    epoint *Q = epoint_init();  // 公钥点
    epoint *temp_point = epoint_init();

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
    ecurve_mult(d, G, temp_point);
    if (epoint_comp(Q, temp_point)) {
        std::cout << "公钥验证成功: Q = d*G" << std::endl;
    } else {
        std::cout << "公钥验证失败: Q ≠ d*G" << std::endl;
    }

    // 假设我们知道签名 (r, s)
    big r, s;
    r = mirvar(0);
    s = mirvar(0);
    
    // 这里我们使用示例签名值，实际使用时需要替换为真实签名
    // 假设r和s是通过签名过程得到的
    cinstr(r, "4251D25B2AC22F51");  // 示例r值
    cinstr(s, "7C5F3020287A2BB5");  // 示例s值

    std::cout << "\n椭圆曲线参数:" << std::endl;
    std::cout << "a = -3" << std::endl;
    std::cout << "b = 0x" << std::hex; cotstr(b, stdout); std::cout << std::dec << std::endl;
    std::cout << "q = 0x" << std::hex; cotstr(q, stdout); std::cout << std::dec << std::endl;
    std::cout << "n = 0x" << std::hex; cotstr(n, stdout); std::cout << std::dec << std::endl;
    std::cout << "G = (0x" << std::hex; cotstr(px, stdout); std::cout << ", 0x"; cotstr(py, stdout); std::cout << ")" << std::dec << std::endl;
    std::cout << std::endl;

    std::cout << "私钥 d = 0x" << std::hex; cotstr(d, stdout); std::cout << std::dec << std::endl;
    std::cout << "公钥 Q = (0x" << std::hex; cotstr(qx, stdout); std::cout << ", 0x"; cotstr(qy, stdout); std::cout << ")" << std::dec << std::endl;
    std::cout << "消息哈希 = 0x" << std::hex; cotstr(hash, stdout); std::cout << std::dec << std::endl;
    std::cout << std::endl;

    std::cout << "签名信息:" << std::endl;
    std::cout << "r = 0x" << std::hex; cotstr(r, stdout); std::cout << std::dec << std::endl;
    std::cout << "s = 0x" << std::hex; cotstr(s, stdout); std::cout << std::dec << std::endl;
    std::cout << std::endl;

    // ECDSA签名方程: s = k^(-1) * (hash + d*r) mod n
    // 因此: k = s^(-1) * (hash + d*r) mod n
    big s_inv = mirvar(0);
    xgcd(s, n, s_inv, s_inv, s_inv);  // s_inv = s^(-1) mod n
    if (size(s_inv) < 0) {
        mad(s_inv, s_inv, s_inv, n, n, s_inv);  // 处理负数情况
    }

    big dr = mirvar(0);
    multiply(d, r, dr);           // dr = d*r
    big hash_plus_dr = mirvar(0);
    add(hash, dr, hash_plus_dr);  // hash_plus_dr = hash + d*r
    big k_recovered = mirvar(0);
    multiply(s_inv, hash_plus_dr, k_recovered);  // k_recovered = s^(-1) * (hash + d*r)
    bigmod(k_recovered, n, k_recovered);         // k_recovered = [s^(-1) * (hash + d*r)] mod n

    std::cout << "通过签名信息恢复的k值:" << std::endl;
    std::cout << "k = s^(-1) * (hash + d*r) mod n = 0x" << std::hex; cotstr(k_recovered, stdout); std::cout << std::dec << std::endl;
    std::cout << std::endl;

    // 验证：用恢复的k值计算r是否一致
    ecurve_mult(k_recovered, G, temp_point);
    big x1 = mirvar(0);
    epoint_get(temp_point, x1, temp_point->Y);  // 获取x坐标
    bigmod(x1, n, x1);                          // x1 mod n
    
    std::cout << "验证:" << std::endl;
    std::cout << "用恢复的k计算的x1 mod n = 0x" << std::hex; cotstr(x1, stdout); std::cout << std::dec << std::endl;
    std::cout << "原始签名中的r = 0x" << std::hex; cotstr(r, stdout); std::cout << std::dec << std::endl;
    
    if (compare(x1, r) == 0) {
        std::cout << "验证成功：恢复的k值是正确的！" << std::endl;
    } else {
        std::cout << "验证失败：恢复的k值不正确。" << std::endl;
    }

    // 清理内存
    mirkill(a); mirkill(b); mirkill(q); mirkill(n);
    mirkill(px); mirkill(py); mirkill(qx); mirkill(qy);
    mirkill(d); mirkill(hash);
    mirkill(r); mirkill(s);
    mirkill(s_inv); mirkill(dr); mirkill(hash_plus_dr); mirkill(k_recovered); mirkill(x1);
    
    epoint_free(G);
    epoint_free(Q);
    epoint_free(temp_point);

    return 0;
}