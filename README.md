# coro

Some examples using C++20 coroutines.

Most of the code and diagrams in this project have been taken or are based in:
- Daniela Engert's [*CppInAction*](https://github.com/DanielaE/CppInAction) GitHub project.
  This is a demo for the [*Contemporary C++ In Action*](https://www.youtube.com/watch?v=J_1-Au2MX6Y) talk (Meeting C++ 2022).
- Georgy Paskkov's [*Multi-Paradigm Programming with Modern C++*](https://www.packtpub.com/product/multi-paradigm-programming-with-modern-c-video/9781839212864) book (Packt Publishing, june 2020).

For example, the code in `MPP_MCpp` folder has been taken from chapters 10 (Introduction to Coroutines) and 11 (Dive Deeper Into Coroutines) of the *Multi-Paradigm Programming with Modern C++* book. I've just made some modifications:
- Changed `std::atomic` and spin locks for `std::stop_source`.
- Changed normal threads for `std::jthreads`.
- Used shorter notations for traits (e.g. `std::is_copy_constructible_v<T>` instead of `std::is_copy_constructible<T>::value`).
- Renamed a few things (e.g. `continuation_manager` instead of `executor_resumer`).
- Added more debug output, and lots of comments explaining what code was being called, in which order, and so on.


Likewise, the `doc` folder contains:
- `cpp_in_action-*.png`: sequence diagrams based in the *CppInAction* project.
- `example_*.png`: class diagrams based in the *Multi-Paradigm Programming with Modern C++* book.
