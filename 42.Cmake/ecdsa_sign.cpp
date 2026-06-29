#include <iostream>
#include <string>
#include <vector>
#include <gmp.h>

struct ECPoint {
    mpz_t x, y;
    
    ECPoint() {
        mpz_init(x);
        mpz_init(y);
    }
    
    ECPoint(const ECPoint& other) {
        mpz_init_set(x, other.x);
        mpz_init_set(y, other.y);
    }
    
    ~ECPoint() {
        mpz_clear(x);
        mpz_clear(y);
    }
    
    void set_infinity() {
        mpz_set_si(x, 0);
        mpz_set_si(y, 0);
    }
    
    bool is_infinity() const {
        return mpz_cmp_ui(x, 0) == 0 && mpz_cmp_ui(y, 0) == 0;
    }
};

class ECC {
private:
    mpz_t a, b, q, n;  // 椭圆曲线参数 y^2 = x^3 + ax + b (mod q)，n为基点的阶
    
public:
    ECC() {
        mpz_init(a);
        mpz_init(b);
        mpz_init(q);
        mpz_init(n);
    }
    
    ~ECC() {
        mpz_clear(a);
        mpz_clear(b);
        mpz_clear(q);
        mpz_clear(n);
    }
    
    void init_params(long a_val, const char* b_hex, const char* q_hex, const char* n_hex) {
        mpz_set_si(a, a_val);
        mpz_set_str(b, b_hex, 16);
        mpz_set_str(q, q_hex, 16);
        mpz_set_str(n, n_hex, 16);
    }
    
    // 模逆运算
    void mod_inverse(mpz_t result, const mpz_t a, const mpz_t m) const {
        mpz_t temp;
        mpz_init(temp);
        mpz_invert(result, a, m);
        mpz_clear(temp);
    }
    
    // 点加法
    void point_add(ECPoint& result, const ECPoint& p1, const ECPoint& p2) const {
        if (p1.is_infinity()) {
            mpz_set(result.x, p2.x);
            mpz_set(result.y, p2.y);
            return;
        }
        
        if (p2.is_infinity()) {
            mpz_set(result.x, p1.x);
            mpz_set(result.y, p1.y);
            return;
        }
        
        mpz_t lambda, temp1, temp2;
        mpz_inits(lambda, temp1, temp2, NULL);
        
        if (mpz_cmp(p1.x, p2.x) == 0) {  // 相同点相加或互为逆元
            if (mpz_cmp(p1.y, p2.y) != 0 || mpz_cmp_ui(p1.y, 0) == 0) {
                result.set_infinity();
                mpz_clears(lambda, temp1, temp2, NULL);
                return;
            } else {  // 点加倍
                // lambda = (3*x1^2 + a) / (2*y1)
                mpz_mul(temp1, p1.x, p1.x);  // x1^2
                mpz_mul_ui(temp1, temp1, 3); // 3*x1^2
                mpz_add(temp1, temp1, a);    // 3*x1^2 + a
                mpz_mul_ui(temp2, p1.y, 2);  // 2*y1
                mod_inverse(temp2, temp2, q); // 1/(2*y1)
                mpz_mul(lambda, temp1, temp2); // (3*x1^2 + a)/(2*y1)
                mpz_mod(lambda, lambda, q);
            }
        } else {  // 不同点相加
            // lambda = (y2-y1)/(x2-x1)
            mpz_sub(temp1, p2.y, p1.y);  // y2-y1
            mpz_sub(temp2, p2.x, p1.x);  // x2-x1
            mod_inverse(temp2, temp2, q); // 1/(x2-x1)
            mpz_mul(lambda, temp1, temp2); // (y2-y1)/(x2-x1)
            mpz_mod(lambda, lambda, q);
        }
        
        // x3 = lambda^2 - x1 - x2
        mpz_mul(temp1, lambda, lambda);  // lambda^2
        mpz_sub(temp1, temp1, p1.x);    // lambda^2 - x1
        mpz_sub(temp1, temp1, p2.x);    // lambda^2 - x1 - x2
        mpz_mod(result.x, temp1, q);
        
        // y3 = lambda*(x1-x3) - y1
        mpz_sub(temp1, p1.x, result.x); // x1-x3
        mpz_mul(temp1, lambda, temp1);  // lambda*(x1-x3)
        mpz_sub(temp1, temp1, p1.y);    // lambda*(x1-x3) - y1
        mpz_mod(result.y, temp1, q);
        
        mpz_clears(lambda, temp1, temp2, NULL);
    }
    
    // 标量乘法
    void scalar_mult(ECPoint& result, const ECPoint& point, const mpz_t scalar) const {
        ECPoint temp_point(point);
        result.set_infinity();
        
        mpz_t temp_scalar;
        mpz_init_set(temp_scalar, scalar);
        
        while (mpz_cmp_ui(temp_scalar, 0) > 0) {
            if (mpz_odd_p(temp_scalar)) {
                point_add(result, result, temp_point);
            }
            
            ECPoint double_temp;
            point_add(double_temp, temp_point, temp_point);
            temp_point = double_temp;
            
            mpz_fdiv_q_ui(temp_scalar, temp_scalar, 2);
        }
        
        mpz_clear(temp_scalar);
    }
    
    // 验证点是否在曲线上
    bool is_on_curve(const ECPoint& point) const {
        if (point.is_infinity()) return true;
        
        mpz_t left, right, temp;
        mpz_inits(left, right, temp, NULL);
        
        // 左边：y^2
        mpz_mul(left, point.y, point.y);
        mpz_mod(left, left, q);
        
        // 右边：x^3 + ax + b
        mpz_mul(right, point.x, point.x);
        mpz_mul(right, right, point.x);      // x^3
        mpz_mul(temp, a, point.x);           // ax
        mpz_add(right, right, temp);         // x^3 + ax
        mpz_add(right, right, b);            // x^3 + ax + b
        mpz_mod(right, right, q);
        
        bool on_curve = (mpz_cmp(left, right) == 0);
        
        mpz_clears(left, right, temp, NULL);
        return on_curve;
    }
};

void ecdsa_sign(ECC& ecc, const ECPoint& G, const char* d_hex, const char* hash_hex, char* r_out, char* s_out) {
    mpz_t d, hash, k, k_inv, z;
    mpz_inits(d, hash, k, k_inv, z, NULL);
    
    mpz_set_str(d, d_hex, 16);
    mpz_set_str(hash, hash_hex, 16);
    
    // 选择随机数k（这里使用固定值便于演示，实际应用中应该使用真随机数）
    // 在实际应用中，每次签名都应该使用不同的安全随机数
    mpz_set_str(k, "123456789ABCDEF123456789ABCDEF12345678", 16);
    
    // 计算 kG = (x1, y1)
    ECPoint kG;
    ecc.scalar_mult(kG, G, k);
    
    // r = x1 mod n
    mpz_t n;
    mpz_init(n);
    mpz_set_str(n, "C564EEF19A080B07", 16);  // n值
    mpz_mod_ui(kG.x, kG.x, n);
    mpz_set_str(r_out, mpz_get_str(NULL, 10, kG.x), 10);
    
    // 计算k的模n逆
    mpz_invert(k_inv, k, n);
    
    // s = k^-1 (z + rd) mod n
    mpz_mul(z, d, kG.x);     // rd
    mpz_add(z, z, hash);     // z + rd
    mpz_mul(z, z, k_inv);    // k^-1 (z + rd)
    mpz_mod(z, z, n);        // mod n
    
    mpz_get_str(s_out, 16, z);
    
    mpz_clears(d, hash, k, k_inv, z, n, NULL);
}

int main() {
    ECC ecc;
    
    // 初始化椭圆曲线参数
    ecc.init_params(-3, "22996B9C33AEEFDB", "C564EEF070E69193", "C564EEF19A080B07");
    
    // 设置基点G
    ECPoint G;
    mpz_set_str(G.x, "2223A1D595845FA2", 16);
    mpz_set_str(G.y, "1C4EEE9222DDDA62", 16);
    
    // 验证基点在曲线上
    if (!ecc.is_on_curve(G)) {
        std::cout << "基点不在椭圆曲线上！" << std::endl;
        return 1;
    }
    
    // 给定参数
    const char* private_key = "54656E63656E7420";  // 私钥d
    const char* msg_hash = "849abd70d3145e0c";     // 消息哈希
    
    // 生成签名
    char r[64], s[64];
    ecdsa_sign(ecc, G, private_key, msg_hash, r, s);
    
    std::cout << "椭圆曲线参数:" << std::endl;
    std::cout << "a = -3" << std::endl;
    std::cout << "b = 0x22996B9C33AEEFDB" << std::endl;
    std::cout << "q = 0xC564EEF070E69193" << std::endl;
    std::cout << "n = 0xC564EEF19A080B07" << std::endl;
    std::cout << "G = (0x2223A1D595845FA2, 0x1C4EEE9222DDDA62)" << std::endl;
    std::cout << std::endl;
    
    std::cout << "私钥 d = 0x" << private_key << std::endl;
    std::cout << "消息哈希 = 0x" << msg_hash << std::endl;
    std::cout << std::endl;
    
    std::cout << "生成的签名:" << std::endl;
    std::cout << "r = 0x" << r << std::endl;
    std::cout << "s = 0x" << s << std::endl;
    
    return 0;
}