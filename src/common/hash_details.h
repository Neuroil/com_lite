//
// Created by Neuroil on 2026/8/13.
//

// =====================================================================
// 学习笔记:OpenSSL EVP API —— 带盐哈希(PBKDF2)从入门到上手
// =====================================================================
//
// 【唯一的心理模型】
//   EVP 全家族(摘要 / MAC / KDF / 加解密 / 签名)共享同一个五步循环:
//
//       (1) _new 建上下文  (2) Init 初始化  (3) Update × N 喂数据
//       (4) Final 取结果   (5) _free 释放
//
//   学会下面 sha_digest() 里的这一个循环,其余函数只是换了个名字的
//   变体。EVP 不是复杂,是 OpenSSL 的 man page 把所有族类混在一起,
//   让你误以为要一次记住几百个函数。实际上每个阶段你只需要 3~4 个。
//
// 【学习顺序】(本文件即按此顺序排列)
//   阶段 0: print_openssl_errors()  —— 错误调试器, 先学会看报错
//   阶段 1: digest_for()            —— 算法编号 -> 摘要对象
//   阶段 2: sha_digest()            —— 手写五步循环, 理解心智模型
//   阶段 3: pbkdf2_hash()           —— PKCS5_PBKDF2_HMAC 一行版
//                                        (★ 实际工程先用它, 最简单)
//   阶段 4: pbkdf2_hash_kdf()       —— EVP_KDF 现代通用版
//                                        (换 scrypt/Argon2id 只改一个字符串)
//
// 【旧 API 为什么弃用】
//   SHA256_Init / SHA256_Update / SHA256_Final、MD5() 这类"裸函数"
//   在 OpenSSL 3.0 起被标记 deprecated。EVP 把算法变成 provider 插件,
//   一份代码可以无缝换成 FIPS 认证模块或其他后端 —— 这就是"EVP 是
//   更好的实践"的全部理由。
//
// 【练习清单】(跟着做一遍就入门了)
//   (1) 编译本文件后,写个小 main 调 sha_digest,和命令行对拍:
//        printf 输出 hex 后与 `openssl dgst -sha512 文件` 比较
//        (注意:OpenSSL 输出"文件内容"的摘要;你手写时要喂同样的字节)
//   (2) 给编译加 -DOPENSSL_API_COMPAT=0x30000000L,再故意写一句
//        SHA256_Init(...),看编译器如何把弃用 API 报成错误 ——
//        让编译器当你的老师。
//   (3) 把阶段 4 的 "PBKDF2" 换成 "SCRYPT",对照 man7/EVP_KDF-SCRYPT
//        改参数,体会 provider 抽象带来的"换算法只改配置"。
//   (4) 用 `openssl kdf -keylen 32 -kdfopt digest:SHA512
//        -kdfopt pass:... -kdfopt salt:... -kdfopt iter:210000 PBKDF2`
//        对拍 pbkdf2_hash_kdf 的结果。
//
// 【安全红线】
//   · 密码哈希必须 = 盐(≥16 字节,RAND_bytes 生成)+ 足够大的迭代数;
//     裸 SHA-512(password||salt) 太快,挡不住暴力破解。
//   · PBKDF2-SHA512 迭代数建议 ≥ 210000(OWASP 基准);
//     更强可考虑 scrypt / Argon2id(OpenSSL 3.2+ 支持)。
//   · 存储时把"算法版本 + 迭代数 + 盐 + 哈希"一起存,
//     例如: $pbkdf2-sha512$i=210000$<盐hex>$<哈希hex>
//     将来升级算法或调迭代数,老记录还能识别、可逐步迁移。
// =====================================================================

#ifndef COM_LITE_HASH_DETAILS_H
#define COM_LITE_HASH_DETAILS_H

#include <openssl/evp.h>
#if defined(__has_include)
#  if __has_include(<openssl/kdf.h>)
#    include <openssl/kdf.h>
#  endif
#endif
#include <openssl/err.h>   // 错误队列(阶段 0 用)

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <vector>

// OpenSSL 具体实现细节
namespace security::details {
    // 打印错误
    inline void print_openssl_errors() {
        ERR_print_errors_fp(stderr);
    }

    namespace old {
        // 选择算法
        inline const EVP_MD* digest_for(int algorithm_name) {
            switch (algorithm_name) {
                case 1: return EVP_sha256();  // 摘要结果固定 32 字节
                default: return EVP_sha512(); // 摘要结果固定 64 字节
            }
        }

        inline std::vector<std::byte> sha_digest(
            // 要哈希的数据(如密码)
            const std::vector<std::byte>& data,

            // 盐(可为空)
            const std::vector<std::byte>& salt,

            // 1 = SHA-256,其他 = SHA-512
            int algorithm_name
        ) {
            // new
            unsigned char buf[EVP_MAX_MD_SIZE];
            unsigned int  out_len = 0;

            EVP_MD_CTX* ctx = EVP_MD_CTX_new();
            if (!ctx) return {};

            // init: 可能失败
            if (EVP_DigestInit_ex(ctx, digest_for(algorithm_name), nullptr) != 1) {
                print_openssl_errors();
                // new -> free 成套
                EVP_MD_CTX_free(ctx);
                return {};
            }

            // update
            EVP_DigestUpdate(ctx, salt.data(), salt.size());
            EVP_DigestUpdate(ctx, data.data(), data.size());

            // final: 可能失败
            if (EVP_DigestFinal_ex(ctx, buf, &out_len) != 1) {
                print_openssl_errors();
                EVP_MD_CTX_free(ctx);
                return {};
            }

            // free
            EVP_MD_CTX_free(ctx);

            return std::vector(reinterpret_cast<std::byte*>(buf), reinterpret_cast<std::byte*>(buf) + out_len);
        }

        // // PBKDF2 实际工程优先用这个
        // inline bool pbkdf2_hash(
        //     const std::vector<std::byte>& password,
        //     // 用户密码
        //     const std::vector<std::byte>& salt,
        //     // 每用户随机盐,≥16 字节
        //     std::size_t                   iterations,
        //     // 迭代数,SHA-512 建议 ≥ 210000
        //     int                           algorithm_name,
        //     // 1 = SHA-256,其他 = SHA-512
        //     std::vector<std::byte>&       out // 调用前先 resize 成期望的密钥字节数
        // ) {
        //     // 返回值:1 = 成功;0 = 失败。
        //     // 注意:所有长度参数都是 int(历史遗留),超大输入会被截断;
        //     // 密码/盐通常只有几十字节,工程上无需担心。
        //     const int ok = PKCS5_PBKDF2_HMAC(
        //         reinterpret_cast<const char*>(password.data()),
        //         // 密码
        //         static_cast<int>(password.size()),
        //         // 密码长度
        //         reinterpret_cast<const unsigned char*>(salt.data()),
        //         // 盐
        //         static_cast<int>(salt.size()),
        //         // 盐长度
        //         static_cast<int>(iterations),
        //         // 迭代数
        //         digest_for(algorithm_name),
        //         // 摘要算法
        //         static_cast<int>(out.size()),
        //         // 期望输出字节数
        //         reinterpret_cast<unsigned char*>(out.data())
        //     ); // 输出缓冲
        //
        //     if (ok != 1) {
        //         print_openssl_errors();
        //         return false;
        //     }
        //     return true;
        // }
    }

    inline bool pbkdf2_hash_kdf(
        const std::vector<std::byte>& password,
        const std::vector<std::byte>& salt,
        std::size_t                   iterations,
        // int                           algorithm_name,
        std::vector<std::byte>&       out_buf
    ) {
        // fetch
        EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "PBKDF2", nullptr);
        if (!kdf) {
            print_openssl_errors();
            return false;
        }

        // new
        EVP_KDF_CTX* ctx = EVP_KDF_CTX_new(kdf);
        EVP_KDF_free(kdf);   // fetch 来的句柄在 new 之后即可释放。
                             // 配对规则:fetch → EVP_KDF_free,
                             //           new   → EVP_KDF_CTX_free。
        if (!ctx) {
            print_openssl_errors();
            return false;
        }

        // init args
        auto iter = static_cast<uint64_t>(iterations);

        OSSL_PARAM param[] = {
            OSSL_PARAM_utf8_string("digest", const_cast<char*>("SHA512"), 0),
            OSSL_PARAM_octet_string("pass", const_cast<std::byte*>(password.data()), password.size()),
            OSSL_PARAM_octet_string("salt", const_cast<std::byte*>(salt.data()), salt.size()),
            OSSL_PARAM_uint64("iter", &iter),
            OSSL_PARAM_END
        };

        // derive the key
        // 返回值为实际写出的字节数(等于 out_buf.size());≤ 0 表示失败。
        int derived_len = EVP_KDF_derive(ctx, reinterpret_cast<unsigned char*>(out_buf.data()), out_buf.size(), param);

        // final
        EVP_KDF_CTX_free(ctx);

        if (derived_len <= 0) {
            print_openssl_errors();
            return false;
        }

        return true;
    }
} // namespace security::details

#endif //COM_LITE_HASH_DETAILS_H
