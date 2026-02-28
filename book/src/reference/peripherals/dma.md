# DMA

Declared in `<gba/dma>`.

| Address | stdgba | Access | Type | tonclib |
|---------|--------|--------|------|---------|
| `0x40000B0` | `reg_dmasad[4]` | W | `const void* volatile[4]` | `REG_DMA0SAD`..`REG_DMA3SAD` |
| `0x40000B4` | `reg_dmadad[4]` | W | `void* volatile[4]` | `REG_DMA0DAD`..`REG_DMA3DAD` |
| `0x40000B8` | `reg_dmacnt_l[4]` | W | `volatile unsigned short[4]` | `REG_DMA0CNT_L`..`REG_DMA3CNT_L` |
| `0x40000BA` | `reg_dmacnt_h[4]` | RW | `dma_control[4]` | `REG_DMA0CNT_H`..`REG_DMA3CNT_H` |
| `0x40000B0` | `reg_dma[4]` | W | `volatile dma[4]` | - |

All DMA arrays have a stride of 12 bytes between channels.

## `dma_control`

```cpp
struct dma_control {
    short : 5;
    dest_op dest_op : 2;   // increment / decrement / fixed / increment_reload
    src_op src_op : 2;     // increment / decrement / fixed
    bool repeat : 1;
    dma_type dma_type : 1; // half (16-bit) / word (32-bit)
    bool gamepak_drq : 1;
    dma_cond dma_cond : 2; // now / vblank / hblank / sound_fifo (or video_capture)
    bool irq_on_finish : 1;
    bool enable : 1;
};
```

## `dma` - high-level descriptor

```cpp
struct dma {
    const void* source;
    void* destination;
    unsigned short units;
    dma_control control;

    static constexpr dma copy(const void* src, void* dst, std::size_t count);
    static constexpr dma copy16(const void* src, void* dst, std::size_t count);
    static constexpr dma fill(const void* val, void* dst, std::size_t count);
    static constexpr dma fill16(const void* val, void* dst, std::size_t count);
    static constexpr dma on_vblank(const void* src, void* dst, std::size_t count);
    static constexpr dma on_hblank(const void* src, void* dst, std::size_t count);
    static constexpr dma to_fifo_a(const void* samples);
    static constexpr dma to_fifo_b(const void* samples);
};
```

```cpp
gba::reg_dma[3] = gba::dma::copy(src, dst, 256);
```
