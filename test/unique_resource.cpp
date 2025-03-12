#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <type_traits>

#include "unique_resource.h"

namespace {

template<bool NE, bool Throw = false>
struct Resource {
    Resource()                             = default;
    Resource(const Resource&) noexcept(NE) = default;

    // NOLINTNEXTLINE(bugprone-exception-escape)
    Resource(Resource&&) noexcept(NE)
    {
        if constexpr (Throw) {
            throw std::runtime_error("Resource move constructor");
        }
    }
};

template<bool NE, typename Res, bool Throw = false>
struct Deleter {
    Deleter(int& run) : m_run(&run) {}
    Deleter(const Deleter& other) : m_run(other.m_run)
    {
        if (other.m_throw_on_copy) {
            throw std::runtime_error("Deleter copy constructor");
        }
    }

    // NOLINTNEXTLINE(bugprone-exception-escape)
    Deleter(Deleter&& other) noexcept(NE) : m_run(other.m_run)
    {
        if constexpr (Throw) {
            throw std::runtime_error("Deleter move constructor");
        }
    }

    void operator()(Res&) { *this->m_run += 1; }

    void set_throw_on_copy(bool value) const { this->m_throw_on_copy = value; }

private:
    int* m_run;
    mutable bool m_throw_on_copy = false;
};

template<bool NE>
struct MoveAssignableResource {
    MoveAssignableResource()                                                 = default;
    MoveAssignableResource(const MoveAssignableResource&)                    = default;
    MoveAssignableResource& operator=(MoveAssignableResource&&) noexcept(NE) = default;
    MoveAssignableResource& operator=(const MoveAssignableResource&)         = default;
};

template<bool NE, typename Res>
struct MoveAssignableDeleter {
    MoveAssignableDeleter(int& run) : m_run(&run) {}
    MoveAssignableDeleter(const MoveAssignableDeleter&)                    = default;
    MoveAssignableDeleter& operator=(MoveAssignableDeleter&&) noexcept(NE) = default;
    MoveAssignableDeleter& operator=(const MoveAssignableDeleter&)         = default;

    void operator()(Res&) { *this->m_run += 1; }

private:
    int* m_run;
};

}  // namespace

TEST(UniqueResource, MakeChecked_Invalid)
{
    bool run   = false;
    auto close = [&run](FILE* file) {
        EXPECT_EQ(std::fclose(file), 0);
        run = true;
    };

    {
        const char* fname = "this-file-does-not-exist.txt";
        auto file         = wwa::utils::make_unique_resource_checked(std::fopen(fname, "r"), nullptr, close);
        EXPECT_EQ(file.get(), nullptr);
    }

    EXPECT_FALSE(run);
}

TEST(UniqueResource, MakeChecked_Valid)
{
    bool run  = false;
    auto free = [&run](void* p) {
        std::free(p);  // NOLINT(cppcoreguidelines-no-malloc)
        run = true;
    };

    {
        // NOLINTNEXTLINE(cppcoreguidelines-no-malloc)
        auto ptr = wwa::utils::make_unique_resource_checked(std::malloc(1), nullptr, free);
        ASSERT_NE(ptr.get(), nullptr);
    }

    EXPECT_TRUE(run);
}

TEST(UniqueResource, DefaultCtor)
{
    const wwa::utils::unique_resource<void*, void (*)(void*)> ptr;
    EXPECT_EQ(ptr.get(), nullptr);
    EXPECT_EQ(ptr.get_deleter(), nullptr);
}

TEST(UniqueResource, MoveCtor_NN)
{
    int run   = 0;
    auto free = [&run](void* p) {
        std::free(p);  // NOLINT(cppcoreguidelines-no-malloc)
        ++run;
    };

    {
        wwa::utils::unique_resource<void*, decltype(free)> f1(nullptr, free);
        const wwa::utils::unique_resource<void*, decltype(free)> f2(std::move(f1));

        EXPECT_NE(&f2.get_deleter(), nullptr);
    }

    EXPECT_EQ(run, 1);
}

TEST(UniqueResource, MoveCtor_NE)
{
    int deleter_run = 0;

    Resource<true> res;
    static_assert(std::is_nothrow_move_constructible_v<Resource<true>>);

    Deleter<false, Resource<true>&> del(deleter_run);
    static_assert(!std::is_nothrow_move_constructible_v<Deleter<false, Resource<true>>>);

    {
        wwa::utils::unique_resource<Resource<true>&, Deleter<false, Resource<true>&>> f1(res, del);
        const wwa::utils::unique_resource<Resource<true>&, Deleter<false, Resource<true>&>> f2(std::move(f1));

        EXPECT_EQ(deleter_run, 0);

        EXPECT_EQ(&f2.get(), &res);
    }

    EXPECT_EQ(deleter_run, 1);
}

TEST(UniqueResource, MoveCtor_EE)
{
    int deleter_run = 0;

    Resource<false> res;
    static_assert(!std::is_nothrow_move_constructible_v<Resource<false>>);

    Deleter<false, Resource<false>> del(deleter_run);
    static_assert(!std::is_nothrow_move_constructible_v<Deleter<false, Resource<false>>>);

    {
        wwa::utils::unique_resource<Resource<false>, Deleter<false, Resource<false>>> f1(res, del);
        const wwa::utils::unique_resource<Resource<false>, Deleter<false, Resource<false>>> f2(std::move(f1));

        EXPECT_EQ(deleter_run, 0);
    }

    EXPECT_EQ(deleter_run, 1);
}

TEST(UniqueResource, MoveCtor_NT)
{
    int deleter_run = 0;

    Resource<true> res;
    static_assert(std::is_nothrow_move_constructible_v<Resource<true>>);

    Deleter<false, Resource<true>, true> del(deleter_run);
    static_assert(!std::is_nothrow_move_constructible_v<Deleter<false, Resource<true>, true>>);

    {
        using ur_t = wwa::utils::unique_resource<Resource<true>, Deleter<false, Resource<true>, true>>;
        ur_t f1(res, del);
        f1.get_deleter().set_throw_on_copy(true);
        EXPECT_THROW(const ur_t f2(std::move(f1)), std::runtime_error);

        EXPECT_EQ(deleter_run, 1);
    }

    EXPECT_EQ(deleter_run, 1);
}

TEST(UniqueResource, MoveCtor_NT2)
{
    int deleter_run = 0;

    Resource<true> res;
    static_assert(std::is_nothrow_move_constructible_v<Resource<true>>);

    Deleter<false, Resource<true>, true> del(deleter_run);
    static_assert(!std::is_nothrow_move_constructible_v<Deleter<false, Resource<true>, true>>);

    {
        using ur_t = wwa::utils::unique_resource<Resource<true>, Deleter<false, Resource<true>, true>>;
        ur_t f1(res, del);
        f1.release();
        f1.get_deleter().set_throw_on_copy(true);
        EXPECT_THROW(const ur_t f2(std::move(f1)), std::runtime_error);

        EXPECT_EQ(deleter_run, 0);
    }

    EXPECT_EQ(deleter_run, 0);
}

TEST(UniqueResource, MoveCtor_TE)
{
    int deleter_run = 0;

    struct Resource {
        Resource() noexcept  = default;
        Resource(Resource&&) = delete;
        Resource(const Resource& other) : m_throw_on_copy(!other.m_throw_on_copy)
        {
            if (other.m_throw_on_copy) {
                throw std::runtime_error("Resource copy constructor");
            }
        }

    private:
        bool m_throw_on_copy = false;
    };

    static_assert(!std::is_nothrow_move_constructible_v<Resource>);

    Resource res;
    Deleter<false, Resource&> del(deleter_run);
    static_assert(!std::is_nothrow_move_constructible_v<Deleter<false, Resource&>>);

    {
        using ur_t = wwa::utils::unique_resource<Resource, Deleter<false, Resource&>>;
        ur_t f1(res, del);
        EXPECT_THROW(const ur_t f2(std::move(f1)), std::runtime_error);

        EXPECT_EQ(deleter_run, 0);
    }

    EXPECT_EQ(deleter_run, 1);
}

TEST(UniqueResource, Assign_NN)
{
    int deleter_run = 0;

    MoveAssignableResource<true> res1;
    const MoveAssignableResource<true> res2;

    MoveAssignableDeleter<true, MoveAssignableResource<true>&> del1(deleter_run);
    const MoveAssignableDeleter<true, MoveAssignableResource<true>&> del2(deleter_run);

    {
        using ur_t = wwa::utils::unique_resource<
            MoveAssignableResource<true>, MoveAssignableDeleter<true, MoveAssignableResource<true>&>>;

        ur_t f1(res1, del1);
        ur_t f2(res2, del2);
        f2 = std::move(f1);

        EXPECT_EQ(deleter_run, 1);  // f2's deleter for the original resource
    }

    EXPECT_EQ(deleter_run, 2);  // f2's deleter for the assigned resource
}

TEST(UniqueResource, Assign_NE)
{
    int deleter_run = 0;

    MoveAssignableResource<true> res1;
    const MoveAssignableResource<true> res2;

    MoveAssignableDeleter<false, MoveAssignableResource<true>&> del1(deleter_run);
    const MoveAssignableDeleter<false, MoveAssignableResource<true>&> del2(deleter_run);

    {
        using ur_t = wwa::utils::unique_resource<
            MoveAssignableResource<true>, MoveAssignableDeleter<false, MoveAssignableResource<true>&>>;

        ur_t f1(res1, del1);
        ur_t f2(res2, del2);
        f2 = std::move(f1);

        EXPECT_EQ(deleter_run, 1);  // f2's deleter for the original resource
    }

    EXPECT_EQ(deleter_run, 2);  // f2's deleter for the assigned resource
}

TEST(UniqueResource, Assign_EN)
{
    int deleter_run = 0;

    MoveAssignableResource<false> res1;
    const MoveAssignableResource<false> res2;

    MoveAssignableDeleter<true, MoveAssignableResource<false>&> del1(deleter_run);
    const MoveAssignableDeleter<true, MoveAssignableResource<false>&> del2(deleter_run);

    {
        using ur_t = wwa::utils::unique_resource<
            MoveAssignableResource<false>, MoveAssignableDeleter<true, MoveAssignableResource<false>&>>;

        ur_t f1(res1, del1);
        ur_t f2(res2, del2);
        f2 = std::move(f1);

        EXPECT_EQ(deleter_run, 1);  // f2's deleter for the original resource
    }

    EXPECT_EQ(deleter_run, 2);  // f2's deleter for the assigned resource
}

TEST(UniqueResource, Assign_EE)
{
    int deleter_run = 0;

    MoveAssignableResource<false> res1;
    const MoveAssignableResource<false> res2;

    MoveAssignableDeleter<false, MoveAssignableResource<false>&> del1(deleter_run);
    const MoveAssignableDeleter<false, MoveAssignableResource<false>&> del2(deleter_run);

    {
        using ur_t = wwa::utils::unique_resource<
            MoveAssignableResource<false>, MoveAssignableDeleter<false, MoveAssignableResource<false>&>>;

        ur_t f1(res1, del1);
        ur_t f2(res2, del2);
        f2 = std::move(f1);

        EXPECT_EQ(deleter_run, 1);  // f2's deleter for the original resource
    }

    EXPECT_EQ(deleter_run, 2);  // f2's deleter for the assigned resource
}

TEST(UniqueResource, Assign_EE2)
{
    int deleter_run = 0;

    MoveAssignableResource<false> res1;
    MoveAssignableResource<false> res2;

    MoveAssignableDeleter<false, MoveAssignableResource<false>&> del1(deleter_run);
    MoveAssignableDeleter<false, MoveAssignableResource<false>&> del2(deleter_run);

    {
        using ur_t = wwa::utils::unique_resource<
            MoveAssignableResource<false>&, MoveAssignableDeleter<false, MoveAssignableResource<false>&>>;

        ur_t f1(res1, del1);
        ur_t f2(res2, del2);
        f2 = std::move(f1);

        EXPECT_EQ(deleter_run, 1);  // f2's deleter for the original resource
    }

    EXPECT_EQ(deleter_run, 2);  // f2's deleter for the assigned resource
}

TEST(UniqueResource, Release)
{
    int deleter_run = 0;

    Resource<true> res;
    Deleter<true, Resource<true>&> del(deleter_run);

    {
        wwa::utils::unique_resource<Resource<true>&, Deleter<true, Resource<true>&>> f(res, del);
        f.release();

        EXPECT_EQ(deleter_run, 0);
    }

    EXPECT_EQ(deleter_run, 0);
}

TEST(UniqueResource, Reset)
{
    int deleter_run = 0;

    Resource<true> res;
    Deleter<true, Resource<true>&> del(deleter_run);

    {
        wwa::utils::unique_resource<Resource<true>&, Deleter<true, Resource<true>&>> f(res, del);
        f.reset();

        EXPECT_EQ(deleter_run, 1);
    }

    EXPECT_EQ(deleter_run, 1);
}

TEST(UniqueResource, Reset2A)
{
    int deleter_run = 0;

    struct Resource {};

    Resource res;
    Deleter<true, Resource&> del(deleter_run);

    {
        wwa::utils::unique_resource<Resource&, Deleter<true, Resource&>> f(res, del);
        f.reset(res);

        EXPECT_EQ(deleter_run, 1);
    }

    EXPECT_EQ(deleter_run, 2);
}

TEST(UniqueResource, Reset2B)
{
    int deleter_run = 0;

    struct Resource {
        Resource()                                           = default;
        Resource(const Resource&)                            = default;
        Resource& operator=(const Resource&) noexcept(false) = default;
    };

    Resource res;
    Deleter<true, Resource&> del(deleter_run);

    {
        wwa::utils::unique_resource<Resource, Deleter<true, Resource&>> f(res, del);
        f.reset(res);

        EXPECT_EQ(deleter_run, 1);
    }

    EXPECT_EQ(deleter_run, 2);
}

TEST(UniqueResource, Accessors)
{
    constexpr int expected_value = 42;

    struct s_t {
        int value;
    };

    auto deallocate = [](s_t* s) { delete s; };
    auto allocate   = []() { return new s_t{expected_value}; };

    auto obj = wwa::utils::make_unique_resource_checked(allocate(), nullptr, deallocate);
    ASSERT_NE(obj.get(), nullptr);
    EXPECT_EQ(obj->value, expected_value);
    EXPECT_EQ((*obj).value, expected_value);
}
