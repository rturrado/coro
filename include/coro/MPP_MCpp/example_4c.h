#pragma once

#include <algorithm>  // for_each
#include <cassert>
#include <chrono>
#include <concepts>
#include <condition_variable>  // condition_variable_any
#include <coroutine>
#include <exception>  // current_exception
#include <fmt/core.h>
#include <fmt/std.h>
#include <functional>  // bind_front
#include <future>  // promise, shared_future
#include <memory>  // enable_shared_from_this, make_shared, shared_ptr, weak_ptr
#include <mutex>  // lock_guard, unique_lock
#include <queue>
#include <stop_token>
#include <string_view>
#include <thread>  // jthread
#include <vector>


// Multi-Paradigm Programming with Modern C++, Georgy Pashkov, Packt Publishing
// Chapter 11 - Dive Deeper Into Coroutines, Adding Syntactic Sugar
//
// example_4c calls a coroutine mul_add, which itself calls a coroutine multiply twice
// The coroutine name is now printed in the debug output, e.g. multiply(2, 4)
//
// Notes on implementation:
//
//   - Each coroutine returns a task scheduler at initial_suspend()
//   - Await transform:
//     - co_await has to wait on an awaiter, but the awaiter doesn't have to be provided by the user of the coroutine
//     - the promise type can transform any value into an awaiter
//
// A possible output (thread numbers, and number of threads may vary depending on the system):
//
// [example_4c]
// [example_4c] Calling mul_add(2, 4, 6, 8)
//     undefined [get_return_object]
//     undefined [initial_suspend]
// [Resuming on executor thread]
//     undefined [get_return_object]
//     undefined [initial_suspend]
//     undefined [get_return_object]
// [Resuming on executor thread]
//     undefined [initial_suspend]
//     multiply(2, 4) [return_value]
// [Resuming on executor thread]
//     multiply(2, 4) [final_suspend]
//     multiply(6, 8) [return_value]
//     multiply(6, 8) [final_suspend]
//     mul_add(2, 4, 6, 8)[return_value]
//     multiply(6, 8) [~state]
// [example_4c] Returned value from coroutine: 56
// [example_4c] Exiting
// [~executor]
//     multiply(2, 4) [~state]
// [Exiting run_thread]
// [Exiting run_thread]
// [Exiting run_thread]
//     mul_add(2, 4, 6, 8) [final_suspend]
//     mul_add(2, 4, 6, 8) [~state]
// [Exiting run_thread]


namespace rtc::coro::mpp_mcpp::v4c {
    // Debug print helper
    inline void debug_print(std::string_view name, std::string_view text) {
        static std::mutex mtx;
        std::lock_guard lock{ mtx };
        fmt::print("\t{} [{}]\n", name, text);
    }


    // Executable
    // Interface class for tasks running in a thread
    //
    class executable : public std::enable_shared_from_this<executable> {
    public:
        virtual void execute() noexcept = 0;
        virtual ~executable() = default;
    };

    using executable_ptr = std::shared_ptr<executable>;


    // Executor
    // Thread pool
    // Schedules tasks to be run in a thread and executes them
    //
    class executor final {
    public:
        executor(size_t number_of_threads) {
            for (size_t i{ 0 }; i < number_of_threads; ++i) {
                threads_.emplace_back(std::bind_front(&executor::run_thread, this));
            }
        }
        ~executor() {
            fmt::print("[~executor]\n");
            std::ranges::for_each(threads_, [](std::jthread& t) { t.request_stop(); });
            std::ranges::for_each(threads_, [](std::jthread& t) { t.join(); });
        }
        void schedule(executable_ptr ex) {
            {
                std::lock_guard lock{ mutex_ };
                queue_.push(std::move(ex));
            }
            wakeup_.notify_one();
        }
        void run_thread(std::stop_token stoken) {
            while (true) {
                std::unique_lock<std::mutex> lock{ mutex_ };
                if (queue_.empty()) {
                    wakeup_.wait(lock, stoken, [this]() {
                        return not queue_.empty();
                    });
                    if (stoken.stop_requested()) {
                        break;
                    }
                }
                auto next{ std::move(queue_.front()) };
                queue_.pop();
                lock.unlock();
                next->execute();
            }
            fmt::print("[Exiting run_thread]\n");
        }
    private:
        std::vector<std::jthread> threads_;
        std::mutex mutex_;
        std::queue<executable_ptr> queue_;
        std::condition_variable_any wakeup_;
    };


    // Executor provider
    // Implemented as a singleton
    // Although, ideally, an executor factory
    //
    constexpr size_t DEFAULT_CONCURRENCY = 4;

    template <size_t concurrency_level = DEFAULT_CONCURRENCY>
    class executor_provider {
    public:
        static executor& get_executor() {
            static class executor instance { concurrency_level };
            return instance;
        }
    };

    using task_executor_provider = executor_provider<>;


    // Concepts
    //
    template <typename T>
    concept is_executor_provider = std::is_same_v<decltype(T::get_executor()), executor&>;

    // Task result
    // At the moment, a coroutine based task cannot return void
    template <typename T>
    concept is_task_result =
        std::is_copy_constructible<T>::value ||
        std::is_move_constructible<T>::value ||
        // std::is_void_v<T> ||
        std::is_reference<T>::value;

    template <typename T>
    concept is_task = requires (T t) {
        typename T::task_type;
        typename T::promise_type;
        typename T::handle_type;
    };


    // Task
    // A coroutine based task
    //
    template <is_task_result result_t>
    class ctask {
        class coroutine_promise;
        struct state;
    public:
        auto get() const {
            return shared_future_.get();
        }

        auto wait() const {
            shared_future_.wait();
        }

        bool ready() const {
            return shared_future_.wait_for(std::chrono::duration<size_t>::zero()) == std::future_status::ready;
        }

        using promise_type = coroutine_promise;
        using task_type = ctask<result_t>;
        using handle_type = std::coroutine_handle<task_type::promise_type>;
    private:
        ctask(handle_type handle)
            : shared_state_{ std::make_shared<state>(handle) }
            , shared_future_{ shared_state_->result_.get_future() }
        {}

        std::shared_ptr<state> shared_state_;
        std::shared_future<result_t> shared_future_;
    };


    // Task scheduler
    // The awaiter schedules tasks on an executor thread
    template <is_task task_t>
    class task_scheduler : public std::suspend_always {
    public:
        void await_suspend(task_t::handle_type handle) const noexcept {
            auto& promise{ handle.promise() };
            task_executor_provider::get_executor().schedule(promise.get_state());
        }
    };


    // Shared state
    // Shared between all instances of a task
    // Keeps the coroutine handle
    template <is_task_result result_t>
    struct ctask<result_t>::state : public executable {
        state(handle_type handle)
            : handle_{ handle }
        {}
        ~state() {
            debug_print(name_, "~state");
        }
        void execute() noexcept override {
            fmt::print("[Resuming on executor thread]\n");
            handle_.resume();
        }
        handle_type handle_;
        std::string name_{};
        std::promise<result_t> result_;  // nothing to do with coroutine promise
    };


    // Promise type
    //
    template <is_task_result result_t>
    class ctask<result_t>::coroutine_promise {
    private:
        std::weak_ptr<state> shared_state_;
        void debug(std::string_view text) {
            debug_print(get_state()->name_, text);
        }
    public:
        auto get_return_object() {
            debug_print("undefined", "get_return_object");
            auto ret{ task_type{ handle_type::from_promise(*this) } };
            shared_state_ = ret.shared_state_;
            return ret;
        }
        auto initial_suspend() {
            debug_print("undefined", "initial_suspend");
            return task_scheduler<task_type>{};
        }
        auto final_suspend() noexcept {
            debug("final_suspend");
            get_state()->handle_ = nullptr;
            return std::suspend_never{};
        }
        template <is_task_result T>
        auto return_value(T&& value) {
            debug("return_value");
            get_state()->result_.set_value(std::forward<T>(value));
        }
        auto unhandled_exception() {
            debug("unhandled_exception");
            get_state()->result_.set_exception(std::current_exception());
        }
        auto await_transform(std::string name) {
            get_state()->name_ = std::move(name);
            return std::suspend_never{};
        }
        auto get_state() {
            auto state{ shared_state_.lock() };
            assert(state);
            return state;
        }
    };


    // Example 4c
    //
    inline ctask<int> multiply(int a, int b) {
        co_await fmt::format("multiply({}, {})", a, b);
        co_return a * b;
    }

    inline ctask<int> mul_add(int a, int b, int c, int d) {
        co_await fmt::format("mul_add({}, {}, {}, {})", a, b, c, d);
        auto p1{ multiply(a, b) };
        auto p2{ multiply(c, d) };
        co_return p1.get() + p2.get();
    }

    inline void example_4c() {
        fmt::print("[example_4c]\n");
        fmt::print("[example_4c] Calling mul_add(2, 4, 6, 8)\n");
        auto task{ mul_add(2, 4, 6, 8) };
        fmt::print("[example_4c] Returned value from coroutine: {}\n", task.get());
        fmt::print("[example_4c] Exiting\n");
    }
}  // namespace rtc::coro::mpp_mcpp::v4c
