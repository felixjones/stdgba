#include <gba/bios>

#include <mgba_test.hpp>

static constexpr uint32_t MAGIC = 0xDEADBEEF;

int main() {
    const auto marker = reinterpret_cast<volatile uint32_t*>(0x203FFFC);

    if (*marker == MAGIC) {
        test::exit(0); // Detected soft reset
    }

    *marker = MAGIC;
    gba::SoftReset();
}
