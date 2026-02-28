# Interrupts

| Address | stdgba | Access | Type | tonclib |
|---------|--------|--------|------|---------|
| `0x4000200` | `reg_ie` | RW | `irq` | `REG_IE` |
| `0x4000202` | `reg_if` | RW | `irq` | `REG_IF` |
| `0x4000202` | `reg_if_stat` | R | `const irq` | `REG_IF` (read) |
| `0x4000202` | `reg_if_ack` | W | `volatile irq` | `REG_IF` (write) |
| `0x4000208` | `reg_ime` | RW | `bool` | `REG_IME` |

## `irq`

```cpp
struct irq {
    bool vblank : 1;
    bool hblank : 1;
    bool vcounter : 1;
    bool timer0 : 1;
    bool timer1 : 1;
    bool timer2 : 1;
    bool timer3 : 1;
    bool serial : 1;
    bool dma0 : 1;
    bool dma1 : 1;
    bool dma2 : 1;
    bool dma3 : 1;
    bool keypad : 1;
    bool gamepak : 1;
};
```

```cpp
gba::reg_ie = { .vblank = true };
gba::reg_ime = true;
```
