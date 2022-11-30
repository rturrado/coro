#pragma once

#include <fmt/core.h>
#include <rtc/console.h>


namespace rtc::coro {
    inline asio::awaitable<void> coro_ui() {
        fmt::print("[ui] Waiting for key pressed...\n");
        rtc::console::wait_for_key_pressed();
        co_return;
    }
}  // namespace rtc::coro
