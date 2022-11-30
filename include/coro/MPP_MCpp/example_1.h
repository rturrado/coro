#pragma once

#include <coroutine>
#include <exception>  // terminate
#include <fmt/core.h>


// Multi-Paradigm Programming with Modern C++, Georgy Pashkov, Packt Publishing
// Chapter 10 - Introduction to Coroutines, Resuming The Coroutine
//
// example_1 calls coroutine coro_1
// 1) coro_1 suspends indefinitely and returns control to caller
// 2) caller receives an intermediate result from the coroutine, coroutine_result{ 1 }, set at get_return_object
//
// Output:
//
//   [example_1] Calling coro_1()
//   [get_return_object]
//   [initial_suspend]
//   [coro_1] Suspending...
//   [example_1] Returned value from coroutine: 1
//   [example_1] Exiting


namespace rtc::coro::mpp_mcpp::v1 {
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
            return coroutine_result{ 1 };
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

    inline coroutine_result coro_1() {
        fmt::print("[coro_1] Suspending...\n");
        co_await std::suspend_always{};
        fmt::print("[coro_1] Resumed\n");
    }

    inline void example_1() {
        fmt::print("[example_1] Calling coro_1()\n");
        auto result{ coro_1() };
        fmt::print("[example_1] Returned value from coroutine: {}\n", result.value);
        fmt::print("[example_1] Exiting\n");
    }
}  // namespace rtc::coro::mpp_mcpp::v1
