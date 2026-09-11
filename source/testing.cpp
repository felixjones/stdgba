#include <gba/testing>

#include <array>
#include <cstddef>
#include <source_location>

// newlib-provided assertion entry point; the reserved spelling is fixed by the C library ABI.
// NOLINTNEXTLINE(bugprone-reserved-identifier,readability-identifier-naming)
extern "C" [[noreturn]] void __assert_func(const char* file, int line, const char* func, const char* expr);

namespace gba::testing {
    namespace {
        using vu16 = volatile unsigned short;

        inline vu16& mgba_enable_reg() {
            return *reinterpret_cast<vu16*>(0x4FFF780);
        }

        inline vu16& mgba_flags_reg() {
            return *reinterpret_cast<vu16*>(0x4FFF700);
        }

        inline char* mgba_buffer_reg() {
            return reinterpret_cast<char*>(0x4FFF600);
        }

        bool mgba_probe() {
            mgba_enable_reg() = 0xC0DE;
            return mgba_enable_reg() == 0x1DEA;
        }

        void mgba_write(int level, const char* msg) {
            char* dst = mgba_buffer_reg();
            std::size_t i = 0;
            while ((msg[i] != 0) && i < 255) {
                // dst is the mGBA debug buffer, a deliberate fixed-address MMIO window.
                dst[i] = msg[i]; // NOLINT(clang-analyzer-optin.core.FixedAddressDereference)
                ++i;
            }
            dst[i] = '\0'; // NOLINT(clang-analyzer-optin.core.FixedAddressDereference)
            mgba_flags_reg() = static_cast<unsigned short>((static_cast<unsigned>(level) & 0x7u) | 0x100u);
        }

        void signal_exit_swi(int code) noexcept {
            register auto r0 asm("r0") = code; // NOLINT(misc-const-correctness)
                                               // mgba-headless traps this SWI via -S and reads r0 as the exit code.
                                               // Default 0x1A (SoundDriverInit) is a harmless no-op on real hardware.
                                               // Override at compile time with -DSTDGBA_EXIT_SWI=0x##.
#define STDGBA_EXIT_SWI_STR2(x) #x
// The SWI number must be a literal immediate in the asm template, so stringification via macro is unavoidable.
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define STDGBA_EXIT_SWI_STR(x) STDGBA_EXIT_SWI_STR2(x)
            asm volatile("swi " STDGBA_EXIT_SWI_STR(STDGBA_EXIT_SWI) " << ((1f - . == 4) * -16); 1:"
                         : "+r"(r0)
                         :
                         : "r1", "r2", "r3", "memory");
#undef STDGBA_EXIT_SWI_STR
#undef STDGBA_EXIT_SWI_STR2
        }

        void copy_cstr(char*& dst, const char* end, const char* src) {
            while (dst < end && ((*src) != 0)) {
                *dst++ = *src++;
            }
        }

        void append_u32(char*& dst, const char* end, unsigned int value) {
            std::array<char, 10> tmp{};
            const char* first = tmp.data();
            const char* last = tmp.data() + tmp.size();
            char* cursor = tmp.data() + tmp.size();
            if (value == 0) {
                *--cursor = '0';
            } else {
                while (value != 0 && cursor != first) {
                    *--cursor = static_cast<char>('0' + (value % 10));
                    value /= 10;
                }
            }
            while (cursor != last && dst < end) {
                *dst++ = *cursor++;
            }
        }

        void log_joined_info(const char* a, const char* b) {
            std::array<char, 192> buf{};
            char* out = buf.data();
            const char* end = buf.data() + buf.size() - 1;
            copy_cstr(out, end, a);
            copy_cstr(out, end, b);
            *out = '\0';
            mgba_write(3, buf.data()); // info
        }

        void log_joined_error(const char* a, const char* b) {
            std::array<char, 192> buf{};
            char* out = buf.data();
            const char* end = buf.data() + buf.size() - 1;
            copy_cstr(out, end, a);
            copy_cstr(out, end, b);
            *out = '\0';
            mgba_write(1, buf.data()); // error
        }
    } // namespace

    api::api(config cfg) noexcept : expect(*this, severity::expect), m_cfg(cfg) {}

    api::~api() noexcept {
        if (!m_cfg.auto_finish || m_finished || !m_used) return;
        (void)finish();
    }

    bool api::running_in_mgba() noexcept {
        if (!m_backendChecked) {
            m_isMgba = mgba_probe();
            m_backendChecked = true;
        }
        return m_isMgba;
    }

    void api::log_line(const char* text) noexcept {
        if (running_in_mgba()) {
            mgba_write(3, text);
        }
    }

    void api::begin_case(const char* name, std::source_location /*loc*/) noexcept {
        m_used = true;
        m_caseName = name;
        if (running_in_mgba()) {
            log_joined_info("[CASE] begin: ", name);
        }
    }

    void api::end_case() noexcept {
        if (running_in_mgba() && m_caseName) {
            log_joined_info("[CASE] end: ", m_caseName);
        }
        m_caseName = nullptr;
    }

    void api::report_failure(const char* message, std::source_location loc) noexcept {
        ++m_failures;
        if (!m_firstFile) {
            m_firstFile = loc.file_name();
            m_firstFunction = loc.function_name();
            m_firstLine = loc.line();
            m_firstMessage = message;
        }

        if (running_in_mgba()) {
            if (message && (message[0] != 0)) {
                if (m_caseName) {
                    log_joined_error("[FAIL] ", m_caseName);
                    log_joined_error("[FAIL] detail: ", message);
                } else {
                    log_joined_error("[FAIL] ", message);
                }
            } else if (m_caseName) {
                log_joined_error("[FAIL] ", m_caseName);
            }

            std::array<char, 48> lineBuf{};
            char* out = lineBuf.data();
            const char* end = lineBuf.data() + lineBuf.size() - 1;
            copy_cstr(out, end, "[FAIL] line ");
            append_u32(out, end, static_cast<unsigned int>(loc.line()));
            *out = '\0';
            mgba_write(1, lineBuf.data());
        }
    }

    [[noreturn]] void api::abort_now(const char* message, std::source_location loc) noexcept {
        report_failure(message, loc);
        if (running_in_mgba()) {
            signal_exit_swi(static_cast<int>(m_failures));
        }
        __assert_func(loc.file_name(), static_cast<int>(loc.line()), loc.function_name(),
                      (message && (message[0] != 0)) ? message : "gba::test assertion failed");
    }

    bool api::is_true_impl(bool condition, const char* message, severity sev, std::source_location loc) noexcept {
        m_used = true;
        if (condition) {
            ++m_passes;
            if (m_cfg.log_passes && running_in_mgba()) {
                log_line("[PASS]");
                if (message && (message[0] != 0)) {
                    log_line(message);
                }
            }
            return true;
        }

        if (sev == severity::assert_) {
            abort_now(message, loc);
        }

        report_failure(message, loc);
        return false;
    }

    int api::finish() noexcept {
        m_used = true;
        if (m_finished) return static_cast<int>(m_failures);
        m_finished = true;

        if (running_in_mgba()) {
            if (m_failures == 0) {
                std::array<char, 64> buf{};
                char* out = buf.data();
                const char* end = buf.data() + buf.size() - 1;
                copy_cstr(out, end, "RESULT: OK (");
                append_u32(out, end, m_passes);
                copy_cstr(out, end, " pass)");
                *out = '\0';
                mgba_write(3, buf.data());
            } else {
                std::array<char, 80> buf{};
                char* out = buf.data();
                const char* end = buf.data() + buf.size() - 1;
                copy_cstr(out, end, "RESULT: FAIL (");
                append_u32(out, end, m_failures);
                copy_cstr(out, end, " fail, ");
                append_u32(out, end, m_passes);
                copy_cstr(out, end, " pass)");
                *out = '\0';
                mgba_write(1, buf.data());
            }
            signal_exit_swi(static_cast<int>(m_failures));
            if (m_failures != 0) {
                __assert_func(m_firstFile ? m_firstFile : "<unknown>", static_cast<int>(m_firstLine),
                              m_firstFunction ? m_firstFunction : "<unknown>",
                              m_firstMessage ? m_firstMessage : "gba::test failure");
            }
        }

        if (m_failures != 0) {
            __assert_func(m_firstFile ? m_firstFile : "<unknown>", static_cast<int>(m_firstLine),
                          m_firstFunction ? m_firstFunction : "<unknown>",
                          m_firstMessage ? m_firstMessage : "gba::test failure");
        }

        return static_cast<int>(m_failures);
    }

    void api::reset() noexcept {
        m_used = false;
        m_finished = false;
        m_passes = 0;
        m_failures = 0;
        m_caseName = nullptr;
        m_firstFile = nullptr;
        m_firstFunction = nullptr;
        m_firstMessage = nullptr;
        m_firstLine = 0;
    }

    summary api::stats() const noexcept {
        return {.passes = m_passes, .failures = m_failures};
    }

    bool api::ok() const noexcept {
        return m_failures == 0;
    }

} // namespace gba::testing
