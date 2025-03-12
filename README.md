# Scope Action Utilities

[![Build and Test](https://github.com/sjinks/scope-action-cpp/actions/workflows/ci.yml/badge.svg)](https://github.com/sjinks/scope-action-cpp/actions/workflows/ci.yml)
[![CodeQL](https://github.com/sjinks/scope-action-cpp/actions/workflows/codeql.yml/badge.svg)](https://github.com/sjinks/scope-action-cpp/actions/workflows/codeql.yml)
[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=sjinks_scope-action-cpp&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=sjinks_scope-action-cpp)

## Overview

This project provides a set of utilities for managing exit actions and resource handling in C++. These utilities ensure that specified actions are executed when a scope is exited, regardless of (or depending on) how the exit occurs. The scope guards include:

- `exit_action`: Executes an action when the scope is exited.
- `fail_action`: Executes an action when the scope is exited due to an exception.
- `success_action`: Executes an action when the scope is exited normally.
- `unique_resource`: Manages a resource through a handle and disposes of that resource upon destruction (scope exit).

These utilities are useful for ensuring that resources are properly released or actions are taken when a scope is exited.

## Features

- **exit_action**: Calls its exit function on destruction, when a scope is exited.
- **fail_action**: Calls its exit function when a scope is exited via an exception.
- **success_action**: Calls its exit function when a scope is exited normally.
- **unique_resource**: Manages a resource with a custom deleter, ensuring the resource is released when the scope is exited.

## Usage

### Example Usage

#### Scope Guards

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

#### `unique_resource`

```cpp
#include <cstdio>
#include <unique_resource.h>

int main()
{
    auto file = wwa::utils::make_unique_resource_checked(
        std::fopen("potentially_nonexistent_file.txt", "r"), nullptr, std::fclose
    );

    if (file.get() != nullptr) {
        std::puts("The file exists.\n");
    }
    else {
        std::puts("The file does not exist.\n");
    }

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

### `unique_resource`

A universal RAII resource handle wrapper for resource handles that owns and manages a resource through a handle and disposes of that resource upon destruction.

```cpp
template<typename Resource, typename Deleter>
class [[nodiscard]] unique_resource {
public:
    unique_resource();

    template<typename Res, typename Del>
    unique_resource(Res&& r, Del&& d) noexcept(
        (std::is_nothrow_constructible_v<WrappedResource, Res> || std::is_nothrow_constructible_v<WrappedResource, Res&>) &&
        (std::is_nothrow_constructible_v<Deleter, Del> || std::is_nothrow_constructible_v<Deleter, Del&>)
    );

    unique_resource(unique_resource&& other) noexcept(std::is_nothrow_move_constructible_v<Resource> && std::is_nothrow_move_constructible_v<Deleter>);
    ~unique_resource() noexcept;

    unique_resource& operator=(unique_resource&& other) noexcept(
        std::is_nothrow_move_assignable_v<Resource> && std::is_nothrow_move_assignable_v<Deleter>
    )

    void release() noexcept;
    void reset() noexcept;

    template<typename Res>
    void reset(Res&& r);

    const Resource& get() const noexcept;
    const Deleter& get_deleter() const noexcept;

    std::add_lvalue_reference_t<std::remove_pointer_t<Resource>> operator*() const noexcept;
    Resource operator->() const noexcept
};

template<typename Resource, typename Deleter, typename Invalid = std::decay_t<Resource>>
unique_resource<std::decay_t<Resource>, std::decay_t<Deleter>>
make_unique_resource_checked(Resource&& r, const Invalid& invalid, Deleter&& d) noexcept(
    std::is_nothrow_constructible_v<std::decay_t<Resource>, Resource> &&
    std::is_nothrow_constructible_v<std::decay_t<Deleter>, Deleter>
);
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
