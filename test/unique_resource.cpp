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

TEST(UniqueResource, SelfAssignment)
{
    int deleter_run = 0;

    MoveAssignableResource<true> res;
    MoveAssignableDeleter<true, MoveAssignableResource<true>&> del(deleter_run);

    {
        using ur_t = wwa::utils::unique_resource<
            MoveAssignableResource<true>, MoveAssignableDeleter<true, MoveAssignableResource<true>&>>;

        ur_t f1(res, del);
        f1 = std::move(f1);  // Self-assignment

        EXPECT_EQ(deleter_run, 0);  // Should not call deleter on self-assignment
    }

    EXPECT_EQ(deleter_run, 1);  // Deleter called once on destruction
}
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

TEST(UniqueResource, MultipleReleases)
{
    int deleter_run = 0;

    Resource<true> res;
    Deleter<true, Resource<true>&> del(deleter_run);

    {
        wwa::utils::unique_resource<Resource<true>&, Deleter<true, Resource<true>&>> f(res, del);
        f.release();
        f.release();  // Second release should be safe
        f.release();  // Third release should be safe

        EXPECT_EQ(deleter_run, 0);
    }

    EXPECT_EQ(deleter_run, 0);  // No deleter calls after releases
}
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

TEST(UniqueResource, MultipleResets)
{
    int deleter_run = 0;

    Resource<true> res;
    Deleter<true, Resource<true>&> del(deleter_run);

    {
        wwa::utils::unique_resource<Resource<true>&, Deleter<true, Resource<true>&>> f(res, del);
        f.reset();
        EXPECT_EQ(deleter_run, 1);
        
        f.reset();  // Second reset should be safe
        EXPECT_EQ(deleter_run, 1);  // No additional deleter calls
        
        f.reset();  // Third reset should be safe
        EXPECT_EQ(deleter_run, 1);  // Still no additional deleter calls
    }

    EXPECT_EQ(deleter_run, 1);  // Only one deleter call total
}
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

TEST(UniqueResource, ResetAfterRelease)
{
    int deleter_run = 0;

    Resource<true> res1;
    Resource<true> res2;
    Deleter<true, Resource<true>&> del(deleter_run);

    {
        wwa::utils::unique_resource<Resource<true>&, Deleter<true, Resource<true>&>> f(res1, del);
        f.release();
        EXPECT_EQ(deleter_run, 0);
        
        f.reset(res2);  // Reset with new resource after release
        EXPECT_EQ(deleter_run, 0);  // No deleter call for released resource
    }

    EXPECT_EQ(deleter_run, 1);  // Deleter called for res2 on destruction
}
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

TEST(UniqueResource, ConstCorrectness)

TEST(UniqueResource, ConstructorExceptionSafety)

TEST(UniqueResource, BasicConstructorPatterns)

TEST(UniqueResource, DifferentResourceTypes)

TEST(UniqueResource, DeleterAccess)

TEST(UniqueResource, ResourceComparison)

TEST(UniqueResource, MoveOperationsStress)

TEST(UniqueResource, MakeCheckedEdgeCases)

TEST(UniqueResource, RAIIExceptionBehavior)

TEST(UniqueResource, TemplateTypeDeduction)

TEST(UniqueResource, ReleaseAndGetInteraction)

TEST(UniqueResource, NoexceptSpecifications)
{
    // Test that key operations are noexcept when they should be
    using SimpleResource = wwa::utils::unique_resource<int*, void(*)(int*)>;
    
    static_assert(noexcept(std::declval<SimpleResource&>().get()));
    static_assert(noexcept(std::declval<SimpleResource&>().get_deleter()));
    static_assert(noexcept(std::declval<SimpleResource&>().release()));
    static_assert(noexcept(std::declval<SimpleResource&>().reset()));
    
    // Verify that pointer access operators are noexcept for pointer types
    static_assert(noexcept(*std::declval<SimpleResource&>()));
    static_assert(noexcept(std::declval<SimpleResource&>().operator->()));
    
    // This test mainly checks compile-time properties
    SUCCEED();
}
{
    int deleter_run = 0;
    auto del = [&deleter_run](int* p) { delete p; ++deleter_run; };
    
    int* original_ptr = new int(42);
    
    {
        wwa::utils::unique_resource<int*, decltype(del)> obj(original_ptr, del);
        EXPECT_EQ(obj.get(), original_ptr);
        
        obj.release();
        // After release, get() should still return the original pointer
        EXPECT_EQ(obj.get(), original_ptr);
        
        // But deleter should not be called on destruction
    }
    
    EXPECT_EQ(deleter_run, 0);
    
    // Manually clean up since release was called
    delete original_ptr;
}
{
    int deleter_run = 0;
    
    {
        // Test template argument deduction with lambda
        auto del = [&deleter_run](int* p) { delete p; ++deleter_run; };
        wwa::utils::unique_resource obj(new int(42), del);
        
        ASSERT_NE(obj.get(), nullptr);
        EXPECT_EQ(*obj.get(), 42);
    }
    
    EXPECT_EQ(deleter_run, 1);
    
    deleter_run = 0;
    
    {
        // Test deduction with function pointer
        void (*del_func)(int*) = [](int* p) { delete p; };
        wwa::utils::unique_resource obj(new int(123), del_func);
        
        ASSERT_NE(obj.get(), nullptr);
        EXPECT_EQ(*obj.get(), 123);
    }
}
{
    int deleter_run = 0;
    
    struct ThrowingDeleter {
        int& counter;
        bool should_throw;
        
        ThrowingDeleter(int& c, bool throw_flag = false) : counter(c), should_throw(throw_flag) {}
        
        void operator()(int* p) {
            delete p;
            ++counter;
            if (should_throw) {
                throw std::runtime_error("Deleter exception");
            }
        }
    };
    
    {
        // Test normal RAII behavior
        ThrowingDeleter del(deleter_run, false);
        {
            wwa::utils::unique_resource<int*, ThrowingDeleter> obj(new int(42), del);
            ASSERT_NE(obj.get(), nullptr);
            EXPECT_EQ(*obj.get(), 42);
        }  // Destructor should call deleter
        
        EXPECT_EQ(deleter_run, 1);
    }
    
    deleter_run = 0;
    
    {
        // Test RAII with early release
        ThrowingDeleter del(deleter_run, false);
        {
            wwa::utils::unique_resource<int*, ThrowingDeleter> obj(new int(42), del);
            obj.release();
        }  // Destructor should NOT call deleter
        
        EXPECT_EQ(deleter_run, 0);
    }
}
{
    int deleter_run = 0;
    auto del = [&deleter_run](int* p) { if (p) delete p; ++deleter_run; };
    
    {
        // Test with zero as invalid value
        auto obj = wwa::utils::make_unique_resource_checked(new int(42), static_cast<int*>(nullptr), del);
        ASSERT_NE(obj.get(), nullptr);
        EXPECT_EQ(*obj.get(), 42);
    }
    
    EXPECT_EQ(deleter_run, 1);
    
    deleter_run = 0;
    
    {
        // Test with -1 as invalid value
        int* valid_ptr = new int(123);
        auto obj = wwa::utils::make_unique_resource_checked(valid_ptr, reinterpret_cast<int*>(-1), del);
        ASSERT_NE(obj.get(), nullptr);
        EXPECT_EQ(*obj.get(), 123);
    }
    
    EXPECT_EQ(deleter_run, 1);
    
    deleter_run = 0;
    
    {
        // Test with custom invalid value type
        struct Handle { int value; bool operator==(const Handle& other) const { return value == other.value; } };
        auto del_handle = [&deleter_run](Handle) { ++deleter_run; };
        Handle valid{42};
        Handle invalid{-1};
        auto obj = wwa::utils::make_unique_resource_checked(valid, invalid, del_handle);
        EXPECT_EQ(obj.get().value, 42);
    }
    
    EXPECT_EQ(deleter_run, 1);
}
{
    int deleter_run = 0;
    auto del = [&deleter_run](int* p) { delete p; ++deleter_run; };
    
    {
        std::vector<wwa::utils::unique_resource<int*, decltype(del)>> resources;
        
        // Create multiple resources
        for (int i = 0; i < 10; ++i) {
            resources.emplace_back(new int(i), del);
        }
        
        // Verify all resources are valid
        for (size_t i = 0; i < resources.size(); ++i) {
            ASSERT_NE(resources[i].get(), nullptr);
            EXPECT_EQ(*resources[i].get(), static_cast<int>(i));
        }
        
        // Move resources around
        auto moved = std::move(resources[5]);
        EXPECT_EQ(resources[5].get(), nullptr);  // Moved-from state
        ASSERT_NE(moved.get(), nullptr);
        EXPECT_EQ(*moved.get(), 5);
        
        // Move assign
        resources[0] = std::move(moved);
        EXPECT_EQ(moved.get(), nullptr);  // Moved-from state
        ASSERT_NE(resources[0].get(), nullptr);
        EXPECT_EQ(*resources[0].get(), 5);
    }
    
    EXPECT_EQ(deleter_run, 10);  // All resources should be cleaned up
}
{
    int deleter_run = 0;
    auto del = [&deleter_run](int* p) { delete p; ++deleter_run; };
    
    int* ptr1 = new int(42);
    int* ptr2 = new int(43);
    
    {
        wwa::utils::unique_resource<int*, decltype(del)> obj1(ptr1, del);
        wwa::utils::unique_resource<int*, decltype(del)> obj2(ptr2, del);
        
        // Test pointer comparison
        EXPECT_EQ(obj1.get(), ptr1);
        EXPECT_EQ(obj2.get(), ptr2);
        EXPECT_NE(obj1.get(), obj2.get());
        
        // Test value comparison
        EXPECT_EQ(*obj1.get(), 42);
        EXPECT_EQ(*obj2.get(), 43);
        EXPECT_NE(*obj1.get(), *obj2.get());
    }
    
    EXPECT_EQ(deleter_run, 2);
}
{
    struct StatefulDeleter {
        int& counter;
        int state = 42;
        
        StatefulDeleter(int& c) : counter(c) {}
        
        void operator()(int* p) {
            delete p;
            counter += state;
        }
        
        void setState(int s) { state = s; }
        int getState() const { return state; }
    };
    
    int deleter_run = 0;
    
    {
        StatefulDeleter del(deleter_run);
        wwa::utils::unique_resource<int*, StatefulDeleter> obj(new int(123), del);
        
        // Test deleter access
        EXPECT_EQ(obj.get_deleter().getState(), 42);
        
        // Test deleter modification via non-const access
        const_cast<StatefulDeleter&>(obj.get_deleter()).setState(100);
        EXPECT_EQ(obj.get_deleter().getState(), 100);
    }
    
    EXPECT_EQ(deleter_run, 100);  // Should use modified state
}
{
    int deleter_run = 0;
    
    {
        // Test with array pointer
        auto del = [&deleter_run](int* p) { delete[] p; ++deleter_run; };
        wwa::utils::unique_resource<int*, decltype(del)> obj(new int[5]{1, 2, 3, 4, 5}, del);
        ASSERT_NE(obj.get(), nullptr);
        EXPECT_EQ(obj.get()[0], 1);
        EXPECT_EQ(obj.get()[4], 5);
    }
    
    EXPECT_EQ(deleter_run, 1);
    
    deleter_run = 0;
    
    {
        // Test with function pointer resource
        auto del = [&deleter_run](void(*)()) { ++deleter_run; };
        auto dummy_func = []() {};
        wwa::utils::unique_resource<void(*)(), decltype(del)> obj(dummy_func, del);
        EXPECT_EQ(obj.get(), dummy_func);
    }
    
    EXPECT_EQ(deleter_run, 1);
}
{
    int deleter_run = 0;
    
    {
        // Test direct constructor with raw pointer
        auto del = [&deleter_run](int* p) { delete p; ++deleter_run; };
        wwa::utils::unique_resource<int*, decltype(del)> obj(new int(123), del);
        ASSERT_NE(obj.get(), nullptr);
        EXPECT_EQ(*obj.get(), 123);
    }
    
    EXPECT_EQ(deleter_run, 1);
    
    deleter_run = 0;
    
    {
        // Test constructor with lambda capture
        int capture_value = 456;
        auto del = [&deleter_run, capture_value](int* p) { 
            EXPECT_EQ(capture_value, 456);
            delete p; 
            ++deleter_run; 
        };
        wwa::utils::unique_resource<int*, decltype(del)> obj(new int(capture_value), del);
        ASSERT_NE(obj.get(), nullptr);
        EXPECT_EQ(*obj.get(), 456);
    }
    
    EXPECT_EQ(deleter_run, 1);
}
{
    int deleter_run = 0;
    
    struct ThrowingResource {
        ThrowingResource() { throw std::runtime_error("Construction failed"); }
    };
    
    auto del = [&deleter_run](ThrowingResource* p) { delete p; ++deleter_run; };
    
    {
        // Test that deleter is not called if resource construction throws
        EXPECT_THROW({
            ThrowingResource* ptr = nullptr;
            try {
                ptr = new ThrowingResource();
            } catch (...) {
                // Resource construction failed, unique_resource should not be created
                throw;
            }
            wwa::utils::unique_resource<ThrowingResource*, decltype(del)> obj(ptr, del);
        }, std::runtime_error);
    }
    
    EXPECT_EQ(deleter_run, 0);  // Deleter should not be called
}
{
    struct TestResource {
        int value = 42;
        int getValue() const { return value; }
        void setValue(int v) { value = v; }
    };

    int deleter_run = 0;
    auto del = [&deleter_run](TestResource* p) { delete p; ++deleter_run; };

    {
        const auto obj = wwa::utils::make_unique_resource_checked(new TestResource(), nullptr, del);
        ASSERT_NE(obj.get(), nullptr);
        
        // Test const access
        EXPECT_EQ(obj->getValue(), 42);
        EXPECT_EQ((*obj).getValue(), 42);
        
        // Verify const get() works
        const TestResource* const_ptr = obj.get();
        EXPECT_EQ(const_ptr->getValue(), 42);
    }

    EXPECT_EQ(deleter_run, 1);
}
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
