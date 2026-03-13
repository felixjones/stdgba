#include <gba/bios>
#include <gba/testing>

#include <cstddef>

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
            while (msg[i] && i < 255) {
                dst[i] = msg[i];
                ++i;
            }
            dst[i] = '\0';
            mgba_flags_reg() = static_cast<unsigned short>(level & 0x7) | 0x100;
        }

        void signal_exit_swi(int code) noexcept {
            register auto r0 asm("r0") = code;
            // mgba-headless traps this SWI via -S and reads r0 as the exit code.
            // Default 0x1A (SoundDriverInit) is a harmless no-op on real hardware.
            // Override at compile time with -DSTDGBA_EXIT_SWI=0x##.
#define STDGBA_EXIT_SWI_STR2(x) #x
#define STDGBA_EXIT_SWI_STR(x)  STDGBA_EXIT_SWI_STR2(x)
            asm volatile("swi " STDGBA_EXIT_SWI_STR(STDGBA_EXIT_SWI) " << ((1f - . == 4) * -16); 1:"
                         : "+r"(r0)
                         :
                         : "r1", "r2", "r3", "memory");
#undef STDGBA_EXIT_SWI_STR
#undef STDGBA_EXIT_SWI_STR2
        }

        void copy_cstr(char*& dst, const char* end, const char* src) {
            while (dst < end && *src) {
                *dst++ = *src++;
            }
        }

        void append_u32(char*& dst, const char* end, unsigned int value) {
            char tmp[10];
            int n = 0;
            do {
                tmp[n++] = static_cast<char>('0' + (value % 10));
                value /= 10;
            } while (value != 0 && n < 10);
            while (n > 0 && dst < end) {
                *dst++ = tmp[--n];
            }
        }

        void log_joined_info(const char* a, const char* b) {
            char buf[192];
            char* out = buf;
            const char* end = buf + sizeof(buf) - 1;
            copy_cstr(out, end, a);
            copy_cstr(out, end, b);
            *out = '\0';
            mgba_write(3, buf); // info
        }

        void log_joined_error(const char* a, const char* b) {
            char buf[192];
            char* out = buf;
            const char* end = buf + sizeof(buf) - 1;
            copy_cstr(out, end, a);
            copy_cstr(out, end, b);
            *out = '\0';
            mgba_write(1, buf); // error
        }
    } // namespace

    api::api(config cfg) noexcept : expect(*this, severity::expect), m_cfg(cfg) {}

    api::~api() noexcept {
        if (!m_cfg.auto_finish || m_finished || !m_used) return;
        (void)finish();
    }

    bool api::running_in_mgba() noexcept {
        if (!m_backend_checked) {
            m_is_mgba = mgba_probe();
            m_backend_checked = true;
        }
        return m_is_mgba;
    }

    void api::log_line(const char* text) noexcept {
        if (running_in_mgba()) {
            mgba_write(3, text);
        }
    }

    void api::begin_case(const char* name, std::source_location) noexcept {
        m_used = true;
        m_case_name = name;
        if (running_in_mgba()) {
            log_joined_info("[CASE] begin: ", name);
        }
    }

    void api::end_case() noexcept {
        if (running_in_mgba() && m_case_name) {
            log_joined_info("[CASE] end: ", m_case_name);
        }
        m_case_name = nullptr;
    }

    void api::report_failure(const char* message, std::source_location loc) noexcept {
        ++m_failures;
        if (!m_first_file) {
            m_first_file = loc.file_name();
            m_first_function = loc.function_name();
            m_first_line = loc.line();
            m_first_message = message;
        }

        if (running_in_mgba()) {
            if (message && message[0]) {
                if (m_case_name) {
                    log_joined_error("[FAIL] ", m_case_name);
                    log_joined_error("[FAIL] detail: ", message);
                } else {
                    log_joined_error("[FAIL] ", message);
                }
            } else if (m_case_name) {
                log_joined_error("[FAIL] ", m_case_name);
            }

            char line_buf[48];
            char* out = line_buf;
            const char* end = line_buf + sizeof(line_buf) - 1;
            copy_cstr(out, end, "[FAIL] line ");
            append_u32(out, end, static_cast<unsigned int>(loc.line()));
            *out = '\0';
            mgba_write(1, line_buf);
        }
    }

    [[noreturn]] void api::abort_now(const char* message, std::source_location loc) noexcept {
        report_failure(message, loc);
        if (running_in_mgba()) {
            signal_exit_swi(static_cast<int>(m_failures));
        }
        __assert_func(loc.file_name(), static_cast<int>(loc.line()), loc.function_name(),
                      (message && message[0]) ? message : "gba::test assertion failed");
    }

    bool api::is_true_impl(bool condition, const char* message, severity sev, std::source_location loc) noexcept {
        m_used = true;
        if (condition) {
            ++m_passes;
            if (m_cfg.log_passes && running_in_mgba()) {
                log_line("[PASS]");
                if (message && message[0]) {
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
                char buf[64];
                char* out = buf;
                const char* end = buf + sizeof(buf) - 1;
                copy_cstr(out, end, "RESULT: OK (");
                append_u32(out, end, m_passes);
                copy_cstr(out, end, " pass)");
                *out = '\0';
                mgba_write(3, buf);
            } else {
                char buf[80];
                char* out = buf;
                const char* end = buf + sizeof(buf) - 1;
                copy_cstr(out, end, "RESULT: FAIL (");
                append_u32(out, end, m_failures);
                copy_cstr(out, end, " fail, ");
                append_u32(out, end, m_passes);
                copy_cstr(out, end, " pass)");
                *out = '\0';
                mgba_write(1, buf);
            }
            signal_exit_swi(static_cast<int>(m_failures));
            if (m_failures != 0) {
                __assert_func(m_first_file ? m_first_file : "<unknown>", static_cast<int>(m_first_line),
                              m_first_function ? m_first_function : "<unknown>",
                              m_first_message ? m_first_message : "gba::test failure");
            }
        }

        if (m_failures != 0) {
            __assert_func(m_first_file ? m_first_file : "<unknown>", static_cast<int>(m_first_line),
                          m_first_function ? m_first_function : "<unknown>",
                          m_first_message ? m_first_message : "gba::test failure");
        }

        return static_cast<int>(m_failures);
    }

    void api::reset() noexcept {
        m_used = false;
        m_finished = false;
        m_passes = 0;
        m_failures = 0;
        m_case_name = nullptr;
        m_first_file = nullptr;
        m_first_function = nullptr;
        m_first_message = nullptr;
        m_first_line = 0;
    }

    summary api::stats() const noexcept {
        return {m_passes, m_failures};
    }

    bool api::ok() const noexcept {
        return m_failures == 0;
    }

} // namespace gba::testing
