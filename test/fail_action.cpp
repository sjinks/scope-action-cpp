#include <gtest/gtest.h>

#include <stdexcept>
#include <utility>

#include "scope_action.h"

static_assert(!std::is_copy_constructible_v<wwa::utils::fail_action<void (*)()>>);
static_assert(!std::is_copy_assignable_v<wwa::utils::fail_action<void (*)()>>);
static_assert(!std::is_move_assignable_v<wwa::utils::fail_action<void (*)()>>);

namespace {

int j = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace

struct X {
    static void incr(int& i) { i += 1; }
};

// Adapted from GSL Lite

TEST(FailAction, LeaveScopeWithException)
{
    struct F {
        static void incr() { j += 1; }

        static void pass()
        {
            try {
                auto _ = wwa::utils::fail_action(&F::incr);
            }
            catch (...) {
                FAIL();
            }
        }

        static void fail()
        {
            bool success = false;
            try {
                auto _ = wwa::utils::fail_action(&F::incr);
                throw std::exception();
            }
            catch (...) {
                success = true;
            }

            EXPECT_TRUE(success);
        }
    };

    struct G {
        ~G() { F::pass(); }
    };

    {
        j = 0;
        F::pass();
        EXPECT_EQ(j, 0);
    }

    {
        j = 0;
        F::fail();
        EXPECT_EQ(j, 1);
    }

    {
        j = 0;
        try {
            G g;
            throw std::exception();
        }
        catch (...) {
            EXPECT_EQ(j, 0);
        }

        EXPECT_EQ(j, 0);
    }
}

// Adapted from MS GSL

TEST(FailAction, Lambda)
{
    int i = 0;

    try {
        auto _ = wwa::utils::fail_action([&i]() { X::incr(i); });
        EXPECT_EQ(i, 0);
        throw std::runtime_error("error");
    }
    catch (const std::runtime_error&) {
        EXPECT_EQ(i, 1);
    }

    EXPECT_EQ(i, 1);
}

TEST(FailAction, LambdaMove)
{
    int i = 0;

    {
        auto _1 = wwa::utils::fail_action([&i]() { X::incr(i); });

        try {
            auto _2 = std::move(_1);
            EXPECT_EQ(i, 0);
            throw std::runtime_error("error");
        }
        catch (const std::runtime_error&) {
            EXPECT_EQ(i, 1);
        }

        EXPECT_EQ(i, 1);

        try {
            auto _2 = std::move(_1);
            EXPECT_EQ(i, 1);
            throw std::runtime_error("error");
        }
        catch (const std::runtime_error&) {
            EXPECT_EQ(i, 1);
        }

        EXPECT_EQ(i, 1);

        {
            auto _3 = std::move(_1);
            EXPECT_EQ(i, 1);
        }
    }

    EXPECT_EQ(i, 1);
}

TEST(FailAction, ConstLValueLambda)
{
    int i = 0;

    try {
        const auto const_lvalue_lambda = [&i]() { X::incr(i); };
        auto _                         = wwa::utils::fail_action(const_lvalue_lambda);
        EXPECT_EQ(i, 0);
        throw std::runtime_error("error");
    }
    catch (const std::runtime_error&) {
        EXPECT_EQ(i, 1);
    }

    EXPECT_EQ(i, 1);
}

TEST(FailAction, MutableLValueLambda)
{
    int i = 0;

    try {
        auto mutable_lvalue_lambda = [&i]() { X::incr(i); };
        auto _                     = wwa::utils::fail_action(mutable_lvalue_lambda);
        EXPECT_EQ(i, 0);
        throw std::runtime_error("error");
    }
    catch (const std::runtime_error&) {
        EXPECT_EQ(i, 1);
    }

    EXPECT_EQ(i, 1);
}

// Original

// !std::is_nothrow_move_constructible_v<Func> && std::is_copy_constructible_v<Func>
TEST(FailAction, MoveAction)
{
    class action {
    public:
        action(int& i) : m_i(&i) {}

        action(const action& other) noexcept = default;
        action(action&& other)               = delete;

        void operator()() { *this->m_i += 1; }

    private:
        int* m_i;
    };

    int i = 0;
    {
        {
            action a(i);
            auto _ = wwa::utils::fail_action(a);

            {
                auto _2 = std::move(_);
                EXPECT_EQ(i, 0);
            }

            EXPECT_EQ(i, 0);
        }

        EXPECT_EQ(i, 0);
    }
}
