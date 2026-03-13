#include <gba/bios>
#include <gba/testing>

static constexpr uint32_t MAGIC = 0xDEADBEEF;

int main() {
    const auto marker = reinterpret_cast<volatile uint32_t*>(0x203FFFC);

    if (*marker == MAGIC) {
        return gba::test.finish(); // Detected soft reset
    }

    *marker = MAGIC;
    gba::SoftReset();
}
