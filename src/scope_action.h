#ifndef A2100C2E_3B3D_4875_ACA4_BFEF2E5B6120
#define A2100C2E_3B3D_4875_ACA4_BFEF2E5B6120

/**
 * @file
 * @brief Scope guard utilities for managing exit actions.
 *
 * This file provides the implementation of scope guards that execute specified actions
 * when a scope is exited. The scope guards include:
 * - `exit_action`: Executes an action when the scope is exited.
 * - `fail_action`: Executes an action when the scope is exited due to an exception.
 * - `success_action`: Executes an action when the scope is exited normally.
 *
 * These utilities are useful for ensuring that resources are properly released or
 * actions are taken when a scope is exited, regardless of how the exit occurs.
 *
 * Examples:
 * @snippet{trimleft} scope_action.cpp Using exit_action: runs on scope exit (success or exception)
 * @snippet{trimleft} scope_action.cpp Using fail_action: runs only if an exception occurs
 * @snippet{trimleft} scope_action.cpp Using success_action: runs only if no exception occurs
 *
 * @note Constructing these scope guards with dynamic storage duration might lead to
 * unexpected behavior.
 */

#include <concepts>
#include <exception>
#include <limits>
#include <type_traits>
#include <utility>

/** @brief Library namespace. */
namespace wwa::utils {

/// @cond INTERNAL

namespace detail {

template<typename Self, typename What, typename From>
concept can_construct_from = !std::is_same_v<std::remove_cvref_t<From>, Self> && std::constructible_from<What, From>;

template<typename Self, typename What, typename From>
concept can_move_construct_from_noexcept = can_construct_from<Self, What, From> && !std::is_lvalue_reference_v<From> &&
                                           std::is_nothrow_constructible_v<What, From>;

template<typename T>
T&& conditional_forward(T&& t, std::true_type)
{
    return std::forward<T>(t);
}

template<typename T>
const T& conditional_forward(T&& t, std::false_type)  // NOLINT(cppcoreguidelines-missing-std-forward)
{
    return t;
}

}  // namespace detail

/// @endcond

/**
 * @brief A scope guard that calls its exit function on destruction, when a scope is exited.
 *
 * An `exit_action` may be either active (i.e., it will calls its exit function on destruction),
 * or inactive (it does nothing on destruction). An `exit_action` is active after construction from an exit function.
 *
 * An `exit_action` becomes inactive by calling `release()` or a move constructor. An inactive `exit_action`
 * may also be obtained by initializing with another inactive `exit_action`. Once an `exit_action` is inactive,
 * it cannot become active again.
 *
 * Usage example:
 * @snippet{trimleft} scope_action.cpp Using exit_action: runs on scope exit (success or exception)
 *
 * @tparam ExitFunc Exit function type. Func is either a
 * [Destructible](https://en.cppreference.com/w/cpp/named_req/Destructible)
 * [FunctionObject](https://en.cppreference.com/w/cpp/named_req/FunctionObject) type, or an lvalue reference to a
 * [FunctionObject](https://en.cppreference.com/w/cpp/named_req/FunctionObject) or function.
 * @see https://en.cppreference.com/w/cpp/experimental/scope_exit
 * @see https://github.com/microsoft/GSL/blob/main/docs/headers.md#user-content-H-util-final_action
 * @note Constructing an `exit_action` of dynamic storage duration might lead to unexpected behavior.
 * @note If the exit function stored in an `exit_action` object refers to a local variable of the function where it is
 * defined (e.g., as a lambda capturing the variable by reference), and that variable is used as a return operand in
 * that function, that variable might have already been returned when the `exit_action`'s destructor executes, calling
 * the exit function. This can lead to surprising behavior.
 */
template<typename ExitFunc>
class [[nodiscard("The object must be used to ensure the exit function is called on scope exit.")]] exit_action {
public:
    /**
     * @brief Constructs a new @a exit_action from an exit function of type @a Func.
     *
     * Initializes the exit function with a function or function object `fn`. The constructed `exit_action` is active.
     * If `Func` is not an lvalue reference type, and `std::is_nothrow_constructible_v<ExitFunc, Func>` is `true`, the
     * stored exit function is initialized with `std::forward<Func>(fn)`; otherwise it is initialized with `fn`. If
     * initialization of the stored exit function throws an exception, calls `fn()`.
     *
     * This overload participates in overload resolution only if:
     *   - `std::is_same_v<std::remove_cvref_t<Func>, exit_action>` is `false`, and
     *   - `std::is_constructible_v<ExitFunc, Func>` is `true`.
     *
     * @tparam Func Exit function type. Must be constructible from @a ExitFunc.
     * @param fn Exit function.
     * @throw anything Any exception thrown during the initialization of the stored exit function.
     * @see https://en.cppreference.com/w/cpp/experimental/scope_exit/scope_exit
     */
    template<typename Func>
    requires(detail::can_construct_from<exit_action, ExitFunc, Func>)
    explicit exit_action(
        Func&& fn
    ) noexcept(std::is_nothrow_constructible_v<ExitFunc, Func> || std::is_nothrow_constructible_v<ExitFunc, Func&>)
    try
        : m_exit_function(
              detail::conditional_forward(
                  std::forward<Func>(fn),
                  std::bool_constant<
                      std::is_nothrow_constructible_v<ExitFunc, Func> && !std::is_lvalue_reference_v<Func>>()
              )
          )
    {}
    catch (...) {
        fn();
    }

    /**
     * @brief Constructs a new @a exit_action from an exit function of type @a Func.
     *
     * Initializes the exit function with a function or function object `fn`. The constructed `exit_action` is active.
     * The stored exit function is initialized with `std::forward<Func>(fn)`.
     *
     * This overload participates in overload resolution only if:
     *   - `std::is_same_v<std::remove_cvref_t<Func>, exit_action>` is `false`, and
     *   - `std::is_lvalue_reference_v<Func>` is `false`, and
     *   - `std::is_nothrow_constructible_v<ExitFunc, Func>` is `true`.
     *
     * @tparam Func Exit function type. Must be constructible from @a ExitFunc.
     * @param fn Exit function.
     * @see https://en.cppreference.com/w/cpp/experimental/scope_exit/scope_exit
     */
    template<typename Func>
    requires(detail::can_move_construct_from_noexcept<exit_action, ExitFunc, Func>)
    explicit exit_action(Func&& fn) noexcept : m_exit_function(std::forward<Func>(fn))
    {}

    /**
     * @brief Move constructor.
     *
     * Initializes the stored exit function with the one in `other`. The constructed `exit_action` is active
     * if and only if `other` is active before the construction.
     *
     * If `std::is_nothrow_move_constructible_v<ExitFunc>` is true, initializes stored exit function (denoted by
     * `exitfun`) with `std::forward<ExitFunc>(other.exitfun)`, otherwise initializes it with `other.exitfun`.
     *
     * After successful move construction, `other` becomes inactive.
     *
     * This overload participates in overload resolution only if:
     *   - `std::is_nothrow_move_constructible_v<ExitFunc>` is `true`, or
     *   - `std::is_copy_constructible_v<ExitFunc>` is `true`.
     *
     * @param other `exit_action` to move from.
     * @throw anything Any exception thrown during the initialization of the stored exit function.
     * @see https://en.cppreference.com/w/cpp/experimental/scope_exit/scope_exit
     */
    exit_action(
        exit_action&& other
    ) noexcept(std::is_nothrow_move_constructible_v<ExitFunc> || std::is_nothrow_copy_constructible_v<ExitFunc>)
    requires(std::is_nothrow_move_constructible_v<ExitFunc> || std::is_copy_constructible_v<ExitFunc>)
        : m_exit_function(
              detail::conditional_forward(
                  std::forward<ExitFunc>(other.m_exit_function),
                  std::bool_constant<std::is_nothrow_move_constructible_v<ExitFunc>>()
              )
          ),
          m_is_armed(other.m_is_armed)
    {
        other.release();
    }

    /** @cond */
    /** @brief @a exit_action is not @a CopyConstructible */
    exit_action(const exit_action&)            = delete;
    /** @brief @a exit_action is not @a CopyAssignable */
    exit_action& operator=(const exit_action&) = delete;
    /** @brief @a exit_action is not @a MoveAssignable */
    exit_action& operator=(exit_action&&)      = delete;
    /** @endcond */

    /**
     * @brief Calls the exit function if @a m_is_armed is active, then destroys the object.
     * @see https://en.cppreference.com/w/cpp/experimental/scope_exit/%7Escope_exit
     */
    ~exit_action() noexcept
    {
        if (this->m_is_armed) {
            this->m_exit_function();
        }
    }

    /**
     * @brief Makes the @a exit_action object inactive.
     *
     * Once an @a exit_action is inactive, it cannot become active again, and it will not call its exit function upon
     * destruction.
     *
     * @see https://en.cppreference.com/w/cpp/experimental/scope_exit/release
     */
    void release() noexcept { this->m_is_armed = false; }

private:
    ExitFunc m_exit_function;  ///< The stored exit function.
    bool m_is_armed = true;    ///< Whether this `exit_action` is active.
};

/**
 * @brief Deduction guide for @a exit_action.
 *
 * @tparam ExitFunc Exit function type.
 */
template<typename ExitFunc>
exit_action(ExitFunc) -> exit_action<ExitFunc>;

/**
 * @brief A scope guard that calls its exit function when a scope is exited via an exception.
 *
 * Like `exit_action`, a `fail_action` may be active or inactive. A `fail_action` is active after construction from an
 * exit function.
 *
 * An `fail_action` becomes inactive by calling `release()` or a move constructor. An inactive `fail_action`
 * may also be obtained by initializing with another inactive `fail_action`. Once an `fail_action` is inactive,
 * it cannot become active again.
 *
 * Usage example:
 * @snippet{trimleft} scope_action.cpp Using fail_action: runs only if an exception occurs
 *
 * @tparam ExitFunc Exit function type. Func is either a
 * [Destructible](https://en.cppreference.com/w/cpp/named_req/Destructible)
 * [FunctionObject](https://en.cppreference.com/w/cpp/named_req/FunctionObject) type, or an lvalue reference to a
 * [FunctionObject](https://en.cppreference.com/w/cpp/named_req/FunctionObject) or function.
 * @see https://en.cppreference.com/w/cpp/experimental/scope_fail
 * @note Constructing a `fail_action` of dynamic storage duration might lead to unexpected behavior.
 * @note Constructing a `fail_action` from another `fail_action` created in a different thread might also lead to
 * unexpected behavior since the count of uncaught exceptions obtained in different threads may be compared during
 * the destruction.
 */
template<typename ExitFunc>
class [[nodiscard("The object must be used to ensure the exit function is called due to an exception.")]] fail_action {
public:
    /**
     * @brief Constructs a new @a fail_action from an exit function of type @a Func.
     *
     * Initializes the exit function with a function or function object, and initializes
     * the counter of uncaught exceptions as if with `std::uncaught_exceptions()`.
     * The constructed `fail_action` is active.
     *
     * If `Func` is not an lvalue reference type, and `std::is_nothrow_constructible_v<ExitFunc, Func>` is `true`, the
     * stored exit function is initialized with `std::forward<Func>(fn)`; otherwise it is initialized with `fn`. If
     * initialization of the stored exit function throws an exception, calls `fn()`.
     *
     * This overload participates in overload resolution only if:
     *   - `std::is_same_v<std::remove_cvref_t<Func>, fail_action>` is `false`, and
     *   - `std::is_constructible_v<ExitFunc, Func>` is `true`.
     *
     * @tparam Func Exit function type. Must be constructible from @a ExitFunc.
     * @param fn Exit function.
     * @throw anything Any exception thrown during the initialization of the stored exit function.
     * @see https://en.cppreference.com/w/cpp/experimental/scope_fail/scope_fail
     */
    template<typename Func>
    requires(detail::can_construct_from<fail_action, ExitFunc, Func>)
    explicit fail_action(
        Func&& fn
    ) noexcept(std::is_nothrow_constructible_v<ExitFunc, Func> || std::is_nothrow_constructible_v<ExitFunc, Func&>)
    try
        : m_exit_function(
              detail::conditional_forward(
                  std::forward<Func>(fn),
                  std::bool_constant<
                      std::is_nothrow_constructible_v<ExitFunc, Func> && !std::is_lvalue_reference_v<Func>>()
              )
          )
    {}
    catch (...) {
        fn();
    }

    /**
     * @brief Constructs a new @a fail_action from an exit function of type @a Func.
     *
     * Initializes the exit function with a function or function object `fn`. The constructed `fail_action` is active.
     * The stored exit function is initialized with `std::forward<Func>(fn)`.
     *
     * This overload participates in overload resolution only if:
     *   - `std::is_same_v<std::remove_cvref_t<Func>, fail_action>` is `false`, and
     *   - `std::is_lvalue_reference_v<Func>` is `false`, and
     *   - `std::is_nothrow_constructible_v<ExitFunc, Func>` is `true`.
     *
     * @tparam Func Exit function type. Must be constructible from @a ExitFunc.
     * @param fn Exit function.
     * @see https://en.cppreference.com/w/cpp/experimental/scope_fail/scope_fail
     */
    template<typename Func>
    requires(detail::can_move_construct_from_noexcept<fail_action, ExitFunc, Func>)
    explicit fail_action(Func&& fn) noexcept : m_exit_function(std::forward<Func>(fn))
    {}

    /**
     * @brief Move constructor.
     *
     * Initializes the stored exit function with the one in `other`, and initializes the counter of
     * uncaught exceptions with the one in `other`. The constructed `fail_action` is active
     * if and only if `other` is active before the construction.
     *
     * If `std::is_nothrow_move_constructible_v<ExitFunc>` is true, initializes stored exit function (denoted by
     * `exitfun`) with `std::forward<ExitFunc>(other.exitfun)`, otherwise initializes it with `other.exitfun`.
     *
     * After successful move construction, `other.release()` is called and `other` becomes inactive.
     *
     * This overload participates in overload resolution only if:
     *   - `std::is_nothrow_move_constructible_v<ExitFunc>` is `true`, or
     *   - `std::is_copy_constructible_v<ExitFunc>` is `true`.
     *
     * @param other `fail_action` to move from.
     * @throw anything Any exception thrown during the initialization of the stored exit function.
     * @see https://en.cppreference.com/w/cpp/experimental/scope_fail/scope_fail
     */
    fail_action(
        fail_action&& other
    ) noexcept(std::is_nothrow_move_constructible_v<ExitFunc> || std::is_nothrow_copy_constructible_v<ExitFunc>)
    requires(std::is_nothrow_move_constructible_v<ExitFunc> || std::is_copy_constructible_v<ExitFunc>)
        : m_exit_function(
              detail::conditional_forward(
                  std::forward<ExitFunc>(other.m_exit_function),
                  std::bool_constant<std::is_nothrow_move_constructible_v<ExitFunc>>()
              )
          ),
          m_uncaught_exceptions_count(other.m_uncaught_exceptions_count)
    {
        other.release();
    }

    /** @cond */
    /** @brief @a fail_action is not @a CopyConstructible */
    fail_action(const fail_action&)            = delete;
    /** @brief @a fail_action is not @a CopyAssignable */
    fail_action& operator=(const fail_action&) = delete;
    /** @brief @a fail_action is not @a MoveAssignable */
    fail_action& operator=(fail_action&&)      = delete;
    /** @endcond */

    /**
     * @brief Calls the exit function if the scope is exited via an exception and destroys the object.
     *
     * Calls the exit function if the result of `std::uncaught_exceptions()` is greater than the counter of uncaught
     * exceptions (typically on stack unwinding) and the `fail_action` is active; then destroys the object.
     *
     * @see https://en.cppreference.com/w/cpp/experimental/scope_fail/%7Escope_fail
     */
    ~fail_action() noexcept
    {
        if (std::uncaught_exceptions() > this->m_uncaught_exceptions_count) {
            this->m_exit_function();
        }
    }

    /**
     * @brief Makes the @a fail_action object inactive.
     *
     * Once an @a fail_action is inactive, it cannot become active again, and it will not call its exit function upon
     * destruction.
     *
     * @note @a release() may be either manually called or automatically called by `fail_action`'s move constructor.
     * @see https://en.cppreference.com/w/cpp/experimental/scope_fail/release
     */
    void release() noexcept { this->m_uncaught_exceptions_count = std::numeric_limits<int>::max(); }

private:
    ExitFunc m_exit_function;                                      ///< The stored exit function.
    int m_uncaught_exceptions_count = std::uncaught_exceptions();  ///< The counter of uncaught exceptions.
};

/**
 * @brief Deduction guide for @a fail_action.
 *
 * @tparam ExitFunc Exit function type.
 */
template<typename ExitFunc>
fail_action(ExitFunc) -> fail_action<ExitFunc>;

/**
 * @brief A scope guard that calls its exit function when a scope is exited normally.
 *
 * Like `exit_action`, a `success_action` may be active or inactive. A `success_action` is active after construction
 * from an exit function.
 *
 * An `success_action` becomes inactive by calling `release()` or a move constructor. An inactive `success_action`
 * may also be obtained by initializing with another inactive `success_action`. Once an `success_action` is inactive,
 * it cannot become active again.
 *
 * Usage example:
 * @snippet{trimleft} scope_action.cpp Using success_action: runs only if no exception occurs
 *
 * @tparam ExitFunc Exit function type. Func is either a
 * [Destructible](https://en.cppreference.com/w/cpp/named_req/Destructible)
 * [FunctionObject](https://en.cppreference.com/w/cpp/named_req/FunctionObject) type, or an lvalue reference to a
 * [FunctionObject](https://en.cppreference.com/w/cpp/named_req/FunctionObject) or function.
 * @see https://en.cppreference.com/w/cpp/experimental/scope_success
 * @note Constructing a `success_action` of dynamic storage duration might lead to unexpected behavior.
 * @note Constructing a `success_action` from another `success_action` created in a different thread might also lead to
 * unexpected behavior since the count of uncaught exceptions obtained in different threads may be compared during
 * the destruction.
 * @note If the exit function stored in an `success_action` object refers to a local variable of the function where it
 * is defined (e.g., as a lambda capturing the variable by reference), and that variable is used as a return operand in
 * that function, that variable might have already been returned when the `success_action`'s destructor executes,
 * calling the exit function. This can lead to surprising behavior.
 */
template<typename ExitFunc>
class [[nodiscard(
    "The object must be used to ensure the exit function is called on a clean scope exit."
)]] success_action {
public:
    /**
     * @brief Constructs a new @a success_action from an exit function of type @a Func.
     *
     * Initializes the exit function with a function or function object, and initializes
     * the counter of uncaught exceptions as if with `std::uncaught_exceptions()`.
     * The constructed `success_action` is active.
     *
     * If `Func` is not an lvalue reference type, and `std::is_nothrow_constructible_v<ExitFunc, Func>` is `true`, the
     * stored exit function is initialized with `std::forward<Func>(fn)`; otherwise it is initialized with `fn`. If
     * initialization of the stored exit function throws an exception, calls `fn()`.
     *
     * This overload participates in overload resolution only if:
     *   - `std::is_same_v<std::remove_cvref_t<Func>, success_action>` is `false`, and
     *   - `std::is_constructible_v<ExitFunc, Func>` is `true`.
     *
     * @tparam Func Exit function type. Must be constructible from @a ExitFunc.
     * @param fn Exit function.
     * @throw anything Any exception thrown during the initialization of the stored exit function.
     * @see https://en.cppreference.com/w/cpp/experimental/scope_success/scope_success
     */
    template<typename Func>
    requires(detail::can_construct_from<success_action, ExitFunc, Func>)
    explicit success_action(
        Func&& fn
    ) noexcept(std::is_nothrow_constructible_v<ExitFunc, Func> || std::is_nothrow_constructible_v<ExitFunc, Func&>)
        : m_exit_function(
              detail::conditional_forward(
                  std::forward<Func>(fn),
                  std::bool_constant<
                      std::is_nothrow_constructible_v<ExitFunc, Func> && !std::is_lvalue_reference_v<Func>>()
              )
          )
    {}

    /**
     * @brief Constructs a new @a success_action from an exit function of type @a Func.
     *
     * Initializes the exit function with a function or function object `fn`. The constructed `success_action` is
     * active. The stored exit function is initialized with `std::forward<Func>(fn)`.
     *
     * This overload participates in overload resolution only if:
     *   - `std::is_same_v<std::remove_cvref_t<Func>, success_action>` is `false`, and
     *   - `std::is_lvalue_reference_v<Func>` is `false`, and
     *   - `std::is_nothrow_constructible_v<ExitFunc, Func>` is `true`.
     *
     * @tparam Func Exit function type. Must be constructible from @a ExitFunc.
     * @param fn Exit function.
     * @see https://en.cppreference.com/w/cpp/experimental/scope_success/scope_success
     */
    template<typename Func>
    requires detail::can_move_construct_from_noexcept<success_action, ExitFunc, Func>
    explicit success_action(Func&& fn) noexcept : m_exit_function(std::forward<Func>(fn))
    {}

    /**
     * @brief Move constructor.
     *
     * Initializes the stored exit function with the one in `other`, and initializes the counter of
     * uncaught exceptions with the one in `other`. The constructed `success_action` is active
     * if and only if `other` is active before the construction.
     *
     * If `std::is_nothrow_move_constructible_v<ExitFunc>` is true, initializes stored exit function (denoted by
     * `exitfun`) with `std::forward<ExitFunc>(other.exitfun)`, otherwise initializes it with `other.exitfun`.
     *
     * After successful move construction, `other.release()` is called and `other` becomes inactive.
     *
     * This overload participates in overload resolution only if:
     *   - `std::is_nothrow_move_constructible_v<ExitFunc>` is `true`, or
     *   - `std::is_copy_constructible_v<ExitFunc>` is `true`.
     *
     * @param other `success_action` to move from.
     * @throw anything Any exception thrown during the initialization of the stored exit function.
     * @see https://en.cppreference.com/w/cpp/experimental/scope_success/scope_success
     */
    success_action(
        success_action&& other
    ) noexcept(std::is_nothrow_move_constructible_v<ExitFunc> || std::is_nothrow_copy_constructible_v<ExitFunc>)
    requires(std::is_nothrow_move_constructible_v<ExitFunc> || std::is_copy_constructible_v<ExitFunc>)
        : m_exit_function(
              detail::conditional_forward(
                  std::forward<ExitFunc>(other.m_exit_function),
                  std::bool_constant<std::is_nothrow_move_constructible_v<ExitFunc>>()
              )
          ),
          m_uncaught_exceptions_count(other.m_uncaught_exceptions_count)
    {
        other.release();
    }

    /** @cond */
    /** @brief @a success_action is not @a CopyConstructible */
    success_action(const success_action&)            = delete;
    /** @brief @a success_action is not @a CopyAssignable */
    success_action& operator=(const success_action&) = delete;
    /** @brief @a success_action is not @a MoveAssignable */
    success_action& operator=(success_action&&)      = delete;
    /** @endcond */

    /**
     * @brief Calls the exit function when the scope is exited normally if the `success_action` is active, then destroys
     * the object.
     *
     * Calls the exit function if the result of `std::uncaught_exceptions()` is less than or equal
     * to the counter of uncaught exceptions (typically on normal exit) and the `success_action` is active,
     * then destroys the object.
     *
     * @throws anything Throws any exception thrown by calling the exit function.
     * @see https://en.cppreference.com/w/cpp/experimental/scope_success/%7Escope_success
     */
    ~success_action() noexcept(noexcept(this->m_exit_function()))
    {
        if (std::uncaught_exceptions() <= this->m_uncaught_exceptions_count) {
            this->m_exit_function();
        }
    }

    /**
     * @brief Makes the @a success_action object inactive.
     *
     * Once an @a success_action is inactive, it cannot become active again, and it will not call its exit function upon
     * destruction.
     *
     * @see https://en.cppreference.com/w/cpp/experimental/scope_exit/release
     */
    void release() noexcept { this->m_uncaught_exceptions_count = std::numeric_limits<int>::min(); }

private:
    ExitFunc m_exit_function;                                      ///< The stored exit function.
    int m_uncaught_exceptions_count = std::uncaught_exceptions();  ///< The counter of uncaught exceptions.
};

/**
 * @brief Deduction guide for @a success_action.
 *
 * @tparam ExitFunc Exit function type.
 */
template<typename ExitFunc>
success_action(ExitFunc) -> success_action<ExitFunc>;

/**
 * @example scope_action.cpp
 * Example of using `exit_action`, `fail_action`, and `success_action`.
 */

}  // namespace wwa::utils

#endif /* A2100C2E_3B3D_4875_ACA4_BFEF2E5B6120 */
