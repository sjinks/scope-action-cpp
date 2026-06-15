#ifndef E25CE0A0_3429_4977_B6AE_73697782F7BD
#define E25CE0A0_3429_4977_B6AE_73697782F7BD

#include <type_traits>
#include <utility>
#include "scope_action.h"

namespace wwa::utils {

/**
 * @brief A universal RAII resource handle wrapper.
 *
 * `unique_resource` is a universal RAII wrapper for resource handles that owns and manages a resource through a handle
 * and disposes of that resource when the `unique_resource` is destroyed.
 *
 * The resource is disposed of using the deleter of type `Deleter` when either of the following happens:
 *   - the managing `unique_resource` object is destroyed,
 *   - the managing `unique_resource` object is assigned from another resource via `operator=()` or `reset()`.
 *
 * Usage example:
 * @snippet{trimleft} unique_resource.cpp Using unique_resource
 *
 * @tparam Resource Resource handle type. `Resource` shall be an object type or an lvalue reference to an object type.
 * `std::remove_reference_t<Resource>` shall be
 * [MoveConstructible](https://en.cppreference.com/w/cpp/named_req/MoveConstructible), and if
 * `std::remove_reference_t<Resource>` is not
 * [CopyConstructible](https://en.cppreference.com/w/cpp/named_req/CopyConstructible),
 * `std::is_nothrow_move_constructible_v<std::remove_reference_t<Resource>>` shall be `true`.
 * @tparam Deleter Deleter type. `Deleter` shall be a
 * [Destructible](https://en.cppreference.com/w/cpp/named_req/Destructible) and
 * [MoveConstructible](https://en.cppreference.com/w/cpp/named_req/MoveConstructible)
 * [FunctionObject](https://en.cppreference.com/w/cpp/named_req/FunctionObject) type, and if `Deleter` is not
 * [CopyConstructible](https://en.cppreference.com/w/cpp/named_req/CopyConstructible),
 * `std::is_nothrow_move_constructible_v<Deleter>` shall be `true`. Given an lvalue `d` of type `Deleter` and an lvalue
 * `r` of type `std::remove_reference_t<Resource>`, the expression `d(r)` shall be well-formed.
 *
 * @see https://en.cppreference.com/w/cpp/experimental/unique_resource
 * @see `make_unique_resource_checked()`
 */
template<typename Resource, typename Deleter>
requires(!std::is_rvalue_reference_v<Resource> && !std::is_reference_v<Deleter>)
class [[nodiscard]] unique_resource {
    /**
     * @brief Dummy "scope guard".
     *
     * A structure that provides a no-op `release()` method.
     * It is used instead of `fail_action` when the operation is known to be no-throw.
     */
    struct dummy_scope_guard {
        /**
         * @brief Dummy method that does nothing.
         */
        constexpr void release() {}
    };

    /**
     * @brief Guards object construction with a scope guard.
     *
     * @tparam T Object type.
     */
    template<typename T>
    struct guard {  // NOLINT(cppcoreguidelines-special-member-functions)
        template<typename U>
        requires std::is_constructible_v<T, U>
        guard(U&&) noexcept(std::is_nothrow_constructible_v<T, U>);

        /**
         * @brief Constructor.
         *
         * Constructs a `T` with `U` and releases the scope guard `ScopeGuard`.
         *
         * @tparam U Object type to construct `T` from. `std::is_constructible_v<T, U>` must be `true`.
         * @tparam ScopeGuard Scope guard type.
         */
        template<typename U, typename ScopeGuard>
        requires(std::is_constructible_v<T, U>)
        // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
        guard(U&& r, ScopeGuard&& d) noexcept(std::is_nothrow_constructible_v<T, U>) : m_obj(std::forward<U>(r))
        {
            d.release();
        }

        /** @brief Default constructor */
        guard()        = default;
        /** @brief Move constructor */
        guard(guard&&) = default;

        /**
         * @brief Move constructor.
         *
         * This overload participates in overload resolution only if `std::is_nothrow_move_constructible_v<T>` is
         * `false`.
         *
         * @param rhs Another `guard` to acquire the ownership from.
         */
        guard(guard&& rhs) noexcept(std::is_nothrow_constructible_v<T, T&>)
        requires(!std::is_nothrow_move_constructible_v<T>)
            : m_obj(rhs.m_obj)
        {}

        /** @brief Assignment operator */
        guard& operator=(const guard&) = default;
        /** @brief Move-assignment operator */
        guard& operator=(guard&&)      = default;

        /**
         * @brief Returns a reference to the stored object.
         *
         * @return Reference to the stored object.
         */
        constexpr T& get() noexcept { return this->m_obj; }

        /**
         * @brief Returns a constant reference to the stored object.
         *
         * @return Constant reference to the stored object.
         */
        [[nodiscard]] constexpr const T& get() const noexcept { return this->m_obj; }

    private:
        [[no_unique_address]] T m_obj{};  ///< Stored object.
    };

    /**
     * @brief Guards construction with a scope guard for a reference type.
     *
     * @tparam T Object type.
     */
    template<typename T>
    struct guard<T&> {  // NOLINT(cppcoreguidelines-special-member-functions)
        template<typename U>
        requires std::is_constructible_v<std::reference_wrapper<T>, U>
        guard(U&&) noexcept(std::is_nothrow_constructible_v<std::reference_wrapper<T>, U>);

        /**
         * @brief Constructor.
         *
         * Constructs a `std::reference_wrapper<std::remove_reference_t<T>>` with `U` and releases the scope guard
         * `ScopeGuard`.
         *
         * @tparam U Object type to construct `std::reference_wrapper<std::remove_reference_t<T>>` from.
         * @tparam ScopeGuard Scope guard type.
         */
        template<typename U, typename ScopeGuard>
        guard(
            U&& r, ScopeGuard&& d  // NOLINT(cppcoreguidelines-missing-std-forward)
        ) noexcept(std::is_nothrow_constructible_v<std::reference_wrapper<std::remove_reference_t<T>>, U>)
            : m_value(static_cast<T&>(r))
        {
            d.release();
        }

        /** @cond */
        guard()                        = delete;
        /** @endcond */
        /** @brief Copy constructor. */
        guard(const guard&)            = default;
        /** @brief Assignment operator. */
        guard& operator=(const guard&) = default;

        /**
         * @brief Returns the stored reference.
         *
         * @return Stored reference.
         */
        T& get() noexcept { return this->m_value.get(); }

        /**
         * @brief Returns the stored reference.
         *
         * @return Stored reference.
         */
        [[nodiscard]] T& get() const noexcept { return this->m_value.get(); }

    private:
        std::reference_wrapper<std::remove_reference_t<T>> m_value;  ///< Stored reference.
    };

    using guarded_resource_t = guard<Resource>;  ///< Guarded `Resource`
    using guarded_deleter_t  = guard<Deleter>;   ///< Guarded `Deleter`

    /**
     * @brief Forward type.
     *
     * If `T` is nothrow constructible from `U`, then `U`; otherwise `U&`.
     *
     * @tparam T Object type to construct.
     * @tparam U Object type to construct `T` from.
     */
    template<typename T, typename U>
    requires(std::is_constructible_v<T, U> && (std::is_nothrow_constructible_v<T, U> || std::is_constructible_v<T, U&>))
    using forwarder_t = std::conditional_t<std::is_nothrow_constructible_v<T, U>, U, U&>;

    /**
     * @brief A helper functon to forwards `U` as `U` or `U&`.
     *
     * @see @a forwarder_t
     *
     * @tparam T Object type to construct.
     * @tparam U Object type to construct `T` from.
     * @param u Object to construct `T` from.
     * @return `U` or `U&`.
     */
    template<typename T, typename U>
    static constexpr forwarder_t<T, U> fwd(U& u)
    {
        return static_cast<forwarder_t<T, U>&&>(u);
    }

    /**
     * @brief Returns a scope guard for the construction of `T` from `U`.
     *
     * If `T` is nothrow constructible from `U`, returns a dummy scope guard; otherwise, returns a `fail_action` that
     * will release the resource `r` with `d(r)` on failure.
     *
     * @tparam T Object type to construct.
     * @tparam U Object type to construct `T` from.
     * @tparam Del Deleter type.
     * @tparam Res Resource type.
     * @param d Deleter.
     * @param r Resource.
     * @return Scope guard.
     */
    template<typename T, typename U, typename Del, typename Res>
    static constexpr auto make_scope_guard(Del& d, Res& r)
    {
        if constexpr (std::is_nothrow_constructible_v<T, U>) {
            return dummy_scope_guard{};
        }
        else {
            return fail_action{[&d, &r] { d(r); }};
        }
    }

public:
    /**
     * @brief Constructs a new `unique_resource`.
     *
     * Default constructor. Value-initializes the stored resource handle and the deleter.
     *
     * This overload participates in overload resolution only if both `std::is_default_constructible_v<Resource>` and
     * `std::is_default_constructible_v<Deleter>` are `true`.
     *
     * @note The constructed `unique_resource` does not own the resource.
     *
     * @see https://en.cppreference.com/w/cpp/experimental/unique_resource/unique_resource
     */
    unique_resource()
    requires(std::is_default_constructible_v<Resource> && std::is_default_constructible_v<Deleter>)
    = default;

    /** @cond */
    unique_resource(const unique_resource&) = delete;
    /** @endcond */

    /**
     * @brief Constructs a new `unique_resource`.
     *
     * The stored resource handle is initialized with `std::forward<Res>(r)` if
     * `std::is_nothrow_constructible_v<Resource, Res>` is `true`, otherwise `r`. If initialization of the stored
     * resource handle throws an exception, calls `d(r)`.
     *
     * Then, the deleter is initialized with `std::forward<Deleter>(d)` if
     * `std::is_nothrow_constructible_v<Deleter, Del>` is `true`, otherwise `d`. If initialization of deleter throws an
     * exception, calls `d(m_resource)`.
     *
     * The constructed `unique_resource` owns the resource.
     *
     * @tparam Res Resource type.
     * @tparam Del Deleter type.
     * @param r A resource handle.
     * @param d A deleter to use to dispose the resource.
     *
     * @see https://en.cppreference.com/w/cpp/experimental/unique_resource/unique_resource
     */
    template<typename Res, typename Del>
    requires requires {
        typename forwarder_t<guarded_resource_t, Res>;
        typename forwarder_t<Deleter, Del>;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
    unique_resource(Res&& r, Del&& d) noexcept(
        (std::is_nothrow_constructible_v<guarded_resource_t, Res> ||
         std::is_nothrow_constructible_v<guarded_resource_t, Res&>) &&
        (std::is_nothrow_constructible_v<Deleter, Del> || std::is_nothrow_constructible_v<Deleter, Del&>)
    )
        : m_resource(fwd<guarded_resource_t, Res>(r), make_scope_guard<guarded_resource_t, Res>(d, r)),
          m_deleter(fwd<Deleter, Del>(d), make_scope_guard<Deleter, Del>(d, m_resource.get())), m_run_on_reset(true)
    {}

    /**
     * @brief Constructs a new `unique_resource`.
     *
     * Move constructor.
     *
     * @li The stored resource handle is initialized from the one of `rhs` using `std::move`.
     * @li Then, the deleter is initialized with the one of `rhs` using `std::move`.
     * @li After construction, the constructed `unique_resource` owns its resource if and only if `rhs` owned the
     * resource before the construction, and `rhs` is set to not own the resource.
     *
     * @param rhs Another unique_resource to acquire the ownership from.
     *
     * @see https://en.cppreference.com/w/cpp/experimental/unique_resource/unique_resource
     */
    unique_resource(unique_resource&& rhs) noexcept
    requires std::is_nothrow_move_constructible_v<guarded_resource_t> && std::is_nothrow_move_constructible_v<Deleter>
        : m_resource(std::move(rhs.m_resource)), m_deleter(std::move(rhs.m_deleter)),
          m_run_on_reset(std::exchange(rhs.m_run_on_reset, false))
    {}

    /**
     * @brief Constructs a new `unique_resource`.
     *
     * Move constructor.
     *
     * @li The stored resource handle is initialized from the one of `rhs` using `std::move`.
     * @li Then, the deleter is initialized with the one of `rhs`. If initialization of the deleter throws an exception
     * and `rhs` owns the resource, calls the deleter of `rhs` with `m_resource` to dispose the resource, then calls
     * `rhs.release()`.
     * @li After construction, the constructed `unique_resource` owns its resource if and only if `rhs` owned the
     * resource before the construction, and `rhs` is set to not own the resource.
     *
     * @param rhs Another unique_resource to acquire the ownership from.
     * @throw * Any exception thrown during initialization of the stored resource handle or the deleter.
     *
     * @see https://en.cppreference.com/w/cpp/experimental/unique_resource/unique_resource
     */
    unique_resource(unique_resource&& rhs)  // NOLINT(performance-noexcept-move-constructor,bugprone-exception-escape)
    requires(std::is_nothrow_move_constructible_v<guarded_resource_t> && !std::is_nothrow_move_constructible_v<Deleter>)
        : m_resource(std::move(rhs.m_resource)),
          m_deleter(fwd<Deleter, Deleter>(rhs.m_deleter.get()), fail_action([&rhs, this] {
                        if (rhs.m_run_on_reset) {
                            rhs.m_deleter.get()(this->m_resource.get());
                            rhs.release();
                        }
                    })),
          m_run_on_reset(std::exchange(rhs.m_run_on_reset, false))
    {}

    /**
     * @brief Constructs a new `unique_resource`.
     *
     * Move constructor.
     *
     * @li The stored resource handle is initialized from the one of `rhs`. If initialization of the stored resource
     * handle throws an exception, `rhs` is not modified.
     * @li Then, the deleter is initialized with the one of `rhs`, using `std::move` if
     * `std::is_nothrow_move_constructible_v<Deleter>` is true.
     * @li After construction, the constructed `unique_resource` owns its resource if and only if `rhs` owned the
     * resource before the construction, and `rhs` is set to not own the resource.
     *
     * @param rhs Another unique_resource to acquire the ownership from.
     * @throw * Any exception thrown during initialization of the stored resource handle or the deleter.
     *
     * @see https://en.cppreference.com/w/cpp/experimental/unique_resource/unique_resource
     */
    unique_resource(unique_resource&& rhs)  // NOLINT(performance-noexcept-move-constructor,bugprone-exception-escape)
    requires(!std::is_nothrow_move_constructible_v<guarded_resource_t>)
        : unique_resource(rhs.m_resource.get(), rhs.m_deleter.get(), dummy_scope_guard{})
    {
        this->m_run_on_reset = std::exchange(rhs.m_run_on_reset, false);
    }

    /**
     * @brief Disposes the managed resource if such is present.
     *
     * Disposes the resource by calling the deleter with the underlying resource handle if the `unique_resource` owns
     * it, equivalent to calling `reset()`. Then destroys the stored resource handle and the deleter.
     *
     * @see https://en.cppreference.com/w/cpp/experimental/unique_resource/%7Eunique_resource
     */
    ~unique_resource() { this->reset(); }

    /** @cond */
    unique_resource& operator=(const unique_resource&) = delete;
    /** @endcond */

    /**
     * @brief Assigns a `unique_resource`.
     *
     * Move assignment operator. Replaces the managed resource and the deleter with `rhs`'s.
     *
     * @li First, calls `reset()` to dispose the currently owned resource, if any.
     * @li Then assigns the stored resource handle and the deleter with `rhs`'s. `std::move` is applied to the stored
     * resource handle or the deleter of `rhs` if `std::is_nothrow_move_assignable_v<Resource>` or
     * `std::is_nothrow_move_assignable_v<Deleter>` is `true` respectively. Assignment of the stored resource handle is
     * executed first, unless `std::is_nothrow_move_assignable_v<Deleter>` is `false` and
     * `std::is_nothrow_move_assignable_v<Resource>` is `true`.
     * @li Finally, sets `*this` to own the resource if and only if `rhs` owned it before assignment, and `rhs` not to
     * own the resource.
     *
     * If `std::is_nothrow_move_assignable_v<Resource>` is true, `Resource` shall satisfy the
     * [MoveAssignable](https://en.cppreference.com/w/cpp/named_req/MoveAssignable) requirements;
     * otherwise `Resource` shall satisfy the
     * [CopyAssignable](https://en.cppreference.com/w/cpp/named_req/CopyAssignable) requirements.
     *
     * If `std::is_nothrow_move_assignable_v<Deleter>` is `true`, `Deleter` shall satisfy the
     * [MoveAssignable](https://en.cppreference.com/w/cpp/named_req/MoveAssignable) requirements;
     * otherwise `Deleter` shall satisfy the
     * [CopyAssignable](https://en.cppreference.com/w/cpp/named_req/CopyAssignable) requirements.
     *
     * Failing to satisfy above requirements results in undefined behavior.
     *
     * @param rhs resource wrapper from which ownership will be transferred
     * @return `*this`
     * @see https://en.cppreference.com/w/cpp/experimental/unique_resource/operator%3D
     */
    unique_resource& operator=(unique_resource&& rhs
    ) noexcept(std::is_nothrow_move_assignable_v<guarded_resource_t> && std::is_nothrow_move_assignable_v<Deleter>)
    {
        this->reset();
        if constexpr (std::is_nothrow_move_assignable_v<guarded_resource_t>) {
            if constexpr (std::is_nothrow_move_assignable_v<Deleter>) {
                this->m_resource = std::move(rhs.m_resource);
                this->m_deleter  = std::move(rhs.m_deleter);
            }
            else {
                this->m_deleter  = rhs.m_deleter;
                this->m_resource = std::move(rhs.m_resource);
            }
        }
        else {
            this->m_resource = rhs.m_resource;
            if constexpr (std::is_nothrow_move_assignable_v<Deleter>) {
                this->m_deleter = std::move(rhs.m_deleter);
            }
            else {
                this->m_deleter = rhs.m_deleter;
            }
        }

        this->m_run_on_reset = std::exchange(rhs.m_run_on_reset, false);
        return *this;
    }

    /**
     * @brief Releases the ownership.
     *
     * Releases the ownership of the managed resource if any. The destructor will not execute the deleter after the
     * call, unless `reset()` is called later for managing new resource.
     *
     * @see https://en.cppreference.com/w/cpp/experimental/unique_resource/release
     */
    void release() noexcept { this->m_run_on_reset = false; }

    /**
     * @brief Disposes the managed resource.
     *
     * Disposes the resource by calling the deleter with the underlying resource handle if the `unique_resource` owns
     * it. The unique_resource does not own the resource after the call.
     *
     * @see https://en.cppreference.com/w/cpp/experimental/unique_resource/reset
     */
    void reset() noexcept
    {
        if (this->m_run_on_reset) {
            this->m_run_on_reset = false;
            this->m_deleter.get()(this->m_resource.get());
        }
    }

    /**
     * @brief Replaces the managed resource.
     *
     * Replaces the resource by calling `reset()` and then assigns the stored resource handle with
     * `std::forward<Res>`(r) if `std::is_nothrow_assignable_v<Resource, Res>` is `true`, otherwise `std::as_const(r)`,
     * where `Resource` is the type of stored resource handle. The `unique_resource` owns the resource after the call.
     * If copy-assignment of the store resource handle throws an exception, calls `del(r)`, where `del` is the deleter
     * object.
     *
     * This overload participates in overload resolution only if the selected assignment expression assigning the
     * stored resource handle is well-formed.
     *
     * @warning The program is ill-formed if `del(r)` is ill-formed.
     * @warning The behavior is undefined if `del(r)` results in undefined behavior or throws an exception.
     *
     * @tparam Res Resource type.
     * @param r The new resource handle.
     * @throw * Any exception thrown in assigning the stored resource handle.
     * @see https://en.cppreference.com/w/cpp/experimental/unique_resource/reset
     */
    template<typename Res>
    void reset(Res&& r)
    {
        this->reset();
        if constexpr (std::is_nothrow_assignable_v<guarded_resource_t&, Res>) {
            this->m_resource.get() = std::forward<Res>(r);
        }
        else {
            this->m_resource.get() = std::as_const(r);  // const_cast<const std::remove_reference_t<Res>&>(r);
        }

        this->m_run_on_reset = true;
    }

    /**
     * @brief Accesses the underlying resource handle.
     *
     * @return The underlying resource handle.
     * @see https://en.cppreference.com/w/cpp/experimental/unique_resource/get
     */
    [[nodiscard]] const Resource& get() const noexcept { return this->m_resource.get(); }

    /**
     * @brief Get the deleter object.
     *
     * Accesses the deleter object which would be used for disposing the managed resource.
     *
     * @return The stored deleter.
     * @see https://en.cppreference.com/w/cpp/experimental/unique_resource/get_deleter
     */
    [[nodiscard]] const Deleter& get_deleter() const noexcept { return this->m_deleter.get(); }

    /**
     * @brief Accesses the pointee if the resource handle is a pointer.
     *
     * Access the object or function pointed by the underlying resource handle which is a pointer.
     * This function participates in overload resolution only if `std::is_pointer_v<Resource>` is `true`
     * and `std::is_void_v<std::remove_pointer_t<Resource>>` is `false`.
     *
     * @warning If the resource handle is not pointing to an object or a function, the behavior is undefined.
     *
     * @return The object or function pointed by the underlying resource handle.
     * @see https://en.cppreference.com/w/cpp/experimental/unique_resource/operator*
     */
    std::add_lvalue_reference_t<std::remove_pointer_t<Resource>> operator*() const noexcept
    requires(std::is_pointer_v<Resource> && !std::is_void_v<std::remove_pointer_t<Resource>>)
    {
        return *this->get();
    }

    /**
     * @brief Accesses the pointee if the resource handle is a pointer.
     *
     * Get a copy of the underlying resource handle which is a pointer. This function participates in overload
     * resolution only if `std::is_pointer_v<Resource>` is `true`. The return value is typically used to access the
     * pointed object.
     *
     * @return Copy of the underlying resource handle.
     * @see https://en.cppreference.com/w/cpp/experimental/unique_resource/operator*
     */
    Resource operator->() const noexcept
    requires(std::is_pointer_v<Resource>)
    {
        return this->get();
    }

private:
    [[no_unique_address]] guarded_resource_t m_resource{};  ///< Resource handle.
    [[no_unique_address]] guarded_deleter_t m_deleter{};    ///< Deleter.
    bool m_run_on_reset = false;                            ///< Whether to invoke the deleter on `reset()`/destruction.

    /**
     * @brief Creates a `unique_resource`, checking invalid value.
     *
     * @tparam Res Resource type.
     * @tparam Del Deleter type.
     * @tparam Invalid Type of the value indicating the resource handle is invalid.
     * @return A unique resource.
     * @see wwa::utils::make_unique_resource_checked()
     */
    template<typename Res, typename Del, typename Invalid>
    friend unique_resource<std::decay_t<Res>, std::decay_t<Del>>
    make_unique_resource_checked(Res&&, const Invalid&, Del&&)
#ifndef _MSC_VER
        noexcept(
            std::is_nothrow_constructible_v<std::decay_t<Res>, Res> &&
            std::is_nothrow_constructible_v<std::decay_t<Del>, Del>
        )
#endif
            ;

    template<typename Res, typename Del>
    unique_resource(
        Res&& r, Del&& d, dummy_scope_guard dummy
    ) noexcept(std::is_nothrow_constructible_v<Resource, Res> && std::is_nothrow_constructible_v<Deleter, Del>)
        : m_resource(std::forward<Res>(r), dummy), m_deleter(std::forward<Deleter>(d), dummy)
    {}
};

/**
 * @brief Deduction guide for @a unique_resource.
 *
 * @tparam Resource Resource type.
 * @tparam Deleter Deleter type.
 */
template<typename Resource, typename Deleter>
unique_resource(Resource, Deleter) -> unique_resource<Resource, Deleter>;

/**
 * @brief Creates a `unique_resource`, checking invalid value.
 *
 * Creates a `unique_resource`, initializes its stored resource handle with `std::forward<Res>(r)` and its deleter
 * with `std::forward<Del>(d)`. The created `unique_resource` owns the resource if and only if `bool(r == invalid)`
 * is `false`.
 *
 * Usage example:
 * @snippet{trimleft} unique_resource.cpp Using make_unique_resource_checked()
 *
 * @warning The program is ill-formed if the expression `r == invalid` cannot be contextually converted to `bool`,
 * and the behavior is undefined if the conversion results in undefined behavior or throws an exception.
 *
 * @note `make_unique_resource_checked()` exists to avoid calling a deleter function with an invalid argument.
 *
 * @tparam Res Resource type.
 * @tparam Del Deleter type.
 * @tparam Invalid Type of the value indicating the resource handle is invalid.
 * @param r A resource handle.
 * @param invalid A value indicating the resource handle is invalid.
 * @param d A deleter to use to dispose the resource
 * @return A unique resource.
 * @throw * Any exception thrown in initialization of the stored resource handle and the deleter.
 * @see https://en.cppreference.com/w/cpp/experimental/unique_resource/make_unique_resource_checked
 */
template<typename Res, typename Del, typename Invalid>
unique_resource<std::decay_t<Res>, std::decay_t<Del>>
make_unique_resource_checked(Res&& r, const Invalid& invalid, Del&& d)
#ifndef _MSC_VER
    noexcept(
        std::is_nothrow_constructible_v<std::decay_t<Res>, Res> &&
        std::is_nothrow_constructible_v<std::decay_t<Del>, Del>
    )
#endif
{
    if (r == invalid) {
        return {std::forward<Res>(r), std::forward<Del>(d), {}};
    }

    return {std::forward<Res>(r), std::forward<Del>(d)};
}

/**
 * @example unique_resource.cpp
 * Example of using `unqiue_resource` and `make_unique_resource_checked`.
 */

}  // namespace wwa::utils

#endif /* E25CE0A0_3429_4977_B6AE_73697782F7BD */
