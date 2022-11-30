#pragma once

#include <coroutine>
#include <exception>  // terminate
#include <fmt/core.h>


// Multi-Paradigm Programming with Modern C++, Georgy Pashkov, Packt Publishing
// Chapter 10 - Introduction to Coroutines, Writing Our First Coroutine
//
// example_2 calls coroutine coro_2
// 1) coro_2 does not suspend, and runs until the end instead
// 2) caller receives an intermediate result from the coroutine, coroutine_result{ 2 }, set at get_return_object
//
// Output:
//
//   [example_2] Calling coro_2()
//   [get_return_object]
//   [initial_suspend]
//   [coro_2] Suspending...
//   [coro_2] Resumed
//   [return_void]
//   [final_suspend]
//   [example_2] Returned value from coroutine: 2
//   [example_2] Exiting


namespace rtc::coro::mpp_mcpp::v2 {
    class coroutine_result {
    public:
        int value{};
        struct promise_type;
    };

    struct coroutine_result::promise_type {
        void debug_print_ln(std::string_view text) {
            fmt::print("[{}]\n", text);
        }
        auto get_return_object() {
            debug_print_ln("get_return_object");
            return coroutine_result{ 2 };
        }
        auto initial_suspend() {
            debug_print_ln("initial_suspend");
            return std::suspend_never{};
        }
        auto final_suspend() noexcept {
            debug_print_ln("final_suspend");
            return std::suspend_never{};
        }
        auto return_void() {
            debug_print_ln("return_void");
        }
        auto unhandled_exception() {
            debug_print_ln("unhandled_exception");
            std::terminate();
        }
    };

    inline coroutine_result coro_2() {
        fmt::print("[coro_2] Suspending...\n");
        co_await std::suspend_never{};
        fmt::print("[coro_2] Resumed\n");
    }

    inline void example_2() {
        fmt::print("[example_2] Calling coro_2()\n");
        auto result{ coro_2() };
        fmt::print("[example_2] Returned value from coroutine: {}\n", result.value);
        fmt::print("[example_2] Exiting\n");
    }
}  // namespace rtc::coro::mpp_mcpp::v2
