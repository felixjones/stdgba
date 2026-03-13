/// @file tests/debug/test_logger.cpp
/// @brief Tests for gba::log module: detection, level filtering, backends,
/// formatted output, and custom backend dispatch.

#include <gba/format>
#include <gba/logger>
#include <gba/testing>

#include <cstddef>

using namespace gba::literals;

// Custom backend that records what it received
struct spy_backend : gba::log::backend {
    gba::log::level last_level{};
    const char* last_msg{};
    std::size_t last_len{};
    int call_count{};

    std::size_t write(gba::log::level lvl, const char* msg, std::size_t len) override {
        last_level = lvl;
        last_msg = msg;
        last_len = len;
        ++call_count;
        return len;
    }
};

int main() {
    // Test: init() detects mGBA (we are running in mgba-headless)
    {
        bool available = gba::log::init();
        gba::test.is_true(available);
    }

    // Test: After init, backend is non-null
    { gba::test.is_true(gba::log::get_backend() != nullptr); }

    // Test: Default min level is debug (most permissive)
    {
        gba::log::set_level(gba::log::level::debug);
        gba::test.eq(static_cast<int>(gba::log::get_level()), static_cast<int>(gba::log::level::debug));
    }

    // Test: All 5 convenience functions return nonzero on mGBA
    {
        gba::log::set_level(gba::log::level::debug);

        auto r_fatal = gba::log::fatal("test fatal");
        gba::test.nz(r_fatal);
        auto r_error = gba::log::error("test error");
        gba::test.nz(r_error);
        auto r_warn = gba::log::warn("test warn");
        gba::test.nz(r_warn);
        auto r_info = gba::log::info("test info");
        gba::test.nz(r_info);
        auto r_debug = gba::log::debug("test debug");
        gba::test.nz(r_debug);
    }

    // Test: write() with runtime level works
    {
        gba::log::set_level(gba::log::level::debug);
        auto r = gba::log::write(gba::log::level::info, "runtime level msg");
        gba::test.nz(r);
    }

    // Test: Level filtering - messages above min_level are suppressed
    {
        gba::log::set_level(gba::log::level::warn);

        // debug (4) > warn (2), so should be filtered
        auto r_debug = gba::log::debug("should be filtered");
        gba::test.eq(r_debug, 0u);

        // info (3) > warn (2), so should be filtered
        auto r_info = gba::log::info("should be filtered");
        gba::test.eq(r_info, 0u);

        // warn (2) == warn (2), so should pass
        auto r_warn = gba::log::warn("should pass");
        gba::test.nz(r_warn);

        // error (1) < warn (2), so should pass
        auto r_error = gba::log::error("should pass");
        gba::test.nz(r_error);

        // fatal (0) < warn (2), so should pass
        auto r_fatal = gba::log::fatal("should pass");
        gba::test.nz(r_fatal);

        // Restore
        gba::log::set_level(gba::log::level::debug);
    }

    // Test: set_level / get_level roundtrip for all levels
    {
        gba::log::set_level(gba::log::level::fatal);
        gba::test.eq(static_cast<int>(gba::log::get_level()), static_cast<int>(gba::log::level::fatal));

        gba::log::set_level(gba::log::level::error);
        gba::test.eq(static_cast<int>(gba::log::get_level()), static_cast<int>(gba::log::level::error));

        gba::log::set_level(gba::log::level::warn);
        gba::test.eq(static_cast<int>(gba::log::get_level()), static_cast<int>(gba::log::level::warn));

        gba::log::set_level(gba::log::level::info);
        gba::test.eq(static_cast<int>(gba::log::get_level()), static_cast<int>(gba::log::level::info));

        gba::log::set_level(gba::log::level::debug);
        gba::test.eq(static_cast<int>(gba::log::get_level()), static_cast<int>(gba::log::level::debug));
    }

    // Test: Formatted output with _fmt returns nonzero
    {
        gba::log::set_level(gba::log::level::debug);
        auto r = gba::log::info("HP: {hp}/{max}"_fmt, "hp"_arg = 42, "max"_arg = 100);
        gba::test.nz(r);
    }

    // Test: Formatted write() with runtime level
    {
        auto r = gba::log::write(gba::log::level::warn, "Val: {x}"_fmt, "x"_arg = 999);
        gba::test.nz(r);
    }

    // Test: Formatted output is filtered by level
    {
        gba::log::set_level(gba::log::level::error);
        auto r = gba::log::info("HP: {hp}"_fmt, "hp"_arg = 42);
        gba::test.eq(r, 0u);
        gba::log::set_level(gba::log::level::debug);
    }

    // Test: set_backend / get_backend roundtrip
    {
        auto* original = gba::log::get_backend();

        spy_backend spy;
        gba::log::set_backend(&spy);
        gba::test.is_true(gba::log::get_backend() == &spy);

        // Restore
        gba::log::set_backend(original);
        gba::test.is_true(gba::log::get_backend() == original);
    }

    // Test: Custom backend receives correct level and message length
    {
        auto* original = gba::log::get_backend();

        spy_backend spy;
        gba::log::set_backend(&spy);
        gba::log::set_level(gba::log::level::debug);

        gba::log::info("hello");
        gba::test.eq(spy.call_count, 1);
        gba::test.eq(static_cast<int>(spy.last_level), static_cast<int>(gba::log::level::info));
        gba::test.eq(spy.last_len, 5u);

        gba::log::error("ab");
        gba::test.eq(spy.call_count, 2);
        gba::test.eq(static_cast<int>(spy.last_level), static_cast<int>(gba::log::level::error));
        gba::test.eq(spy.last_len, 2u);

        gba::log::set_backend(original);
    }

    // Test: Custom backend is not called when level is filtered
    {
        auto* original = gba::log::get_backend();

        spy_backend spy;
        gba::log::set_backend(&spy);
        gba::log::set_level(gba::log::level::error);

        gba::log::debug("should not reach spy");
        gba::test.eq(spy.call_count, 0);

        gba::log::info("should not reach spy");
        gba::test.eq(spy.call_count, 0);

        gba::log::warn("should not reach spy");
        gba::test.eq(spy.call_count, 0);

        gba::log::error("should reach spy");
        gba::test.eq(spy.call_count, 1);

        gba::log::fatal("should reach spy");
        gba::test.eq(spy.call_count, 2);

        gba::log::set_backend(original);
        gba::log::set_level(gba::log::level::debug);
    }

    // Test: Custom backend receives formatted output
    {
        auto* original = gba::log::get_backend();

        spy_backend spy;
        gba::log::set_backend(&spy);
        gba::log::set_level(gba::log::level::debug);

        gba::log::info("X: {v}"_fmt, "v"_arg = 123);
        gba::test.eq(spy.call_count, 1);
        gba::test.nz(spy.last_len);

        gba::log::set_backend(original);
    }

    // Test: null_backend returns len without crashing
    {
        auto* original = gba::log::get_backend();

        gba::log::null_backend null_be;
        gba::log::set_backend(&null_be);
        gba::log::set_level(gba::log::level::debug);

        auto r = gba::log::info("test null");
        gba::test.nz(r);

        r = gba::log::info("X: {v}"_fmt, "v"_arg = 42);
        gba::test.nz(r);

        gba::log::set_backend(original);
    }

    // Test: write() with explicit length
    {
        auto* original = gba::log::get_backend();

        spy_backend spy;
        gba::log::set_backend(&spy);
        gba::log::set_level(gba::log::level::debug);

        // Pass only first 3 bytes of "hello"
        auto r = gba::log::write(gba::log::level::info, "hello", 3);
        gba::test.eq(spy.last_len, 3u);
        gba::test.eq(r, 3u);

        gba::log::set_backend(original);
    }

    // Test: write() returns 0 when no backend is set
    {
        auto* original = gba::log::get_backend();

        gba::log::set_backend(nullptr);
        auto r = gba::log::info("no backend");
        gba::test.eq(r, 0u);

        r = gba::log::info("X: {v}"_fmt, "v"_arg = 1);
        gba::test.eq(r, 0u);

        gba::log::set_backend(original);
    }

    // Test: fatal level filters everything except fatal
    {
        gba::log::set_level(gba::log::level::fatal);

        auto r = gba::log::debug("no");
        gba::test.eq(r, 0u);
        r = gba::log::info("no");
        gba::test.eq(r, 0u);
        r = gba::log::warn("no");
        gba::test.eq(r, 0u);
        r = gba::log::error("no");
        gba::test.eq(r, 0u);
        r = gba::log::fatal("yes");
        gba::test.nz(r);

        gba::log::set_level(gba::log::level::debug);
    }
    return gba::test.finish();
}
