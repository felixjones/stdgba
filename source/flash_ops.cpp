/**
 * @file flash_ops.cpp
 * @brief Layer 0 Flash hardware operation implementations.
 *
 * Each function copies a position-independent assembly routine from ROM
 * onto the stack and executes it via bx sp. Stack allocation sizes are
 * chosen to be the smallest ARM-immediate-encodable value that fits
 * each routine.
 */

#include <gba/bits/flash/operations.hpp>

// External assembly routine symbols (from flash.s)

extern "C" {
    extern const char flash_write_byte_routine[];
    extern const char flash_write_byte_routine_end[];

    extern const char flash_erase_sector_routine[];
    extern const char flash_erase_sector_routine_end[];

    extern const char flash_erase_chip_routine[];
    extern const char flash_erase_chip_routine_end[];

    extern const char flash_switch_bank_routine[];
    extern const char flash_switch_bank_routine_end[];

    extern const char flash_read_id_routine[];
    extern const char flash_read_id_routine_end[];

    extern const char flash_memcpy_routine[];
    extern const char flash_memcpy_routine_end[];

    extern const char flash_write_atmel_routine[];
    extern const char flash_write_atmel_routine_end[];
}

namespace gba::flash {

// Global Flash state

namespace detail {
    flash_state g_state{};
}

// Layer 0 naked wrapper functions
//
// Pattern for each:
//   1. Push arguments + callee-saved registers onto stack
//   2. Allocate stack space (sub sp)
//   3. Copy PIC routine from ROM to stack via word-copy loop
//   4. Reload arguments from their pushed locations
//   5. Call routine on stack (mov lr, pc; bx sp)
//   6. Return value stays in r0
//   7. Deallocate + pop + return
//
// Stack sizes (ARM immediates that fit the routine + alignment):
//   read_id:          256 bytes (routine ~144 bytes)
//   switch_bank:       64 bytes (routine ~56 bytes)
//   erase_sector:     256 bytes (routine ~148 bytes)
//   erase_chip:       256 bytes (routine ~132 bytes)
//   write_byte:       256 bytes (routine ~116 bytes)
//   write_atmel_page: 384 bytes (routine ~148 bytes + uses 6 callee-saved)
//   read_bytes:        64 bytes (routine ~28 bytes)

namespace detail {

[[gnu::naked, gnu::noinline, gnu::target("arm")]]
std::uint32_t read_id() noexcept {
    asm volatile(
        "push   {r4, r5, r6, r7, lr}\n\t"
        "sub    sp, sp, #256\n\t"
        "ldr    r4, =flash_read_id_routine\n\t"
        "ldr    r5, =flash_read_id_routine_end\n\t"
        "mov    r6, sp\n\t"
    "1:\n\t"
        "ldr    r7, [r4], #4\n\t"
        "str    r7, [r6], #4\n\t"
        "cmp    r4, r5\n\t"
        "blo    1b\n\t"
        "mov    lr, pc\n\t"
        "bx     sp\n\t"
        "add    sp, sp, #256\n\t"
        "pop    {r4, r5, r6, r7, lr}\n\t"
        "bx     lr\n\t"
        ::: "memory"
    );
}

[[gnu::naked, gnu::noinline, gnu::target("arm")]]
void switch_bank(int /* bank */) noexcept {
    asm volatile(
        "push   {r0, r4, r5, r6, r7, lr}\n\t"
        "sub    sp, sp, #64\n\t"
        "ldr    r4, =flash_switch_bank_routine\n\t"
        "ldr    r5, =flash_switch_bank_routine_end\n\t"
        "mov    r6, sp\n\t"
    "1:\n\t"
        "ldr    r7, [r4], #4\n\t"
        "str    r7, [r6], #4\n\t"
        "cmp    r4, r5\n\t"
        "blo    1b\n\t"
        "ldr    r0, [sp, #64]\n\t"
        "mov    lr, pc\n\t"
        "bx     sp\n\t"
        "add    sp, sp, #64\n\t"
        "pop    {r2, r4, r5, r6, r7, lr}\n\t"
        "bx     lr\n\t"
        ::: "memory"
    );
}

[[gnu::naked, gnu::noinline, gnu::target("arm")]]
int erase_sector(std::uint32_t /* addr */) noexcept {
    asm volatile(
        "push   {r0, r4, r5, r6, r7, lr}\n\t"
        "sub    sp, sp, #256\n\t"
        "ldr    r4, =flash_erase_sector_routine\n\t"
        "ldr    r5, =flash_erase_sector_routine_end\n\t"
        "mov    r6, sp\n\t"
    "1:\n\t"
        "ldr    r7, [r4], #4\n\t"
        "str    r7, [r6], #4\n\t"
        "cmp    r4, r5\n\t"
        "blo    1b\n\t"
        "ldr    r0, [sp, #256]\n\t"
        "mov    lr, pc\n\t"
        "bx     sp\n\t"
        "add    sp, sp, #256\n\t"
        "pop    {r2, r4, r5, r6, r7, lr}\n\t"
        "bx     lr\n\t"
        ::: "memory"
    );
}

[[gnu::naked, gnu::noinline, gnu::target("arm")]]
int erase_chip() noexcept {
    asm volatile(
        "push   {r4, r5, r6, r7, lr}\n\t"
        "sub    sp, sp, #256\n\t"
        "ldr    r4, =flash_erase_chip_routine\n\t"
        "ldr    r5, =flash_erase_chip_routine_end\n\t"
        "mov    r6, sp\n\t"
    "1:\n\t"
        "ldr    r7, [r4], #4\n\t"
        "str    r7, [r6], #4\n\t"
        "cmp    r4, r5\n\t"
        "blo    1b\n\t"
        "mov    lr, pc\n\t"
        "bx     sp\n\t"
        "add    sp, sp, #256\n\t"
        "pop    {r4, r5, r6, r7, lr}\n\t"
        "bx     lr\n\t"
        ::: "memory"
    );
}

[[gnu::naked, gnu::noinline, gnu::target("arm")]]
int write_byte(std::uint32_t /* addr */, std::uint8_t /* data */) noexcept {
    asm volatile(
        "push   {r0, r1, r4, r5, r6, r7, lr}\n\t"
        "sub    sp, sp, #256\n\t"
        "ldr    r4, =flash_write_byte_routine\n\t"
        "ldr    r5, =flash_write_byte_routine_end\n\t"
        "mov    r6, sp\n\t"
    "1:\n\t"
        "ldr    r7, [r4], #4\n\t"
        "str    r7, [r6], #4\n\t"
        "cmp    r4, r5\n\t"
        "blo    1b\n\t"
        "ldr    r0, [sp, #256]\n\t"
        "ldr    r1, [sp, #260]\n\t"
        "mov    lr, pc\n\t"
        "bx     sp\n\t"
        "add    sp, sp, #256\n\t"
        "pop    {r2, r3, r4, r5, r6, r7, lr}\n\t"
        "bx     lr\n\t"
        ::: "memory"
    );
}

[[gnu::naked, gnu::noinline, gnu::target("arm")]]
int write_atmel_page(std::uint32_t /* addr */, const std::uint8_t* /* data */) noexcept {
    asm volatile(
        "push   {r0, r1, r4, r5, r6, r7, lr}\n\t"
        "sub    sp, sp, #384\n\t"
        "ldr    r4, =flash_write_atmel_routine\n\t"
        "ldr    r5, =flash_write_atmel_routine_end\n\t"
        "mov    r6, sp\n\t"
    "1:\n\t"
        "ldr    r7, [r4], #4\n\t"
        "str    r7, [r6], #4\n\t"
        "cmp    r4, r5\n\t"
        "blo    1b\n\t"
        "ldr    r0, [sp, #384]\n\t"
        "ldr    r1, [sp, #388]\n\t"
        "mov    lr, pc\n\t"
        "bx     sp\n\t"
        "add    sp, sp, #384\n\t"
        "pop    {r2, r3, r4, r5, r6, r7, lr}\n\t"
        "bx     lr\n\t"
        ::: "memory"
    );
}

[[gnu::naked, gnu::noinline, gnu::target("arm")]]
void read_bytes(void* /* dst */, const volatile std::uint8_t* /* src */, std::size_t /* n */) noexcept {
    asm volatile(
        "push   {r0, r1, r2, r4, r5, r6, r7, lr}\n\t"
        "sub    sp, sp, #64\n\t"
        "ldr    r4, =flash_memcpy_routine\n\t"
        "ldr    r5, =flash_memcpy_routine_end\n\t"
        "mov    r6, sp\n\t"
    "1:\n\t"
        "ldr    r7, [r4], #4\n\t"
        "str    r7, [r6], #4\n\t"
        "cmp    r4, r5\n\t"
        "blo    1b\n\t"
        "ldr    r0, [sp, #64]\n\t"
        "ldr    r1, [sp, #68]\n\t"
        "ldr    r2, [sp, #72]\n\t"
        "mov    lr, pc\n\t"
        "bx     sp\n\t"
        "add    sp, sp, #64\n\t"
        "pop    {r0, r1, r2, r4, r5, r6, r7, lr}\n\t"
        "bx     lr\n\t"
        ::: "memory"
    );
}

} // namespace detail

// detect() — populate global Flash state

namespace {

inline constexpr std::uint8_t dev_mx29l010 = 0x09;
inline constexpr std::uint8_t dev_le26fv10n1ts = 0x13;
inline constexpr std::uint8_t dev_mn63f805mnp = 0x1B;
inline constexpr std::uint8_t dev_mx29l512 = 0x1C;
inline constexpr std::uint8_t dev_at29lv512 = 0x3D;
inline constexpr std::uint8_t dev_le39fw512 = 0xD4;

struct chip_entry {
    std::uint8_t device;
    manufacturer mfr;
    size sz;
};

inline constexpr chip_entry known_chips[] = {
    {dev_mx29l512,      manufacturer::macronix,  size::flash_64k},
    {dev_mn63f805mnp,   manufacturer::panasonic,  size::flash_64k},
    {dev_le39fw512,     manufacturer::sst,        size::flash_64k},
    {dev_at29lv512,     manufacturer::atmel,      size::flash_64k},
    {dev_mx29l010,      manufacturer::macronix,  size::flash_128k},
    {dev_le26fv10n1ts,  manufacturer::sanyo,     size::flash_128k},
};

} // anonymous namespace

chip_info detect(size size_hint) noexcept {
    // Set 8-cycle waitstate for Flash detection
    *reinterpret_cast<volatile std::uint16_t*>(0x04000204) |= 3;

    const std::uint32_t id = detail::read_id();
    const auto mfr_id = static_cast<std::uint8_t>(id & 0xFF);
    const auto device_id = static_cast<std::uint8_t>((id >> 8) & 0xFF);

    // Sanyo 128K needs double exit from ID mode
    if (mfr_id == static_cast<std::uint8_t>(manufacturer::sanyo)) {
        *detail::flash_cmd_ptr(0x5555) = 0xF0;
    }

    auto& state = detail::g_state;
    state.info.device = device_id;
    state.info.mfr = static_cast<manufacturer>(mfr_id);
    state.info.chip_size = size::detect;

    for (const auto& chip : known_chips) {
        if (device_id == chip.device && state.info.mfr == chip.mfr) {
            state.info.chip_size = chip.sz;
            break;
        }
    }

    if (size_hint != size::detect) {
        state.info.chip_size = size_hint;
    }

    state.current_bank = 0;

    return state.info;
}

} // namespace gba::flash
