/// @file tests/memory/test_function.cpp
/// @brief Unit tests for function type.

#include <gba/functional>
#include <gba/testing>

namespace {
    int global_call_count = 0;
    int global_arg_sum = 0;

    void free_function(int x) {
        global_call_count++;
        global_arg_sum += x;
    }

    struct Functor {
        int value = 0;
        void operator()(int x) { value += x; }
    };
} // namespace

int main() {
    using namespace gba;

    // Test default construction
    {
        handler<int> h;
        gba::test.is_true(h == handler<int>{});
    }

    // Test function pointer
    {
        global_call_count = 0;
        global_arg_sum = 0;

        handler<int> h{free_function};
        h(42);

        gba::test.eq(global_call_count, 1);
        gba::test.eq(global_arg_sum, 42);
    }

    // Test lambda
    {
        int captured = 0;
        handler<int> h{[&captured](int x) { captured += x; }};
        h(10);
        h(20);

        gba::test.eq(captured, 30);
    }

    // Test copy construction
    {
        global_call_count = 0;

        handler<int> h1{free_function};
        handler<int> h2{h1};

        h1(1);
        h2(1);

        gba::test.eq(global_call_count, 2);
    }

    // Test move construction
    {
        int captured = 0;
        handler<int> h1{[&captured](int x) { captured = x; }};
        handler<int> h2{std::move(h1)};

        h2(99);
        gba::test.eq(captured, 99);
    }

    // Test copy assignment
    {
        global_call_count = 0;

        handler<int> h1{free_function};
        handler<int> h2;

        h2 = h1;
        h2(1);

        gba::test.eq(global_call_count, 1);
    }

    // Test move assignment
    {
        int captured = 0;
        handler<int> h1{[&captured](int x) { captured = x; }};
        handler<int> h2;

        h2 = std::move(h1);
        h2(77);

        gba::test.eq(captured, 77);
    }

    // Test equality
    {
        handler<int> h1;
        handler<int> h2;
        gba::test.is_true(h1 == h2);

        handler<int> h3{free_function};
        gba::test.is_false(h1 == h3);
    }

    // Test small object optimization (fits in storage)
    {
        int x = 0;
        handler<int> h{[&x](int v) { x = v; }};
        h(123);
        gba::test.eq(x, 123);
    }

    // Test handler with no arguments
    {
        int called = 0;
        handler<> h{[&called]() { called++; }};
        h();
        h();
        gba::test.eq(called, 2);
    }

    // Test handler with multiple arguments
    {
        int result = 0;
        handler<int, int, int> h{[&result](int a, int b, int c) { result = a + b + c; }};
        h(1, 2, 3);
        gba::test.eq(result, 6);
    }

    // Test function with return value
    {
        function<int(int, int)> h{[](int a, int b) { return a + b; }};
        gba::test.eq(h(3, 4), 7);
    }

    return gba::test.finish();
}
