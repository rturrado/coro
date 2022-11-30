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
// Chapter 11 - Dive Deeper Into Coroutines, Returning Values
//
// example_4b calls task.get() in order to wait for the value returned from the coroutine
//
// Notes on implementation:
//
//   - The coroutine state adds a result, of type std::promise<result_t>
//   - The coroutine task adds a shared future, connected to the state's promise, 
//     and the functions get(), wait(), and ready(), in order to manage this shared future
//
// A possible output (thread numbers, and number of threads may vary depending on the system):
//
//   [example_4b]
//   Current thread ID: 5612
//   [example_4b] Calling coro_4b()
//       get_return_object
//       initial_suspend
//   Current thread ID: 5612
//   [Resuming on executor thread]
//   Current thread ID: 30840
//       return_value
//       final_suspend
//   [example_4b] Returned value from coroutine: 0x4b
//   [example_4b] Exiting
//       ~state
//       ~executor
//   [Exiting run_thread]
//   [Exiting run_thread]
//   [Exiting run_thread]
//   [Exiting run_thread]

namespace rtc::coro::mpp_mcpp::v4b {
    // Debug print helper
    inline void debug_print(std::string_view text) {
        static std::mutex mtx;
        std::lock_guard lock{ mtx };
        fmt::print("\t[{}]\n", text);
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
            debug_print("~executor");
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


    // Shared state
    // Shared between all instances of a task
    // Keeps the coroutine handle
    template <is_task_result result_t>
    struct ctask<result_t>::state : public executable {
        state(handle_type handle)
            : handle_{ handle }
        {}
        ~state() {
            debug_print("~state");
        }
        void execute() noexcept override {
            fmt::print("[Resuming on executor thread]\n");
            handle_.resume();
        }
        handle_type handle_;
        std::promise<result_t> result_;  // nothing to do with coroutine promise
    };


    // Promise type
    //
    template <is_task_result result_t>
    class ctask<result_t>::coroutine_promise {
    private:
        std::weak_ptr<state> shared_state_;
    public:
        auto get_return_object() {
            debug_print("get_return_object");
            auto ret{ task_type{ handle_type::from_promise(*this) } };
            shared_state_ = ret.shared_state_;
            return ret;
        }
        auto initial_suspend() {
            debug_print("initial_suspend");
            return std::suspend_never{};
        }
        auto final_suspend() noexcept {
            debug_print("final_suspend");
            get_state()->handle_ = nullptr;
            return std::suspend_never{};
        }
        template <is_task_result T>
        auto return_value(T&& value) {
            debug_print("return_value");
            get_state()->result_.set_value(std::forward<T>(value));
        }
        auto unhandled_exception() {
            debug_print("unhandled_exception");
            get_state()->result_.set_exception(std::current_exception());
        }
        auto get_state() {
            auto state{ shared_state_.lock() };
            assert(state);
            return state;
        }
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


    // Example 4b
    //
    inline void print_thread_id() {
        fmt::print("Current thread ID: {}\n", std::this_thread::get_id());
    }

    inline ctask<int> coro_4b() {
        print_thread_id();
        co_await task_scheduler<ctask<int>>{};
        print_thread_id();
        co_return 0x4b;
    }

    inline void example_4b() {
        fmt::print("[example_4b]\n");
        print_thread_id();
        fmt::print("[example_4b] Calling coro_4b()\n");
        auto task{ coro_4b() };
        fmt::print("[example_4b] Returned value from coroutine: {:#x}\n", task.get());
        fmt::print("[example_4b] Exiting\n");
    }
}  // namespace rtc::coro::mpp_mcpp::v4b
