/// @file tests/memory/test_handler.cpp
/// @brief Unit tests for handler type.

#include <gba/bits/handler.hpp>

#include <mgba_test.hpp>

namespace {
    int global_call_count = 0;
    int global_arg_sum = 0;

    void free_function(int x) {
        global_call_count++;
        global_arg_sum += x;
    }

    struct Functor {
        int value = 0;
        void operator()(int x) {
            value += x;
        }
    };
}

int main() {
    using namespace gba::bits;

    // Test default construction
    {
        handler<int> h;
        ASSERT_TRUE(h == handler<int>{});
    }

    // Test function pointer
    {
        global_call_count = 0;
        global_arg_sum = 0;

        handler<int> h{free_function};
        h(42);

        ASSERT_EQ(global_call_count, 1);
        ASSERT_EQ(global_arg_sum, 42);
    }

    // Test lambda
    {
        int captured = 0;
        handler<int> h{[&captured](int x) { captured += x; }};
        h(10);
        h(20);

        ASSERT_EQ(captured, 30);
    }

    // Test copy construction
    {
        global_call_count = 0;

        handler<int> h1{free_function};
        handler<int> h2{h1};

        h1(1);
        h2(1);

        ASSERT_EQ(global_call_count, 2);
    }

    // Test move construction
    {
        int captured = 0;
        handler<int> h1{[&captured](int x) { captured = x; }};
        handler<int> h2{std::move(h1)};

        h2(99);
        ASSERT_EQ(captured, 99);
    }

    // Test copy assignment
    {
        global_call_count = 0;

        handler<int> h1{free_function};
        handler<int> h2;

        h2 = h1;
        h2(1);

        ASSERT_EQ(global_call_count, 1);
    }

    // Test move assignment
    {
        int captured = 0;
        handler<int> h1{[&captured](int x) { captured = x; }};
        handler<int> h2;

        h2 = std::move(h1);
        h2(77);

        ASSERT_EQ(captured, 77);
    }

    // Test equality
    {
        handler<int> h1;
        handler<int> h2;
        ASSERT_TRUE(h1 == h2);

        handler<int> h3{free_function};
        ASSERT_FALSE(h1 == h3);
    }

    // Test small object optimization (fits in storage)
    {
        // Lambda that captures one pointer - should fit in 12 bytes
        int x = 0;
        handler<int> h{[&x](int v) { x = v; }};
        h(123);
        ASSERT_EQ(x, 123);
    }

    // Test handler with no arguments
    {
        int called = 0;
        handler<> h{[&called]() { called++; }};
        h();
        h();
        ASSERT_EQ(called, 2);
    }

    // Test handler with multiple arguments
    {
        int result = 0;
        handler<int, int, int> h{[&result](int a, int b, int c) { result = a + b + c; }};
        h(1, 2, 3);
        ASSERT_EQ(result, 6);
    }

    test::finalize();
}
