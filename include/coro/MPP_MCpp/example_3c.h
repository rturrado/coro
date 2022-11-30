#pragma once

#include <coroutine>
#include <exception>  // terminate
#include <fmt/core.h>
#include <iostream>  // cin
#include <memory>  // make_shared, shared_ptr, weak_ptr


// Multi-Paradigm Programming with Modern C++, Georgy Pashkov, Packt Publishing
// Chapter 10 - Introduction to Coroutines, Resuming the Coroutine
//
// A little tweak on example_3b
// Instead of explicitly suspending the coroutine, we can have it suspended when initial_suspend() is invoked
//
// Output:
//
//   [example_3c] Calling coro_3c()
//   [get_return_object]
//   [initial_suspend]
//   [value]
//   [coro_3c] Please enter a number:
//   <user enters 0x3c>
//   [return_value]
//   [final_suspend]
//   [value] Returning 0x3c
//   [example_3c] Returned value from coroutine: 0x3c
//   [example_3c] Exiting


namespace rtc::coro::mpp_mcpp::v3c {
    class coroutine_result {
    public:
        int value() const {
            fmt::print("[value]\n");
            if (auto handle{ shared_state_->handle }) {
                handle.resume();
            }
            fmt::print("[value] Returning {:#x}\n", shared_state_->value);
            return shared_state_->value;
        }
        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;
    private:
        coroutine_result(handle_type handle)
            : shared_state_{ std::make_shared<state>() } {

            shared_state_->handle = handle;
        }
        struct state {
            int value{};
            handle_type handle{};

            ~state() {
                fmt::print("[~state]\n");
                if (handle) {
                    handle.destroy();
                }
            }
        };
        std::shared_ptr<state> shared_state_;
    };

    struct coroutine_result::promise_type {
        std::weak_ptr<state> shared_state_;
        void debug_print_ln(std::string_view text) {
            fmt::print("[{}]\n", text);
        }
        auto get_return_object() {
            debug_print_ln("get_return_object");
            auto ret{ coroutine_result{ handle_type::from_promise(*this) } };
            shared_state_ = ret.shared_state_;
            return ret;
        }
        auto initial_suspend() {
            debug_print_ln("initial_suspend");
            return std::suspend_always{};
        }
        auto final_suspend() noexcept {
            debug_print_ln("final_suspend");
            // Avoid double deletion of the coroutine frame
            if (auto state{ shared_state_.lock() }) {
                state->handle = nullptr;
            }
            return std::suspend_never{};
        }
        auto return_value(int value) {
            debug_print_ln("return_value");
            if (auto state{ shared_state_.lock() }) {
                state->value = value;
            }
        }
        auto unhandled_exception() {
            debug_print_ln("unhandled_exception");
            std::terminate();
        }
    };

    inline coroutine_result coro_3c() {
        fmt::print("[coro_3c] Please enter a number: ");
        int input{};
        std::cin >> input;
        co_return input;
    }

    inline void example_3c() {
        fmt::print("[example_3c] Calling coro_3c()\n");
        auto result{ coro_3c() };
        fmt::print("[example_3c] Returned value from coroutine: {:#x}\n", result.value());
        fmt::print("[example_3c] Exiting\n");
    }
}  // namespace rtc::coro::mpp_mcpp::v3c
