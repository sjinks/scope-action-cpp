# Scope Action Utilities

## Overview

This project provides a set of scope guard utilities for managing exit actions in C++. These utilities ensure that specified actions are executed when a scope is exited, regardless of how the exit occurs. The scope guards include:

- `exit_action`: Executes an action when the scope is exited.
- `fail_action`: Executes an action when the scope is exited due to an exception.
- `success_action`: Executes an action when the scope is exited normally.

These utilities are useful for ensuring that resources are properly released or actions are taken when a scope is exited.

## Features

- **exit_action**: Calls its exit function on destruction, when a scope is exited.
- **fail_action**: Calls its exit function when a scope is exited via an exception.
- **success_action**: Calls its exit function when a scope is exited normally.

## Usage

### Example Usage

```cpp
#include <iostream>
#include <scope_action.h>

void maybe_throw(bool should_throw)
{
    if (should_throw) {
        throw std::runtime_error("An error occurred");
    }
}

int main()
{
    bool exit_status = false;
    bool did_throw = false;

    auto func = [&exit_status]() { exit_status = true; };

    // Using exit_action: runs on scope exit (success or exception)
    try {
        auto guard = wwa::utils::exit_action(func);
        maybe_throw(false);
    } catch (...) {
        did_throw = true;
    }
    std::cout << "exit_action: " << exit_status << ", did_throw: " << did_throw << std::endl;

    exit_status = did_throw = false;

    // Using fail_action: runs only if an exception occurs
    try {
        auto guard = wwa::utils::fail_action(func);
        maybe_throw(true);
    } catch (...) {
        did_throw = true;
    }
    std::cout << "fail_action: " << exit_status << ", did_throw: " << did_throw << std::endl;

    exit_status = did_throw = false;

    // Using success_action: runs only if no exception occurs
    try {
        auto guard = wwa::utils::success_action(func);
        maybe_throw(false);
    } catch (...) {
        did_throw = true;
    }
    std::cout << "success_action: " << exit_status << ", did_throw: " << did_throw << std::endl;

    return 0;
}
```

## API Documentation

The documentation is available at [https://sjinks.github.io/scope-action-cpp/](https://sjinks.github.io/scope-action-cpp/).

### `exit_action`

A scope guard that calls its exit function on destruction, when a scope is exited.

```cpp
template<typename ExitFunc>
class [[nodiscard]] exit_action {
public:
    template<typename Func>
    explicit exit_action(Func&& fn)
    noexcept(std::is_nothrow_constructible_v<ExitFunc, Func> || std::is_nothrow_constructible_v<ExitFunc, Func&>);

    exit_action(exit_action&& other)
    noexcept(std::is_nothrow_move_constructible_v<ExitFunc> || std::is_nothrow_copy_constructible_v<ExitFunc>);

    ~exit_action() noexcept;

    void release() noexcept;
};
```

### `fail_action`

A scope guard that calls its exit function when a scope is exited via an exception.

```cpp
template<typename ExitFunc>
class [[nodiscard]] fail_action {
public:
    template<typename Func>
    explicit fail_action(Func&& fn)
    noexcept(std::is_nothrow_constructible_v<ExitFunc, Func> || std::is_nothrow_constructible_v<ExitFunc, Func&>);

    fail_action(fail_action&& other)
    noexcept(std::is_nothrow_move_constructible_v<ExitFunc> || std::is_nothrow_copy_constructible_v<ExitFunc>);

    ~fail_action() noexcept;

    void release() noexcept;
};
```

### `success_action`

A scope guard that calls its exit function when a scope is exited normally.

```cpp
template<typename ExitFunc>
class [[nodiscard]] success_action {
public:
    template<typename Func>
    explicit success_action(Func&& fn)
    noexcept(std::is_nothrow_constructible_v<ExitFunc, Func> || std::is_nothrow_constructible_v<ExitFunc, Func&>);

    success_action(success_action&& other)
    noexcept(std::is_nothrow_move_constructible_v<ExitFunc> || std::is_nothrow_copy_constructible_v<ExitFunc>);

    ~success_action() noexcept(noexcept(this->m_exit_function()));

    void release() noexcept;
};
```

## Building and Testing

### Prerequisites

  * C++20 compatible compiler;
  * CMake;
  * Google Test (for running tests).

### Installation

```sh
cmake -B build
cmake --build build
sudo cmake --install build
```

#### Configuration Options

| Option                  | Description                                                               | Default |
|-------------------------|---------------------------------------------------------------------------|---------|
| `BUILD_TESTS`           | Build tests                                                               | `ON`    |
| `BUILD_EXAMPLES`        | Build examples                                                            | `ON`    |
| `BUILD_DOCS`            | Build documentation                                                       | `ON`    |
| `BUILD_INTERNAL_DOCS`   | Build internal documentation                                              | `OFF`   |
| `ENABLE_MAINTAINER_MODE`| Maintainer mode (enable more compiler warnings, treat warnings as errors) | `OFF`   |
| `USE_CLANG_TIDY`        | Use `clang-tidy` during build                                             | `OFF`   |

The `BUILD_DOCS` (public API documentation) and `BUILD_INTERNAL_DOCS` (public and private API documentation) require [Doxygen](https://www.doxygen.nl/)
and, optionally, `dot` (a part of [Graphviz](https://graphviz.org/)).

The `USE_CLANG_TIDY` option requires [`clang-tidy`](https://clang.llvm.org/extra/clang-tidy/).

#### Build Types

| Build Type       | Description                                                                     |
|------------------|---------------------------------------------------------------------------------|
| `Debug`          | Build with debugging information and no optimization.                           |
| `Release`        | Build with optimization for maximum performance and no debugging information.   |
| `RelWithDebInfo` | Build with optimization and include debugging information.                      |
| `MinSizeRel`     | Build with optimization for minimum size.                                       |
| `ASAN`           | Build with AddressSanitizer enabled for detecting memory errors.                |
| `LSAN`           | Build with LeakSanitizer enabled for detecting memory leaks.                    |
| `UBSAN`          | Build with UndefinedBehaviorSanitizer enabled for detecting undefined behavior. |
| `TSAN`           | Build with ThreadSanitizer enabled for detecting data races.                    |
| `Coverage`       | Build with code coverage analysis enabled.                                      |

ASAN, LSAN, UBSAN, TSAN, and Coverage builds are only supported with GCC or clang.

Coverage build requires `gcov` (GCC) or `llvm-gcov` (clang) and [`gcovr`](https://gcovr.com/en/stable/).

### Running Tests

To run the tests, use the following command:

```sh
ctest -T test --test-dir build/test
```

or run the following binary:

```sh
./build/test/test_scope_action
```

The test binary uses [Google Test](http://google.github.io/googletest/) library.
Its behavior [can be controlled](http://google.github.io/googletest/advanced.html#running-test-programs-advanced-options)
via environment variables and/or command line flags.

Run `test_scope_action --help` for the list of available options.

## License

This project is licensed under the MIT License.

## Acknowledgements

This project is inspired by the scope guard utilities from the (experimental) [Version 3 of the C++ Extensions for Library Fundamentals](https://en.cppreference.com/w/cpp/experimental/lib_extensions_3) and the [Guidelines Support Library (GSL)](https://github.com/microsoft/GSL).
