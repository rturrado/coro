#pragma once

#include <coroutine>
#include <generator.hpp>


namespace rtc::coro {
    std::generator<int> coro_sequence() {
        int i{};
        for (;;) {
            co_yield i++;
        }
    }
}  // namespace rtc::coro
