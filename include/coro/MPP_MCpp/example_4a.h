#pragma once

#include <algorithm>  // for_each
#include <cassert>
#include <chrono>
#include <concepts>
#include <condition_variable>  // condition_variable_any
#include <coroutine>
#include <fmt/core.h>
#include <fmt/std.h>
#include <functional>  // bind_front
#include <memory>  // enable_shared_from_this, make_shared, shared_ptr, weak_ptr
#include <mutex>  // lock_guard
#include <queue>
#include <stdexcept>  // runtime_error
#include <stop_token>
#include <string_view>
#include <thread>  // jthread, sleep_for
#include <vector>


// Multi-Paradigm Programming with Modern C++, Georgy Pashkov, Packt Publishing
// Chapter 11 - Dive Deeper Into Coroutines, Coroutines on a Thread Pool, Scheduling a Coroutine
//
// Implementation of a framework where a coroutine that returns a task, runs automatically on a thread pool
//
// example_4a prints the thread ID and calls coroutine coro_4a
// The coroutine also prints the thread ID, still the same as in main
// Then the coroutine co_awaits for a task being scheduled in a thread pool
// The executor schedules the task in one thread and starts running it
// The main thread returns to example_4a and waits 100ms
// Meanwhile, the task is run in a thread, printing the 'Resuming on executor thread' text, and resuming the coroutine
// The coroutine prints the thread ID, which is now different than the main thread's, and returns
// The main thread exits
//
// Notes on implementation:
//
//   - The task_scheduler implements await_suspend:
//     - on one hand gets the coroutine promise from the handle,
//       and then gets the shared state, which is an executable, from the promise
//     - on the other hand, gets an executor from the executor provider,
//     - and, finally, schedules the executable to be run in the executor
//   - The task executor is implemented as a singleton:
//     - i.e. there is a single thread pool available
//     - it is first instantiated when the task scheduler asks the task executor provider for an executor
//   - The coroutine task, together with its coroutine state, are destroyed at the end of example_4a
//   - Notice the task executor is destroyed at the end of main, after it asks all its threads to finish
//
// A possible output (thread numbers, and number of threads may vary depending on the system):
//
//   [example_4a]
//   Current thread ID: 22448
//   [example_4a] Calling coro_4a()
//       get_return_object
//       initial_suspend
//   Current thread ID: 22448
//       [Resuming on executor thread]
//   Current thread ID: 46912
//       return_value
//       final_suspend
//   [example_4a] Exiting
//       ~state
//       ~executor
//   [Exiting run_thread]
//   [Exiting run_thread]
//   [Exiting run_thread]
//   [Exiting run_thread]


namespace rtc::coro::mpp_mcpp::v4a {
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
    template<is_task_result result_t>
    class ctask {
        class coroutine_promise;
        struct state;
    public:
        ctask()
            : shared_state_{ std::make_shared<state>() }
        {}

        using promise_type = coroutine_promise;
        using task_type = ctask<result_t>;
        using handle_type = std::coroutine_handle<task_type::promise_type>;
    private:
        std::shared_ptr<state> shared_state_;
    };


    // Shared state
    // Keeps the coroutine handle
    template<is_task_result result_t>
    struct ctask<result_t>::state : public executable {
        ~state() {
            debug_print("~state");
        }
        void execute() noexcept override {
            fmt::print("[Resuming on executor thread]\n");
            handle.resume();
        }
        handle_type handle;
    };


    // Promise type
    //
    template<is_task_result result_t>
    class ctask<result_t>::coroutine_promise {
    private:
        std::weak_ptr<state> shared_state_;
    public:
        auto get_return_object() {
            debug_print("get_return_object");
            auto task = task_type{};
            shared_state_ = task.shared_state_;
            task.shared_state_->handle = handle_type::from_promise(*this);
            return task;
        }
        auto initial_suspend() {
            debug_print("initial_suspend");
            return std::suspend_never{};
        }
        auto final_suspend() noexcept {
            debug_print("final_suspend");
            get_state()->handle = nullptr;
            return std::suspend_never{};
        }
        auto return_value(result_t value) {
            debug_print("return_value");
        }
        auto unhandled_exception() {
            debug_print("unhandled_exception");
        }
        auto get_state() {
            auto state{ shared_state_.lock() };
            assert(state);
            return state;
        }
    };


    // Task scheduler
    // The awaiter schedules tasks on an executor thread
    // Deriving from std::suspend_always because it will always suspend the coroutine
    template<is_task task_t>
    class task_scheduler : public std::suspend_always {
    public:
        void await_suspend(task_t::handle_type handle) const noexcept {
            auto& promise{ handle.promise() };
            task_executor_provider::get_executor().schedule(promise.get_state());
        }
    };


    // Example 4a
    //
    inline void print_thread_id() {
        fmt::print("Current thread ID: {}\n", std::this_thread::get_id());
    }

    inline ctask<int> coro_4a() {
        print_thread_id();
        co_await task_scheduler<ctask<int>>{};
        print_thread_id();
        co_return -1;
    }

    inline void example_4a() {
        using namespace std::chrono_literals;

        fmt::print("[example_4a]\n");
        print_thread_id();
        fmt::print("[example_4a] Calling coro_4a()\n");
        auto task{ coro_4a() };
        std::this_thread::sleep_for(100ms);
        fmt::print("[example_4a] Exiting\n");
    }
}  // namespace rtc::coro::mpp_mcpp::v4a
