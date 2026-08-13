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

    // 根据盐值哈希
    // 计算结果输出至 buf
    inline void salt_hash_compute(
        const std::vector<std::byte>& hex,
        const std::vector<std::byte>& salt,
        std::vector<std::byte>&       buf,
        std::size_t                   iterations,
        int                           algorithm_name = 3
    ) {
        switch (algorithm_name) {
            case 1:
                // sha-256
            case 2:
                // ...
            default:
                // sha-512
                break;
        }
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
}

#endif //COM_LITE_SECURITY_H
