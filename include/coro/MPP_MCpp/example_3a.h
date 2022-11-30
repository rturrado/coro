#pragma once

#include <coroutine>
#include <exception>  // terminate
#include <fmt/core.h>
#include <memory>  // make_shared, shared_ptr, weak_ptr


// Multi-Paradigm Programming with Modern C++, Georgy Pashkov, Packt Publishing
// Chapter 10 - Introduction to Coroutines, Resuming the Coroutine
//
// Implementation of a lazy evaluation of a value, an int
// Similar to calling std::async with a deferred launch policy
//
// example_3a calls coroutine coro_3a
// Coroutine suspends immediately and returns a coroutine_result<int>, a kind of future<int>
// Caller invokes result.value(), and waits until this future is available, a kind of future.get() 
// The value() function resumes the coroutine via the coroutine handle
// The handle.resume() method loads the coroutine frame from the heap onto the stack, and continue executing from the saved instruction pointer
// The coroutine runs until the end, sets the return value, and returns
// The value() function returns the coroutine result value
//
// Notes on implementation:
//
//   - return_value() is added to the promise_type, and return_void() is removed
//   - since both get_return_object() and return_value(v) need to access the coroutine result value,
//     a shared state is added to the coroutine result
//   - coroutine_result keeps a shared pointer to the shared state, and promise_type a weak_pointer to it
//   - the shared state contains the coroutine result value and the coroutine handle, needed to resume the coroutine
//   - the coroutine handle is created from the promise at get_return_object(), and used to construct a coroutine_result object
//   - the return_value(v) function access the coroutine result value via the weak pointer to the shared state, and updates it
//
// Output:
//
//   [example_3a] Calling coro_3a()
//   [get_return_object]
//   [initial_suspend]
//   [coro_3a] Suspending...
//   [value]
//   [coro_3a] Resumed
//   [return_value]
//   [final_suspend]
//   [value] Returning 0x3a
//   [example_3a] Returned value from coroutine: 0x3a
//   [example_3a] Exiting


namespace rtc::coro::mpp_mcpp::v3a {
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
            auto handle{ handle_type::from_promise(*this) };
            auto ret{ coroutine_result{ handle } };
            shared_state_ = ret.shared_state_;
            return ret;
        }
        auto initial_suspend() {
            debug_print_ln("initial_suspend");
            return std::suspend_never{};
        }
        auto final_suspend() noexcept {
            debug_print_ln("final_suspend");
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

    inline coroutine_result coro_3a() {
        fmt::print("[coro_3a] Suspending...\n");
        co_await std::suspend_always{};
        fmt::print("[coro_3a] Resumed\n");
        co_return 0x3a;
    }

    inline void example_3a() {
        fmt::print("[example_3a] Calling coro_3a()\n");
        auto result{ coro_3a() };
        fmt::print("[example_3a] Returned value from coroutine: {:#x}\n", result.value());
        fmt::print("[example_3a] Exiting\n");
    }
}  // namespace rtc::coro::mpp_mcpp::v3a
