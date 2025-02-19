#include <gtest/gtest.h>

#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>

#include "scope_action.h"

static_assert(!std::is_copy_constructible_v<wwa::utils::exit_action<void (*)()>>);
static_assert(!std::is_copy_assignable_v<wwa::utils::exit_action<void (*)()>>);
static_assert(!std::is_move_assignable_v<wwa::utils::exit_action<void (*)()>>);

namespace {

int j = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

void f(int& i)
{
    i += 1;
}

void g()
{
    j += 1;
}

}  // namespace

// The following tests are adapted from MS GSL tests.

TEST(ExitAction, Lambda)
{
    int i = 0;

    {
        auto _ = wwa::utils::exit_action([&i]() { f(i); });
        EXPECT_EQ(i, 0);
    }

    EXPECT_EQ(i, 1);
}

TEST(ExitAction, LambdaMove)
{
    int i = 0;

    {
        auto _1 = wwa::utils::exit_action([&i]() { f(i); });

        {
            auto _2 = std::move(_1);
            EXPECT_EQ(i, 0);
        }

        EXPECT_EQ(i, 1);

        {
            auto _2 = std::move(_1);  // NOLINT(bugprone-use-after-move)
            EXPECT_EQ(i, 1);
        }

        EXPECT_EQ(i, 1);
    }

    EXPECT_EQ(i, 1);
}

TEST(ExitAction, ConstLValueLambda)
{
    int i = 0;

    {
        const auto const_lvalue_lambda = [&i]() { f(i); };
        auto _                         = wwa::utils::exit_action(const_lvalue_lambda);
        EXPECT_EQ(i, 0);
    }

    EXPECT_EQ(i, 1);
}

TEST(ExitAction, MutableLValueLambda)
{
    int i = 0;

    {
        auto mutable_lvalue_lambda = [&i]() { f(i); };
        auto _                     = wwa::utils::exit_action(mutable_lvalue_lambda);
        EXPECT_EQ(i, 0);
    }

    EXPECT_EQ(i, 1);
}

TEST(ExitAction, FunctionPtr)
{
    j = 0;

    {
        auto _ = wwa::utils::exit_action(&g);
        EXPECT_EQ(j, 0);
    }

    EXPECT_EQ(j, 1);
}

TEST(ExitAction, Function)
{
    j = 0;

    {
        auto _ = wwa::utils::exit_action(g);
        EXPECT_EQ(j, 0);
    }

    EXPECT_EQ(j, 1);
}

// The following tests are adapted from GSL Lite tests.

TEST(ExitAction, LambdaOnLeavingScope)
{
    struct F {
        static void incr(int& i) { i += 1; }
    };

    int i = 0;

    {
        auto _ = wwa::utils::exit_action([&i]() { F::incr(i); });
        EXPECT_EQ(i, 0);
    }

    EXPECT_EQ(i, 1);
}

TEST(ExitAction, FunctionBindOnLeavingScope)
{
    struct F {
        static void incr(int& i) { i += 1; }
    };

    int i = 0;

    {
        // NOLINTNEXTLINE(modernize-avoid-bind)
        auto _ = wwa::utils::exit_action(std::bind(&F::incr, std::ref(i)));
        EXPECT_EQ(i, 0);
    }

    EXPECT_EQ(i, 1);
}

TEST(ExitAction, FunctionPtrOnLeavingScope)
{
    struct F {
        static void incr() { j += 1; }
    };

    j = 0;

    {
        auto _ = wwa::utils::exit_action(&F::incr);
        EXPECT_EQ(j, 0);
    }

    EXPECT_EQ(j, 1);
}

TEST(ExitAction, Movable)
{
    struct F {
        static void incr(int& i) { i += 1; }
    };

    int i = 0;

    {
        auto _1 = wwa::utils::exit_action([&i]() { F::incr(i); });

        {
            auto _2 = std::move(_1);
            EXPECT_EQ(i, 0);
        }

        EXPECT_EQ(i, 1);

        {
            auto _2 = std::move(_1);  // NOLINT(bugprone-use-after-move)
            EXPECT_EQ(i, 1);
        }

        EXPECT_EQ(i, 1);
    }

    EXPECT_EQ(i, 1);
}

// Original tests :-)

TEST(ExitAction, CopyCanThrow)
{
    class action {
    public:
        action(int& i) : m_i(&i) {}
        [[noreturn]] action(const action& other) noexcept(false) : m_i(other.m_i)
        {
            throw std::runtime_error("action copy ctor");
        }

        void operator()() { *m_i += 1; }

    private:
        int* m_i;
    };

    int i = 0;
    {
        {
            action a(i);
            EXPECT_THROW(static_cast<void>(wwa::utils::exit_action(a)), std::runtime_error);
        }

        EXPECT_EQ(i, 1);
    }
}

TEST(ExitAction, MoveAction)
{
    class action {
    public:
        action(int& i) : m_i(&i) {}

        action(const action& other) noexcept = default;
        action(action&& other)               = delete;

        void operator()() { *m_i += 1; }

    private:
        int* m_i;
    };

    int i = 0;
    {
        {
            action a(i);
            auto _ = wwa::utils::exit_action(a);

            {
                auto _2 = std::move(_);
                EXPECT_EQ(i, 0);
            }

            EXPECT_EQ(i, 1);
        }

        EXPECT_EQ(i, 1);
    }
}
