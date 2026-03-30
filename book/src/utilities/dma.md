# DMA Transfers

`<gba/dma>` gives you two layers of control:

- raw register access (`reg_dmasad`, `reg_dmadad`, `reg_dmacnt_l`, `reg_dmacnt_h`, `reg_dma`)
- helper constructors on `gba::dma` for common transfer patterns

Use the helper layer for most gameplay code, then drop to raw registers when you need an exact hardware setup.

For full register/type tables, see [DMA Peripheral Reference](../reference/peripherals/dma.md).

## Why DMA matters on GBA

DMA moves data without per-element CPU loops. Typical wins:

- bulk tile/map/palette uploads
- repeated clears/fills
- VBlank/HBlank timed updates
- DirectSound FIFO streaming

The ARM7TDMI is fast enough for logic, but memory traffic can eat frame budget quickly. DMA is the default path for larger copies.

> **Note:** stdgba provides a hand-tuned implementation of `std::memset`/`memclr` (via the `__aeabi_memset*` entry points).
>
> For large contiguous buffers in RAM (especially EWRAM), this can be faster than an immediate DMA fill.

## API map

| API | What it represents | Typical use |
|-----|---------------------|-------------|
| `reg_dmasad[4]` | source address register per channel | manual setup |
| `reg_dmadad[4]` | destination address register per channel | manual setup |
| `reg_dmacnt_l[4]` | transfer unit count per channel | manual setup |
| `reg_dmacnt_h[4]` | `dma_control` flags per channel | timing, size, repeat, enable |
| `reg_dma[4]` | combined `volatile dma[4]` descriptor write | one-shot configuration |
| `dma_control` | low-level control bitfield | explicit register programming |
| `dma::copy()` | immediate 32-bit copy | VRAM/OAM/block copies |
| `dma::copy16()` | immediate 16-bit copy | palette or halfword tables |
| `dma::fill()` | immediate 32-bit fill (`src` fixed) | clears/pattern fills |
| `dma::fill16()` | immediate 16-bit fill | halfword fills |
| `dma::on_vblank()` | VBlank-triggered repeating transfer | per-frame buffered updates |
| `dma::on_hblank()` | HBlank-triggered repeating transfer | scanline effects |
| `dma::to_fifo_a()` | repeating FIFO A stream setup | DirectSound A |
| `dma::to_fifo_b()` | repeating FIFO B stream setup | DirectSound B |

## Choosing helper vs raw registers

Use `gba::dma` helpers when:

- transfer pattern is standard (copy/fill/vblank/hblank/fifo)
- you want fewer control-bit mistakes
- you do not need unusual flag combinations

Use raw registers when:

- you need custom `dma_control` fields not covered by helper defaults
- you are debugging exact channel state
- you are doing unusual timing/control experiments

## Immediate transfer examples

### 32-bit copy

```cpp
#include <gba/dma>

// Copy 256 words now.
gba::reg_dma[3] = gba::dma::copy(src, dst, 256);
```

### 16-bit copy

```cpp
#include <gba/dma>

// Copy 256 halfwords now.
gba::reg_dma[3] = gba::dma::copy16(src16, dst16, 256);
```

### 32-bit fill

```cpp
#include <gba/dma>

static constexpr unsigned int zero = 0;
gba::reg_dma[3] = gba::dma::fill(&zero, dst, 1024);
```

`fill()` and `fill16()` use fixed-source mode; the source points at the value to repeat.

## Timed transfer examples

### VBlank repeating transfer

Useful for per-frame buffered copies such as OAM shadow updates.

```cpp
#include <gba/dma>

// Run once per VBlank until disabled.
gba::reg_dma[3] = gba::dma::on_vblank(shadow_oam, oam_dst, 128);

// Later, stop channel 3.
gba::reg_dmacnt_h[3] = {};
```

### HBlank repeating transfer (HDMA)

Useful for scanline effects (scroll gradients, wave distortions, etc.).

```cpp
#include <gba/dma>

// One halfword per HBlank from a scanline table.
gba::reg_dma[0] = gba::dma::on_hblank(scanline_values, bg_hofs_reg_ptr, 1);

// Later, stop channel 0.
gba::reg_dmacnt_h[0] = {};
```

## DirectSound FIFO streaming

```cpp
#include <gba/dma>

// Common convention: DMA1 -> FIFO A, DMA2 -> FIFO B.
gba::reg_dma[1] = gba::dma::to_fifo_a(samples_a);
gba::reg_dma[2] = gba::dma::to_fifo_b(samples_b);
```

These helpers set fixed destination, repeat, 32-bit units, and sound FIFO timing.

## Manual register setup (raw path)

Equivalent to helper-style configuration when you need full control:

```cpp
#include <gba/dma>

gba::reg_dmasad[3] = src;
gba::reg_dmadad[3] = dst;
gba::reg_dmacnt_l[3] = 256;
gba::reg_dmacnt_h[3] = {
    .dest_op = gba::dest_op_increment,
    .src_op = gba::src_op_increment,
    .dma_type = gba::dma_type::word,
    .dma_cond = gba::dma_cond_now,
    .enable = true,
};
```

## Safety and correctness notes

- `count`/`units` means **transfer units**, not bytes.
  - `dma_type::half` -> halfwords
  - `dma_type::word` -> words
- For `fill()` and repeating transfers, source memory must remain valid while DMA can still run.
- Repeating channels keep firing until disabled (`reg_dmacnt_h[n] = {}`).
- Channel conventions are common practice, not hard rules:
  - DMA0: HBlank effects
  - DMA1/DMA2: DirectSound FIFO
  - DMA3: bulk/general transfers
- For VRAM/OAM writes, prefer VBlank/HBlank-safe timing patterns.

## See also

- [DMA Peripheral Reference](../reference/peripherals/dma.md)
- [Peripheral Register Reference](../reference/peripherals.md)
- [Video Memory](../reference/peripherals/video-memory.md)
