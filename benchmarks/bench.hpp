/// @file benchmarks/bench.hpp
/// @brief Backward-compatible aliases for gba/benchmark API.
///
/// All functionality now lives in `gba/benchmark`. This header provides
/// the `bench::` namespace aliases used by existing benchmark files.
/// New benchmarks should use `gba::benchmark` directly.
#pragma once

#include <gba/benchmark>
#include <gba/testing>

namespace bench {

    using gba::benchmark::exit;
    using gba::benchmark::measure;
    using gba::benchmark::measure_avg;

    using gba::benchmark::report::header;
    using gba::benchmark::report::log_printf;
    using gba::benchmark::report::row;

    inline void print_header(const char* title) {
        header(title);
    }
    inline void print_row(const char* desc, unsigned int sg, unsigned int ab) {
        row(desc, sg, ab);
    }

    template<typename Fn>
    inline void with_logger(Fn&& fn) {
        if (!gba::log::get_backend()) gba::log::init();
        std::forward<Fn>(fn)();
    }

} // namespace bench
