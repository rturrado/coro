#pragma once

#include "coro_ui.h"

#include <asio.hpp>
#include <coroutine>
#include <fmt/core.h>
#include <fmt/format.h>


namespace rtc::coro {
    inline asio::awaitable<int> coro_f(int i) {
        fmt::print("[coro_f] &i: {}\n", fmt::ptr(&i));
        co_return ++i;
    }

    inline asio::awaitable<void> call_coro_f(int i) {
        fmt::print("[call_coro_f({})] &coro_f: {}\n", i, fmt::ptr(coro_f));
        auto n{ co_await coro_f(i) };
        fmt::print("{}\n", n);
    }

    inline void test_coro_f() {
        asio::io_context io_ctx{};
        co_spawn(io_ctx, call_coro_f(5), asio::detached);
        co_spawn(io_ctx, call_coro_f(10), asio::detached);
        co_spawn(io_ctx, coro_ui, asio::detached);
        io_ctx.run();
    }
}  // namespace rtc::coro
