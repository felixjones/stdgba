# Save Data

The GBA supports three save memory types. stdgba provides APIs for all three: SRAM, Flash, and EEPROM.

## SRAM (32KB)

SRAM is the simplest save type - byte-addressable static RAM at `0x0E000000`. Read and write directly through the `gba::mem_sram` registral:

```cpp
#include <gba/save>

// Write a byte
gba::mem_sram[0] = std::byte{0x42};

// Read it back
auto val = gba::mem_sram[0];
```

SRAM must be accessed one byte at a time (no 16/32-bit access). Data persists as long as the cartridge battery lasts.

## Flash (64KB / 128KB)

Flash memory uses sector-erased NOR storage. Unlike SRAM, Flash requires a command protocol - you cannot write directly. stdgba provides two chip-family APIs that compile command sequences at build time:

- `gba::flash::standard` - Macronix, Panasonic, Sanyo, SST chips
- `gba::flash::atmel` - Atmel chips (128-byte page writes, no separate erase)

### Standard Flash example

```cpp
#include <gba/save>

namespace sf = gba::flash::standard;

// Define callbacks for writing and reading sector data
void fill(sf::sector_span buf) {
    buf[0] = std::byte{0x42};
}

void recv(sf::const_sector_span buf) {
    // process loaded data...
}

// Compile a command sequence at build time
constexpr auto cmds = sf::compile(
    sf::erase_sector(0),
    sf::write_sector(0, fill),
    sf::read_sector(0, recv)
);

// Execute at runtime
auto err = cmds.execute();
```

### Flash detection

Before using Flash, detect the chip to populate the global state:

```cpp
auto info = gba::flash::detect();
// info.mfr      - manufacturer (macronix, panasonic, sanyo, sst, atmel)
// info.chip_size - flash_64k or flash_128k
```

### Flash specifics

- Writing is slow (milliseconds per byte)
- Flash has a limited number of erase cycles (~100,000)
- Flash and ROM share the same bus - interrupts that read ROM must be disabled during Flash operations

## EEPROM (512B / 8KB)

EEPROM is serial memory accessed via DMA3 in 8-byte blocks. Two APIs for the two sizes:

- `gba::eeprom::eeprom_512b` - 64 blocks, 6-bit addressing
- `gba::eeprom::eeprom_8k` - 1024 blocks, 14-bit addressing

Both provide raw block access and sequential stream types:

```cpp
#include <gba/save>

namespace ee = gba::eeprom::eeprom_512b;

// Stream-based write
ee::ostream out;
ee::block data = {std::byte{0xAA}};
out.write(&data, 1);

// Stream-based read
ee::istream in;
ee::block buf;
in.read(&buf, 1);
```
