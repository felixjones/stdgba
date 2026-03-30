# Testing, Assertions & Benchmarking

stdgba provides lightweight APIs for unit testing, assertions, and cycle-accurate benchmarking on hardware or emulator.

## Test API

The `gba::test` singleton provides simple assertion and expectation checking. Tests run on real GBA hardware or mGBA emulator, with results reported via log output.

### Basic test structure

```cpp
#include <gba/testing>

int main() {
    gba::test("example test case", [] {
        gba::test.expect.eq(2 + 2, 4);
    });

    return gba::test.finish(); // Must call finish() to exit
}
```

Every test must:
1. Call `gba::test(name, lambda)` to define a test case
2. Use `gba::test.expect.*` or `gba::test.assert.*` inside the lambda
3. Call `gba::test.finish()` at the end of `main()`

The test framework automatically exits via SWI 0x1A (or a custom exit SWI in `-DSTDGBA_EXIT_SWI=0x##`).

### Expectation checks

Expectations continue execution on failure and count failures for the final report:

```cpp
gba::test("expectations", [] {
    gba::test.expect.eq(2 + 2, 4, "arithmetic");                    // Pass
    gba::test.expect.ne(0, 1, "inequality");                        // Pass
    gba::test.expect.lt(1, 2);                                      // Pass
    gba::test.expect.le(1, 1);                                      // Pass
    gba::test.expect.gt(2, 1);                                      // Pass
    gba::test.expect.ge(1, 1);                                      // Pass
    gba::test.expect.is_true(true);                                 // Pass
    gba::test.expect.is_false(false);                               // Pass
    gba::test.expect.is_zero(0);                                    // Pass
    gba::test.expect.at_least(5, 3);                                // Pass (5 >= 3)
});
```

### Assertion checks

Assertions stop execution on failure immediately:

```cpp
gba::test("assertions", [] {
    gba::test.assert.eq(5, 5);                                      // Pass, continue
    gba::test.assert.eq(5, 6);                                      // FAIL, stop test
    gba::test.expect.eq(1, 1);                                      // Never reached
});
```

### Range and container checks

Test ranges and containers element-wise:

```cpp
#include <array>
#include <gba/testing>

int main() {
    gba::test("ranges", [] {
        std::array<int, 3> a = {1, 2, 3};
        std::array<int, 3> b = {1, 2, 3};
        gba::test.expect.range_eq(a, b, "array equality");

        std::array<int, 3> c = {1, 2, 4};
        gba::test.expect.range_ne(a, c, "array inequality");
    });

    return gba::test.finish();
}
```

### Running tests on mGBA

Build your test executable, then run with `mgba-headless`:

```bash
# Build
cmake --build build --target my_test -- -j 8

# Run (exit SWI 0x1A, return exit code in r0, timeout 10 seconds)
timeout 15 mgba-headless -S 0x1A -R r0 -t 10 build/tests/my_test.elf
echo "Exit code: $?"
```

The test framework writes results to the logger, viewable via:
- `mGBA` debug console (Ctrl+D or Tools → GDB)
- `no$gba` debug window
- Custom logger backend

## Benchmark API

The `gba::benchmark` module provides cycle-accurate timing using cascading hardware timers.

### Cycle counter

A `cycle_counter` wraps two cascading timers to form a 32-bit counter:

```cpp
#include <gba/benchmark>

gba::benchmark::cycle_counter counter;
counter.start();
// ... code to measure ...
unsigned int cycles = counter.stop();
```

By default, `cycle_counter` uses TM2+TM3, leaving TM0+TM1 free for audio or other uses. Override via:

```cpp
using namespace gba::benchmark;
cycle_counter counter(make_timer_pair(timer_pair_id::tm0_tm1));
```

Valid pairs: `(0,1)`, `(1,2)`, `(2,3)`.

### Measuring code

Use `measure()` to run a function and return its cycle cost:

```cpp
#include <gba/benchmark>

unsigned int work(unsigned int n) {
    unsigned int sum = 0;
    for (unsigned int i = 0; i < n; ++i) {
        sum += i;
    }
    return sum;
}

int main() {
    // Measure one run
    auto cycles = gba::benchmark::measure(work, 1024u);
    
    // Measure and average 8 runs
    auto avg = gba::benchmark::measure_avg(8, work, 1024u);

    return 0;
}
```

`measure()` returns the cycle count. `measure_avg()` runs the function N times and returns the average, reducing noise from interrupts or cache effects.

### Preventing dead-code elimination

Use `do_not_optimize()` to wrap code so the compiler cannot eliminate it:

```cpp
#include <gba/benchmark>

gba::benchmark::cycle_counter counter;
counter.start();

gba::benchmark::do_not_optimize([&] {
    // Compiler cannot dead-code eliminate or reorder this
    volatile unsigned int x = 0;
    for (int i = 0; i < 100; ++i) x += i;
});

auto cycles = counter.stop();
```

Without `do_not_optimize()`, the compiler may optimize away unused computations, giving misleading cycle counts.

## Combined example

Test a function with both assertions and benchmarks:

```cpp
#include <gba/benchmark>
#include <gba/testing>

// Function under test
unsigned int sum_of_squares(unsigned int n) {
    unsigned int sum = 0;
    for (unsigned int i = 1; i <= n; ++i) {
        sum += i * i;
    }
    return sum;
}

int main() {
    // Unit test
    gba::test("sum_of_squares", [] {
        gba::test.expect.eq(sum_of_squares(1), 1, "sum(1) = 1");
        gba::test.expect.eq(sum_of_squares(3), 14, "sum(1..3) = 14");
        gba::test.expect.eq(sum_of_squares(5), 55, "sum(1..5) = 55");
    });

    // Benchmark
    gba::test("sum_of_squares benchmark", [] {
        using namespace gba::benchmark;
        auto cycles = measure_avg(4, sum_of_squares, 100u);
        gba::test.expect.lt(cycles, 5000, "reasonable cycle cost");
    });

    return gba::test.finish();
}
```

## Tips & Best Practices

- **Always call `gba::test.finish()`**: It flushes logs and signals the exit SWI to mgba-headless.
- **Use `expect.*` for non-critical checks**: Failures don't stop the test, so you can gather multiple failures at once.
- **Use `assert.*` for setup validation**: Stop immediately if preconditions fail, preventing cascade failures.
- **Add descriptive messages**: The third parameter makes test failure output readable.
- **Benchmark multiple runs**: Use `measure_avg()` to reduce noise from VBlank interrupts.
- **Isolate what you measure**: Wrap only the code under test with `do_not_optimize()`.
- **Test on hardware too**: emulator behavior may differ from real GBA in timing or memory access patterns.

## Reference

| Function | Purpose |
|----------|---------|
| `gba::test(name, fn)` | Run test case |
| `gba::test.expect.eq(a, b)` | Expect `a == b` |
| `gba::test.expect.ne(a, b)` | Expect `a != b` |
| `gba::test.expect.lt(a, b)` | Expect `a < b` |
| `gba::test.expect.le(a, b)` | Expect `a <= b` |
| `gba::test.expect.gt(a, b)` | Expect `a > b` |
| `gba::test.expect.ge(a, b)` | Expect `a >= b` |
| `gba::test.expect.is_true(x)` | Expect `x` is true |
| `gba::test.expect.is_false(x)` | Expect `x` is false |
| `gba::test.expect.is_zero(x)` | Expect `x == 0` |
| `gba::test.expect.range_eq(a, b)` | Expect ranges `a` and `b` are equal |
| `gba::test.expect.range_ne(a, b)` | Expect ranges `a` and `b` are not equal |
| `gba::test.assert.*` | Same as expect, but stops on failure |
| `gba::test.finish()` | Exit the test (required) |
| `gba::benchmark::measure(fn, args...)` | Measure cycles for one run |
| `gba::benchmark::measure_avg(n, fn, args...)` | Measure and average N runs |
| `gba::benchmark::do_not_optimize(fn)` | Prevent dead-code elimination |
| `gba::benchmark::cycle_counter` | Manual 32-bit timer pair counter |
