//
// Created by Neuroil on 2026/8/13.
//

#ifndef COM_LITE_SECURITY_H
#define COM_LITE_SECURITY_H

// #include <unistd.h>
#include <chrono>
#include <cstddef>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <openssl/rand.h>

#include "hash_details.h"

namespace utils {
    // 日志用途：转换16进制
    inline std::string bin_to_hex(const std::vector<std::byte>& data) {
        std::string result;
        result.reserve(data.size() * 2);
        constexpr char hex_chars[] = "0123456789abcdef";
        for (auto b : data) {
            auto c = static_cast<unsigned char>(b);
            result.push_back(hex_chars[c >> 4]);
            result.push_back(hex_chars[c & 0x0F]);
        }
        return result;
    }
}

namespace security {
    // 不断尝试，直到成功生成随机数
    // RAND_bytes 成功返回 1，失败返回 0 或 -1；失败时重试
    inline void gen_salt(std::vector<std::byte>& buf) {
        int success = 0;
        do {
            success = RAND_bytes(reinterpret_cast<unsigned char*>(buf.data()), static_cast<int>(buf.size()));
        } while (success != 1);
    }

    // 根据盐值计算哈希
    // 计算结果输出至 out_buf;成功返回 true。
    inline bool salt_hash_compute(
        const std::vector<std::byte>& passwd,
        const std::vector<std::byte>& salt,
        std::vector<std::byte>&       out_buf,
        std::size_t                   iterations
    ) {
        return details::pbkdf2_hash_kdf(passwd, salt, iterations, out_buf);
    }

    // 生成盐并计算哈希
    // salt:调用方先 resize 到期望长度(建议 16 字节),本函数填入随机值;
    // out_buf:调用方先 resize 为期望的哈希字节数,成功后写入结果。
    // 成功返回 true。
    inline bool salt_hash_gen(
        const std::vector<std::byte>& passwd,
        std::vector<std::byte>&       salt,
        std::vector<std::byte>&       out_buf,
        std::size_t                   iterations
    ) {
        gen_salt(salt);
        return details::pbkdf2_hash_kdf(passwd, salt, iterations, out_buf);
    }

    // inline bool timer() {
    //     usleep(500000);
    //     return true;
    // }
    // 固定耗时执行器
    template <typename Func, typename... Args>
    auto fixed_time_execute(
        std::chrono::milliseconds target_duration,
        Func&&                    func,
        Args&&...                 args
    ) -> decltype(func(std::forward<Args>(args)...)) {
        using Clock    = std::chrono::steady_clock;
        using Duration = std::chrono::nanoseconds; // 用纳秒做精细控制

        auto start  = Clock::now();
        auto target = start + target_duration;

        auto result = func(std::forward<Args>(args)...);

        const auto sleep_until_before = target - std::chrono::microseconds(1);
        if (Clock::now() < sleep_until_before) {
            std::this_thread::sleep_until(sleep_until_before);
        }

        while (Clock::now() < target) {
            // 防止循环被优化
            __asm__ volatile("" ::: "memory");
        }

        return result;
    }

    inline bool register_new_passwd(
        const std::vector<std::byte>& passwd,
        std::vector<std::byte>&       salt,
        std::vector<std::byte>&       buf,
        std::size_t                   iterations
    ) {
        if (passwd.size() < 8) return false;

        return salt_hash_gen(passwd, salt, buf, iterations);
    }
}

#endif //COM_LITE_SECURITY_H
