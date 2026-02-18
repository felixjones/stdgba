/**
 * @file bits/flash/operations.hpp
 * @brief Layer 0: Flash types and low-level hardware operations.
 *
 * Defines the core Flash types (error codes, chip sizes, manufacturer IDs)
 * and provides direct access to Flash chip primitives. Each hardware
 * function copies a position-independent routine from ROM to the stack
 * (IWRAM) and executes it via bx, because Flash and ROM share the same
 * memory bus on GBA.
 *
 * These are the building blocks for:
 * - Layer 1: Chip-specific compiled command sequences
 * - Layer 2: Unified compiled save API
 *
 * @warning These functions must NOT be called from ROM-resident code
 *          that runs concurrently with Flash access. Interrupts that
 *          read ROM should be disabled during Flash operations.
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace gba::flash {

/**
 * @brief Flash operation error codes.
 */
enum class error : std::uint8_t {
    success = 0,        ///< Operation completed successfully
    invalid_param,      ///< Invalid input parameter (null pointer)
    out_of_range,       ///< Address exceeds Flash size
    verify_failed,      ///< Written data verification failed
    unsupported_device, ///< Flash chip not recognized
    timeout             ///< Operation timed out
};

/**
 * @brief Flash chip size.
 */
enum class size : std::uint8_t {
    detect = 0, ///< Auto-detect chip size
    flash_64k,  ///< 64KB (512Kbit)
    flash_128k  ///< 128KB (1Mbit)
};

/**
 * @brief Known Flash chip manufacturers.
 */
enum class manufacturer : std::uint8_t {
    unknown = 0,
    atmel = 0x1F,
    panasonic = 0x32,
    sanyo = 0x62,
    sst = 0xBF,
    macronix = 0xC2
};

/**
 * @brief Flash chip information.
 */
struct chip_info {
    std::uint8_t device;        ///< Device ID
    manufacturer mfr;           ///< Manufacturer
    size chip_size;             ///< Detected size
};

/// @brief Size of a standard Flash sector in bytes (4KB).
inline constexpr std::size_t sector_size = 4096;

/// @brief Size of an Atmel Flash page in bytes (128B).
inline constexpr std::size_t page_size_atmel = 128;

/// @brief Size of a Flash bank in bytes (64KB).
inline constexpr std::size_t bank_size = 0x10000;

/// @brief Number of 4KB sectors per bank.
inline constexpr int sectors_per_bank = 16;

/// @brief Number of 128B Atmel pages per bank.
inline constexpr int pages_per_bank = 512;

namespace detail {

/**
 * @brief Global Flash chip state.
 *
 * Set once by detect() during initialization. Referenced by all
 * layers without needing to pass device handles around.
 */
struct flash_state {
    chip_info info{};
    int current_bank = 0;
};

/// @brief The single global Flash state instance.
extern flash_state g_state;

// Layer 0 primitives — direct hardware access
//
// Each function copies a position-independent assembly routine onto the
// stack and executes it. These are the irreducible hardware operations.

/**
 * @brief Read the Flash chip manufacturer and device ID.
 * @return Low byte = manufacturer ID, high byte = device ID.
 *
 * Enters and exits chip ID mode. For Sanyo chips, the caller must
 * issue an additional exit command after this returns.
 */
[[gnu::noinline]]
std::uint32_t read_id() noexcept;

/**
 * @brief Switch the active Flash bank (128KB chips only).
 * @param bank Bank number (0 or 1).
 *
 * Sends the bank switch command sequence directly. Does NOT update
 * g_state.current_bank — callers are responsible for tracking state.
 */
[[gnu::noinline]]
void switch_bank(int bank) noexcept;

/**
 * @brief Erase a 4KB Flash sector (standard chips, not Atmel).
 * @param addr Sector address offset (will be aligned to 4KB boundary).
 * @return 0 on success, non-zero on timeout.
 */
[[gnu::noinline]]
int erase_sector(std::uint32_t addr) noexcept;

/**
 * @brief Erase the entire Flash chip.
 * @return 0 on success, non-zero on timeout.
 */
[[gnu::noinline]]
int erase_chip() noexcept;

/**
 * @brief Write a single byte to Flash (standard chips, not Atmel).
 * @param addr Address offset from Flash base (0x0E000000).
 * @param data Byte value to write.
 * @return 0 on success, non-zero on timeout.
 *
 * The target sector must already be erased.
 */
[[gnu::noinline]]
int write_byte(std::uint32_t addr, std::uint8_t data) noexcept;

/**
 * @brief Write a 128-byte page to Flash (Atmel chips only).
 * @param addr Page address offset (must be 128-byte aligned).
 * @param data Pointer to 128 bytes of source data.
 * @return 0 on success, non-zero on timeout.
 *
 * No separate erase step is needed — Atmel page writes implicitly
 * erase and reprogram the 128-byte region.
 */
[[gnu::noinline]]
int write_atmel_page(std::uint32_t addr, const std::uint8_t* data) noexcept;

/**
 * @brief Byte-by-byte copy from Flash memory.
 * @param dst Destination buffer.
 * @param src Source address in Flash (volatile pointer).
 * @param n Number of bytes to copy.
 *
 * Flash must be read byte-by-byte because it sits on a shared bus.
 */
[[gnu::noinline]]
void read_bytes(void* dst, const volatile std::uint8_t* src, std::size_t n) noexcept;

/**
 * @brief Compute a volatile pointer to the Flash memory at a given offset.
 * @param offset Byte offset within the current bank (0 to bank_size-1).
 * @return Volatile pointer to the Flash memory location.
 */
inline const volatile std::uint8_t* flash_ptr(std::uint32_t offset) noexcept {
    return reinterpret_cast<const volatile std::uint8_t*>(0x0E000000 + offset);
}

/**
 * @brief Compute a writable volatile pointer to Flash command addresses.
 * @param offset Byte offset.
 * @return Writable volatile pointer to the Flash memory location.
 */
inline volatile std::uint8_t* flash_cmd_ptr(std::uint32_t offset) noexcept {
    return reinterpret_cast<volatile std::uint8_t*>(0x0E000000 + offset);
}

} // namespace detail

/**
 * @brief Detect the Flash chip and populate global state.
 *
 * Reads the chip ID, matches against known devices, and stores the
 * result in detail::g_state. Call once during game initialization
 * before any other Flash operations.
 *
 * @param size_hint Override auto-detection with a specific chip size.
 *                  Use size::detect for auto-detection.
 * @return The detected chip info, or size::detect if detection failed.
 */
chip_info detect(size size_hint = size::detect) noexcept;

} // namespace gba::flash
