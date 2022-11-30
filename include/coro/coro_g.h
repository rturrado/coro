#pragma once

#include "coro_ui.h"
#include "generator.hpp"

#include <asio.hpp>
#include <coroutine>
#include <fmt/core.h>
#include <fmt/format.h>


namespace rtc::coro {
    inline std::generator<int> coro_g() {
        int i{ 5 };
        fmt::print("[coro_g] &i: {}\n", fmt::ptr(&i));
        while (i--) {
            co_yield i;
        }
    }

    inline asio::awaitable<void> call_coro_g() {
        fmt::print("[call_coro_g()] &coro_g: {}\n", fmt::ptr(coro_g));
        for (auto n : coro_g()) {
            fmt::print("{}\n", n);
        }
        co_return;
    }

    inline void test_coro_g() {
        asio::io_context io_ctx{};
        co_spawn(io_ctx, call_coro_g(), asio::detached);
        co_spawn(io_ctx, coro_ui, asio::detached);
        io_ctx.run();
    }
}  // namespace rtc::coro
