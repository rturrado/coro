#pragma once

#include <asio.hpp>
#include <coroutine>
#include <exception>  // terminate
#include <fmt/core.h>
#include <memory>  // shared_ptr, weak_ptr
#include <string>
#include <type_traits>  // is_void_v


namespace rtc::coro::my_coro {
    template <typename T>
        requires (!std::is_void_v<T>)
    class my_awaitable {
    public:
        using value_type = T;

        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;

        T value() const {
            if (auto handle{ shared_state_->handle }) {
                // Resume the coroutine
                handle.resume();
            }
            // When we get here, the coroutine has resumed and returned
            return shared_state_->value;
        }

        constexpr my_awaitable() noexcept = default;
        my_awaitable(my_awaitable&& other) noexcept = default;
        my_awaitable& operator=(my_awaitable&& other) = default;
        ~my_awaitable() = default;


        // Is the coroutine ready to resume immediately?
        // Returning true would make this suspend_never
        bool await_ready() const noexcept {
            fmt::print("await_ready\n");
            return true;
        }

        // Called when the coroutine is suspended
        // You can schedule resumption here
        void await_suspend(handle_type h) {
            fmt::print("await_suspend\n");
        }

        // Called before the coroutine resumes
        T await_resume() {
            fmt::print("await_resume\n");
            return value();
        }
    private:
        struct state {
            std::string name{};
            T value{};
            handle_type handle{};
        };

        std::shared_ptr<state> shared_state_{};

        my_awaitable(handle_type handle)
            : shared_state_{ std::make_shared<state>() } {
            shared_state_->handle = handle;
        }
    };

    template <typename T>
        requires (!std::is_void_v<T>)
    struct my_awaitable<T>::promise_type {
        // Return value of a coroutine
        auto get_return_object() {
            fmt::print("get_return_object\n");

            auto handle{ handle_type::from_promise(*this) };

            auto return_object{ my_awaitable<T>{ handle } };
            shared_state_ = return_object.shared_state_;
            return return_object;
        }
        // Suspend a coroutine before running it?
        auto initial_suspend() {
            fmt::print("initial_suspend\n");
            return std::suspend_never{};
        }
        // Suspend a coroutine before exiting it?
        auto final_suspend() noexcept {
            fmt::print("final_suspend\n");
            return std::suspend_never{};
        }
        // Invoked when co_return is used
        // Update the return object here
        void return_value(T value) {
            fmt::print("return_value\n");
            if (auto state{ shared_state_.lock() }) {
                state->value = value;
            }
        }
        // Invoked when co_yield is used
        // Update the return object here
        auto yield_value(T value) {
            fmt::print("yield_value\n");
            return std::suspend_always{};
        }
        // Invoked when coroutine code throws
        void unhandled_exception() {
            fmt::print("unhandled_exception\n");
            std::terminate();
        }

    private:
        std::weak_ptr<state> shared_state_{};
    };

    std::generator<int> server() {
        for (int i{ 0 }; i < 3; ++i) {
            fmt::print("[server] Returning: {}\n", i);
            co_yield i;
        }
    }

    my_awaitable<int> client() {
        for (int i : server()) {
            fmt::print("[client] Received: {}\n", i);
            if (i > 3) {
                co_return 0;
            }
        }
    }

    inline void test_my_coro() {
        client();
    }
}  // namespace rtc::coro::my_coro
