/// @file test_benchmark.cpp
/// @brief Tests for public gba/benchmark API.

#include <gba/benchmark>
#include <gba/testing>

namespace {

    volatile unsigned int sink = 0;

    void burn(unsigned int count) {
        for (unsigned int i = 0; i < count; ++i) {
            sink += i;
        }
    }

    void noop() {
        asm volatile("" ::: "memory");
    }

} // namespace

int main() {
    // Default measure path should configure TM2+TM3 while the lambda runs.
    {
        const auto cycles = gba::benchmark::measure([] {
            const auto low = gba::reg_tmcnt_h[2].value();
            const auto high = gba::reg_tmcnt_h[3].value();
            gba::test.is_true(low.enabled, "low timer enabled");
            gba::test.is_true(high.enabled, "high timer enabled");
            gba::test.is_true(high.cascade, "high timer cascades");
            gba::test.eq(low.cycles, gba::cycles_1, "low timer cycle mode");
        });
        gba::test.gtz(cycles, "counter stop returns non-zero cycles");
    }

    // Basic measurement should report non-zero elapsed cycles.
    {
        const auto cycles = gba::benchmark::measure(burn, 1024u);
        gba::test.gtz(cycles, "measure reports cycles");
    }

    // Average measurement should also be non-zero.
    {
        const auto avg = gba::benchmark::measure_avg(8, burn, 1024u);
        gba::test.gtz(avg, "measure_avg reports cycles");
    }

    // Overhead calibration should produce a positive baseline.
    {
        const auto overhead = gba::benchmark::calibrate_overhead();
        gba::test.gtz(overhead, "calibrate_overhead reports cycles");
    }

    // Custom timer pair support (TM0+TM1).
    {
        constexpr gba::benchmark::timer_pair pair{0, 1};
        const auto pair_cycles = gba::benchmark::measure_on(pair, [] {
            const auto low = gba::reg_tmcnt_h[0].value();
            const auto high = gba::reg_tmcnt_h[1].value();
            gba::test.is_true(low.enabled, "pair low timer enabled");
            gba::test.is_true(high.enabled, "pair high timer enabled");
            gba::test.is_true(high.cascade, "pair high timer cascades");
            gba::test.eq(low.cycles, gba::cycles_1, "pair low timer cycle mode");
        });
        gba::test.gtz(pair_cycles, "measure_on reports cycles");

        const auto avg = gba::benchmark::measure_avg_on(pair, 4, burn, 256u);
        gba::test.gtz(avg, "measure_avg_on reports cycles");

        const auto overhead = gba::benchmark::calibrate_overhead_on(pair);
        gba::test.gtz(overhead, "calibrate_overhead_on reports cycles");
    }

    // Net measurement should not exceed gross measurement.
    {
        const auto gross = gba::benchmark::measure(burn, 1024u);
        const auto net = gba::benchmark::measure_net(burn, 1024u);
        gba::test.le(net, gross, "measure_net is bounded by gross");
    }

    // Net average measurement should be bounded by gross average.
    {
        const auto gross_avg = gba::benchmark::measure_avg(8, burn, 1024u);
        const auto net_avg = gba::benchmark::measure_avg_net(8, burn, 1024u);
        gba::test.le(net_avg, gross_avg, "measure_avg_net is bounded by gross average");
    }

    // Net no-op should stay bounded by gross no-op.
    {
        const auto gross_noop = gba::benchmark::measure(noop);
        const auto net_noop = gba::benchmark::measure_net(noop);
        gba::test.le(net_noop, gross_noop, "measure_net(noop) is bounded by gross noop");
    }

    return gba::test.finish();
}
