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
#include <ranges>
#include <stop_token>
#include <string_view>
#include <thread>  // jthread
#include <utility>  // make_pair
#include <vector>


// Multi-Paradigm Programming with Modern C++, Georgy Pashkov, Packt Publishing
// Chapter 11 - Dive Deeper Into Coroutines, Implementing Continuations
//
// We want to co_await for other tasks
// Every task will keep handles to coroutines that are co_awaiting for it (we'll call these continuations)
// Once a task is finished, it will resume all these continuations; but it won't do it in a loop;
// instead, for each continuation, it will schedule one executable that will resume the coroutine in a seaparate thread
//
// Notes on implementation:
//
//   - There is a continuation manager to register and resume the continuations
//   - The continuation manager will create a resumer executable and schedule it to be run in an executor
//   - The resumer will just resume a coroutine
//
// A possible output (thread numbers, and number of threads may vary depending on the system):
//
// example|promise|ctask_scheduler|state
// example_4d [calling mul_add(16, 4, 13, 1)]
//         undefined [get_return_object]                                   (1a) auto task{ mul_add(16, 4, 13, 1) };
//         undefined [initial_suspend]                                     (1b)
//                 ctask_scheduler [await_suspend]                         (1c)
//                                 undefined [execute]                     (1d)
//         undefined [get_return_object]                                   (2a) auto p1{ mul(a, b) };
//         undefined [initial_suspend]                                     (2b)
//                 ctask_scheduler [await_suspend]                         (2c)
//         undefined [get_return_object]                                   (3a) auto p2{ mul(c, d) };
//                                 undefined [execute]                     (2d) auto p1{ mul(a, b) };
//         undefined [initial_suspend]                                     (3b) auto p2{ mul(c, d) };
//         mul(16, 4) [return_value]                                       (2e)
//         mul(16, 4) [return_value: resume all continuations]             (2f)
//         mul(16, 4) [final_suspend]                                      (2g)
//                 ctask_scheduler [await_suspend]                         (3c) auto p2{ mul(c, d) };
//         mul_add(16, 4, 13, 1) [await_transform(other_task)]                  auto v1 = co_await p1;
//                 ctask_awaiter [await_ready]                             (4i)
//                 ctask_awaiter [await_resume]                            (4l)
//         mul_add(16, 4, 13, 1) [await_transform(other_task)]                  auto v2 = co_await p2;
//                 ctask_awaiter [await_ready]                             (5i)
//                 ctask_awaiter [await_suspend]                           (5j)
//                                 undefined [execute]                     (3d) auto p2{ mul(c, d) };
//                 ctask_awaiter [await_suspend: register continuation]    (5k)
//         mul(13, 1) [return_value]                                       (3e) auto p2{ mul(c, d) };
//         mul(13, 1) [return_value: resume all continuations]             (3f)
//         mul(13, 1) [final_suspend]                                      (3g)
//                 ctask_awaiter [await_resume]                            (5l) auto v2 = co_await p2;
//         mul_add(16, 4, 13, 1) [return_value]                            (1e)
//         mul_add(16, 4, 13, 1) [return_value: resume all continuations]  (1f)
//                                 mul(13, 1) [~state]                     (3h)
//                                 mul(16, 4) [~state]                     (2h)
//         mul_add(16, 4, 13, 1) [final_suspend]                           (1g)
// example_4d [returned value from coroutine: 0x4d]
// example_4d [exiting]
//                                 mul_add(16, 4, 13, 1) [~state]          (1h)
// executor [~executor]
// executor [exiting run_thread]
// executor [exiting run_thread]
// executor [exiting run_thread]
// executor [exiting run_thread]
//
// Notes on the output:
//
//   - Race between mul_add thread and mul(16, 4) thread:
//                                 undefined [execute]  (1d)                        <--- The thread running mul_add
//         undefined [get_return_object]                (2a) auto p1{ mul(a, b) };  <--- co_awaits on the coroutine mul(16, 4),
//         undefined [initial_suspend]                  (2b)                        <--- which calls the task scheduler with its coroutine state (an executable)
//                 ctask_scheduler [await_suspend]      (2c)                             The task scheduler enqueues the coroutine state
//                                                                                       There is now a race between
//         undefined [get_return_object]                (3a) auto p2{ mul(c, d) };  <--- the thread running mul_add, which goes on to co_await on mul(13, 1),
//                                 undefined [execute]  (2d) auto p1{ mul(a, b) };  <--- and the thread executing the coroutine state for mul(16, 4)
//
//   - Task awaiter does not have to await_suspend:
//         mul(16, 4) [final_suspend]                           (2g)                        <--- By this point, p1 has finished
//                 ctask_scheduler [await_suspend]              (3c) auto p2{ mul(c, d) };
//         mul_add(16, 4, 13, 1) [await_transform(other_task)]       auto v1 = co_await p1;
//                 ctask_awaiter [await_ready]                  (4i)                        <--- The awaiter checks if the task p1 is ready,
//                 ctask_awaiter [await_resume]                 (4l)                        <--- and resumes
//
//   - Task awaiter does have to await_suspend:
//         mul_add(16, 4, 13, 1) [await_transform(other_task)]                auto v2 = co_await p2;
//                 ctask_awaiter [await_ready]                           (5i)                         <--- The awaiter checks if the task p2 is ready,
//                 ctask_awaiter [await_suspend]                         (5j)                         <--- and suspends,
//                                 undefined [execute]                   (3d) auto p2{ mul(c, d) };
//                 ctask_awaiter [await_suspend: register continuation]  (5k)
//         mul(13, 1) [return_value]                                     (3e) auto p2{ mul(c, d) };   <--- because p2 finishes sometime later
//         mul(13, 1) [return_value: resume all continuations]           (3f)                         <--- Before finishing, p2 resumes all continuations,
//         mul(13, 1) [final_suspend]                                    (3g)                         
//                 ctask_awaiter [await_resume]                          (5l) auto v2 = co_await p2;  <--- what makes the awaiter to resume
// 
//   - Coroutine steps:
//       (a) get_return_object
//       (b) initial_suspend
//       (c) ctask_scheduler [await_suspend]
//       (d) state [execute]
//       (e) return_value
//       (f) return_value: resume all continuations
//       (g) final_suspend
//       (h) state [~state]
//
//   - ctask_awaiter steps:
//       (i) await_ready
//       (j) await_suspend (optional)
//       (k) await_suspend: register continuation (optional)
//       (l) await_resume


namespace rtc::coro::mpp_mcpp::v4d {
    // Debug print helper
    struct indentation {
        std::string::size_type level;
    };

    inline void debug_print(std::string_view name, std::string_view text, const indentation& indentation = {}) {
        static std::mutex mtx;
        std::lock_guard lock{ mtx };
        fmt::print("{}{} [{}]\n",
            std::string(indentation.level * 8, ' '),
            (name.empty() ? "undefined" : name),
            text
        );
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
            debug_print("executor", "~executor");
            std::ranges::for_each(threads_, [](std::jthread& t) { t.request_stop(); });
            std::ranges::for_each(threads_, [](std::jthread& t) { t.join(); });
        }
        void schedule(executable_ptr ex) {
            {
                std::lock_guard lock{ mutex_ };
                queue_.push(std::move(ex));
            }
            cva_.notify_one();
        }
    private:
        void run_thread(std::stop_token stoken) {
            while (true) {
                std::unique_lock<std::mutex> lock{ mutex_ };
                if (queue_.empty()) {
                    cva_.wait(lock, stoken, [this]() {
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
            debug_print("executor", "exiting run_thread");
        }
        std::vector<std::jthread> threads_;
        std::mutex mutex_;
        std::queue<executable_ptr> queue_;
        std::condition_variable_any cva_;
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

    using ctask_executor_provider = executor_provider<>;


    // Continuation
    // Pair of coroutine_handle and executor
    struct continuation {
        using handle_type = std::coroutine_handle<>;

        handle_type handle_;
        executor* executor_;
    };


    // Continuation manager
    // Register continuations
    // Resumes a collection of coroutine handles at once, possibly on a different executor each
    class continuation_manager {
    public:
        using handle_type = std::coroutine_handle<>;

        void register_continuation(continuation c) {
            std::lock_guard lock{ mutex_ };
            continuations_.push_back(std::move(c));
        }
        void resume_all_continuations() {
            std::lock_guard lock{ mutex_ };
            std::ranges::for_each(continuations_, [](const auto& c) {
                resume_continuation(c);
            });
        }
    private:
        struct resumer : public executable {
            handle_type handle_;
            resumer(handle_type handle)
                : handle_(handle)
            {}
            void execute() noexcept override {
                handle_.resume();
            }
        };
        static void resume_continuation(const auto& c) {
            c.executor_->schedule(std::make_shared<resumer>(c.handle_));
        }

        std::mutex mutex_;
        std::vector<continuation> continuations_;
    };


    // Concepts
    //
    template <typename T>
    concept is_executor_provider = std::is_same_v<decltype(T::get_executor()), executor&>;

    // Task result
    // At the moment, a coroutine based task cannot return void
    template <typename T>
    concept is_task_result =
        std::is_copy_constructible_v<T> ||
        std::is_move_constructible_v<T> ||
        // std::is_void_v<T> ||
        std::is_reference_v<T>;

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
        class state;
    public:
        auto get_result() const {
            return shared_state_->get_result().get();
        }
        auto wait() const {
            shared_state_->get_result().wait();
        }
        bool ready() const {
            return
                shared_state_->get_result().wait_for(std::chrono::duration<size_t>::zero()) ==
                std::future_status::ready;
        }
        void register_continuation(std::coroutine_handle<> handle) {
            auto& continuation_manager{ shared_state_->get_continuation_manager() };
            continuation_manager.register_continuation({handle, &ctask_executor_provider::get_executor()});
            if (ready()) {
                continuation_manager.resume_all_continuations();
            }
        }

        using promise_type = coroutine_promise;
        using task_type = ctask<result_t>;
        using handle_type = std::coroutine_handle<task_type::promise_type>;
    private:
        ctask(handle_type handle)
            : shared_state_{ std::make_shared<state>(handle) }
        {}

        std::shared_ptr<state> shared_state_;
    };


    // Task scheduler
    // The awaiter schedules tasks on an executor thread
    template <is_task task_t>
    class ctask_scheduler : public std::suspend_always {
    public:
        void await_suspend(task_t::handle_type handle) const noexcept {
            debug_print("ctask_scheduler", "await_suspend", indentation{ 2 });
            auto& promise{ handle.promise() };
            ctask_executor_provider::get_executor().schedule(promise.get_state());
        }
    };


    template <is_task task_t>
    class ctask_awaiter {
    public:
        ctask_awaiter(task_t t)
            : task_{ std::move(t) }
        {}

        // Compiler-generated code will invoke await_ready before invoking await_suspend
        bool await_ready() const noexcept {
            debug_print("ctask_awaiter", "await_ready", indentation{ 2 });
            return task_.ready();
        }
        bool await_suspend(std::coroutine_handle<> handle) noexcept {
            debug_print("ctask_awaiter", "await_suspend", indentation{ 2 });
            if (task_.ready()) {
                return false;
            }
            debug_print("ctask_awaiter", "await_suspend: register continuation", indentation{ 2 });
            task_.register_continuation(handle);
            return true;
        }
        // Compiler-generated code will invoke await_resume after the coroutine has been resumed but before any code has been executed
        // The return value of this function is the return value of the co_await operator, and it can be any type we want
        // If we return the result of the task, the user will be able to call co_await and use the result without having to call get
        // E.g.
        // task<int> task = calculate();
        // int x = co_await task;
        auto await_resume() const {
            debug_print("ctask_awaiter", "await_resume", indentation{ 2 });
            return task_.get_result();
        }
    private:
        task_t task_;
    };


    // Shared state
    // Shared between all instances of a task
    // Keeps the coroutine handle
    template <is_task_result result_t>
    class ctask<result_t>::state : public executable {
    public:
        state(handle_type handle)
            : handle_{ handle }
            , shared_future_{ result_.get_future() }
        {}
        ~state() {
            debug_print(name_, "~state", indentation{ 4 });
        }
        void execute() noexcept override {
            debug_print(name_, "execute", indentation{ 4 });
            handle_.resume();
        }
        void set_handle(handle_type handle) {
            handle_ = std::move(handle);
        }
        auto get_name() {
            return name_;
        }
        auto set_name(std::string name) {
            name_ = std::move(name);
        }
        auto get_result() {
            return shared_future_;
        }
        void set_result(result_t&& value) {
            result_.set_value(std::forward<result_t>(value));
        }
        auto& get_continuation_manager() {
            return continuation_manager_;
        }
        void set_exception(std::exception_ptr exception_ptr) {
            result_.set_exception(std::move(exception_ptr));
        }

    private:
        handle_type handle_;
        std::string name_;
        std::promise<result_t> result_;  // nothing to do with coroutine promise
        std::shared_future<result_t> shared_future_;
        continuation_manager continuation_manager_;
    };


    // Promise type
    //
    template <is_task_result result_t>
    class ctask<result_t>::coroutine_promise {
    public:
        auto get_return_object() {
            debug_print("", "get_return_object", indentation{ 1 });
            auto ret{ task_type{ handle_type::from_promise(*this)  } };
            shared_state_ = ret.shared_state_;
            return ret;
        }
        auto initial_suspend() {
            debug_print("", "initial_suspend", indentation{ 1 });
            return ctask_scheduler<task_type>{};
        }
        auto final_suspend() noexcept {
            if (auto state{ shared_state_.lock() }) {
                debug_print(state->get_name(), "final_suspend", indentation{1});
                state->set_handle(nullptr);
            }
            return std::suspend_never{};
        }
        auto return_value(result_t&& value) {
            if (auto state{ shared_state_.lock() }) {
                debug_print(state->get_name(), "return_value", indentation{1});
                state->set_result(std::forward<result_t>(value));
                debug_print(state->get_name(), "return_value: resume all continuations", indentation{1});
                state->get_continuation_manager().resume_all_continuations();
            }
        }
        auto unhandled_exception() {
            if (auto state{ shared_state_.lock() }) {
                debug_print(state->get_name(), "unhandled_exception", indentation{1});
                state->set_exception(std::current_exception());
            }
        }
        auto await_transform(std::string name) {
            if (auto state{ shared_state_.lock() }) {
                state->set_name(std::move(name));
            }
            return std::suspend_never{};
        }
        template <is_task other_task_t>
        auto await_transform(other_task_t other_task) {
            if (auto state{ shared_state_.lock() }) {
                debug_print(state->get_name(), "await_transform(other_task)", indentation{1});
            }
            return ctask_awaiter<other_task_t>{ std::move(other_task) };
        }
        auto get_state() {
            auto state{ shared_state_.lock() };
            assert(state);
            return state;
        }
    private:
        std::weak_ptr<state> shared_state_;
    };


    // Example 4d
    //
    inline ctask<int> mul(int a, int b) {
        co_await fmt::format("mul({}, {})", a, b);
        co_return a * b;
    }

    inline ctask<int> mul_add(int a, int b, int c, int d) {
        co_await fmt::format("mul_add({}, {}, {}, {})", a, b, c, d);
        auto p1{ mul(a, b) };  // start a task; task will be executed in a separate thread, and the main thread will resume its execution here
        auto p2{ mul(c, d) };  // start a second task; again, this task will be executed in a thread, and the main thread will continue from here
        auto v1 = co_await p1;  // suspend until the first result is available, and again
        auto v2 = co_await p2;  // suspend until the second result is available
        co_return v1 + v2;
    }

    inline void example_4d() {
        fmt::print("example|promise|ctask_scheduler|state\n");
        debug_print("example_4d", "calling mul_add(16, 4, 13, 1)");
        auto task{ mul_add(16, 4, 13, 1) };
        debug_print("example_4d", fmt::format("returned value from coroutine: {:#x}", task.get_result()));
        debug_print("example_4d", "exiting");
    }
}  // namespace rtc::coro::mpp_mcpp::v4d
