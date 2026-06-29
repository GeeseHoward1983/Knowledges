/*
 * 使用Miracl库实现椭圆曲线数字签名
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
    cinstr(d, "54656E63656E7420");                  // d = 0x54656E63656E7020 (私钥)
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

    // 生成签名
    big k, r, s, z;
    k = mirvar(0);
    r = mirvar(0);
    s = mirvar(0);
    z = mirvar(0);

    // 选择随机数k (在实际应用中应使用真正的随机数)
    // 这里为了演示，我们使用一个固定值
    cinstr(k, "123456789ABCDEF123456789ABCDEF12345678");

    // 计算点 (x1, y1) = k * G
    ecurve_mult(k, G, temp_point);
    
    // 提取x坐标到r
    epoint_get(temp_point, r, temp_point->Y);  // 获取x坐标到r

    // 计算 r = x1 mod n
    bigmod(r, n, r);

    // 如果r为0，则签名失败
    if (size(r) == 0) {
        std::cout << "错误: r = 0，签名失败" << std::endl;
        return 1;
    }

    // 计算s = k^(-1)(hash + dr) mod n
    // 首先计算 hash + dr mod n
    multiply(d, r, s);        // s = d*r
    add(hash, s, s);          // s = hash + d*r
    bigmod(s, n, s);          // s = (hash + d*r) mod n

    // 计算k的模n逆
    big inv_k = mirvar(0);
    xgcd(k, n, inv_k, inv_k, inv_k);  // inv_k = k^(-1) mod n
    if (size(inv_k) < 0) {
        mad(inv_k, inv_k, inv_k, n, n, inv_k);  // 处理负数情况
    }

    // 计算 s = k^(-1) * (hash + d*r) mod n
    mad(inv_k, s, inv_k, n, n, s);    // s = [k^(-1) * (hash + d*r)] mod n

    // 如果s为0，则签名失败
    if (size(s) == 0) {
        std::cout << "错误: s = 0，签名失败" << std::endl;
        return 1;
    }

    // 输出签名结果
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

    std::cout << "生成的签名:" << std::endl;
    std::cout << "r = 0x" << std::hex; cotstr(r, stdout); std::cout << std::dec << std::endl;
    std::cout << "s = 0x" << std::hex; cotstr(s, stdout); std::cout << std::dec << std::endl;

    // 清理内存
    mirkill(a); mirkill(b); mirkill(q); mirkill(n);
    mirkill(px); mirkill(py); mirkill(qx); mirkill(qy);
    mirkill(d); mirkill(hash);
    mirkill(k); mirkill(r); mirkill(s); mirkill(z);
    mirkill(inv_k);
    
    epoint_free(G);
    epoint_free(Q);
    epoint_free(temp_point);

    return 0;
}