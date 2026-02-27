# DMA Transfers

DMA (Direct Memory Access) copies memory without CPU involvement. The GBA has 4 DMA channels (0-3), each capable of transferring 16-bit or 32-bit words between any memory regions. DMA3 is the general-purpose channel; DMA1/DMA2 are used for audio streaming.

## Factory functions

stdgba provides factory functions that return a complete DMA register configuration:

### Immediate copy

```cpp
#include <gba/dma>

// 32-bit copy (word)
gba::reg_dma[3] = gba::dma::copy(src, dst, word_count);

// 16-bit copy (halfword)
gba::reg_dma[3] = gba::dma::copy16(src, dst, halfword_count);
```

### Fill

```cpp
// Fill memory with a constant value
// Note: source must be a pointer to a persistent value (not a temporary)
static constexpr unsigned int zero = 0;
gba::reg_dma[3] = gba::dma::fill(&zero, dst, word_count);
```

### Triggered transfers

```cpp
// Copy OAM shadow buffer every VBlank
gba::reg_dma[3] = gba::dma::on_vblank(oam_shadow, oam_mem, 128);

// HDMA: transfer one entry per scanline
gba::reg_dma[0] = gba::dma::on_hblank(scroll_table, &bg_hofs, 1);
```

### Audio FIFO

```cpp
// Stream PCM samples to sound FIFO A
gba::reg_dma[1] = gba::dma::to_fifo_a(sample_buffer);

// Stream to FIFO B
gba::reg_dma[2] = gba::dma::to_fifo_b(sample_buffer);
```

## DMA channels

| Channel | Priority | Special capabilities |
|---------|----------|---------------------|
| DMA0 | Highest | HBlank DMA |
| DMA1 | High | Sound FIFO A |
| DMA2 | Medium | Sound FIFO B |
| DMA3 | Lowest | General purpose, game pak |

Higher-priority channels preempt lower ones. DMA0 should be reserved for time-critical HBlank effects.

## Raw register access

For full control:

```cpp
gba::reg_dmasad[3] = src;     // Source address
gba::reg_dmadad[3] = dst;     // Destination address
gba::reg_dmacnt_l[3] = count; // Transfer count
gba::reg_dmacnt_h[3] = {      // Control
    .dma_type = gba::dma_type::word,
    .enable = true,
};
```

## tonclib comparison

| stdgba | tonclib |
|--------|---------|
| `gba::dma::copy(s, d, n)` | `dma3_cpy(d, s, n)` |
| `gba::dma::fill(&v, d, n)` | `dma3_fill(d, v, n)` |
| `gba::reg_dma[3] = ...;` | Manual register writes |
