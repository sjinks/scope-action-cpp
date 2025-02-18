// Adapted from https://en.cppreference.com/w/cpp/experimental/scope_exit#Example

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string_view>

#include "scope_action.h"

namespace {

void print_exit_status(std::string_view name, bool exit_status, bool did_throw)
{
    std::cout << name << ":\n";
    std::cout << "  Throwed exception  " << (did_throw ? "yes" : "no") << "\n";
    std::cout << "  Exit status        " << (exit_status ? "finished" : "pending") << "\n\n";
}

void maybe_throw()
{
    // NOLINTNEXTLINE(concurrency-mt-unsafe, cert-msc50-cpp)
    if (std::rand() >= RAND_MAX / 2) {
        throw std::exception{};
    }
}

}  // namespace

int main()
{
    bool exit_status = false;
    bool did_throw   = false;

    // NOLINTNEXTLINE(cert-msc51-cpp)
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    // Manual handling at "end of scope"
    try {
        maybe_throw();
        exit_status = true;
    }
    catch (...) {
        did_throw = true;
    }
    print_exit_status("Manual handling", exit_status, did_throw);

    exit_status = did_throw = false;
    //! [Using exit_action: runs on scope exit (success or exception)]
    try {
        auto guard = wwa::utils::exit_action{[&exit_status]() { exit_status = true; }};
        maybe_throw();
    }
    catch (...) {
        did_throw = true;
    }
    print_exit_status("exit_action", exit_status, did_throw);
    //! [Using exit_action: runs on scope exit (success or exception)]

    exit_status = did_throw = false;
    //! [Using fail_action: runs only if an exception occurs]
    try {
        auto guard = wwa::utils::fail_action{[&exit_status]() { exit_status = true; }};
        maybe_throw();
    }
    catch (...) {
        did_throw = true;
    }
    print_exit_status("fail_action", exit_status, did_throw);
    //! [Using fail_action: runs only if an exception occurs]

    exit_status = did_throw = false;
    //! [Using success_action: runs only if no exception occurs]
    try {
        auto guard = wwa::utils::success_action{[&exit_status] { exit_status = true; }};
        maybe_throw();
    }
    catch (...) {
        did_throw = true;
    }
    print_exit_status("success_action", exit_status, did_throw);
    //! [Using success_action: runs only if no exception occurs]

    return 0;
}
