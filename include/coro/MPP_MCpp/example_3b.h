#pragma once

#include <coroutine>
#include <exception>  // terminate
#include <fmt/core.h>
#include <memory>  // make_shared, shared_ptr, weak_ptr


// Multi-Paradigm Programming with Modern C++, Georgy Pashkov, Packt Publishing
// Chapter 10 - Introduction to Coroutines, Resuming the Coroutine
//
// There is a problem with coro_3a
// If the caller never invokes result.value(), the coroutine will always remain suspended,
// so the coroutine frame will not be freed from the heap before the program finishes, which is a memory leak
//
// Notes on implementation:
//
//   - coroutine handles have a destroy() member function that destroys the coroutine frame
//   - we can call handle.destroy() from the coroutine result's shared state destructor
//   - in order to avoid double deletions, handle is first set to nullptr at final_suspend(),
//     and then checked for nullptr at the shared state destructor
// 
// Output:
//
//   [example_3b] Calling coro_3b()
//   [get_return_object]
//   [initial_suspend]
//   [example_3b] Suspending...
//   [example_3b] Exiting
//   [~state] Destroying coroutine frame


namespace rtc::coro::mpp_mcpp::v3b {
    class coroutine_result {
    public:
        int value() const {
            fmt::print("[value]\n");
            if (auto handle{ shared_state_->handle }) {
                handle.resume();
            }
            fmt::print("[value] Returning {}\n", shared_state_->value);
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
                if (handle) {
                    fmt::print("[~state] Destroying coroutine frame\n");
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

    inline coroutine_result coro_3b() {
        fmt::print("[coro_3b] Suspending...\n");
        co_await std::suspend_always{};
        fmt::print("[coro_3b] Resumed\n");
        co_return 0x3b;
    }

    inline void example_3b() {
        fmt::print("[example_3b] Calling coro_3b()\n");
        auto result{ coro_3b() };
        //fmt::print("[example_3b] Returned value from coroutine: {}\n", result.value());
        fmt::print("[example_3b] Exiting\n");
    }
}  // namespace rtc::coro::mpp_mcpp::v3b
