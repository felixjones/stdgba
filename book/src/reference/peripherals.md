# Peripheral Register Reference

This is a complete reference of every memory-mapped I/O register exposed by stdgba. Registers are grouped by subsystem and listed by hardware address.

All registers are declared in `<gba/peripherals>` unless noted otherwise. DMA registers are in `<gba/dma>`, palette memory symbols are in `<gba/color>`, and VRAM/OAM symbols are in `<gba/video>`.

## How to read this reference

Each entry shows:

- **stdgba name** - the `inline constexpr` variable you use in code
- **Address** - the memory-mapped hardware address
- **Access** - R (read), W (write), or RW (read-write)
- **Type** - the bitfield struct or integer type
- **tonclib name** - the equivalent `#define` from tonclib/libtonc

Array registers are written as `name[N]` with their element stride.
