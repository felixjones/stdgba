# System

| Address | stdgba | Access | Type | tonclib |
|---------|--------|--------|------|---------|
| `0x4000204` | `reg_waitcnt` | RW | `waitcnt` | `REG_WAITCNT` |

## `waitcnt`

`waitcnt` is the GBA wait-control register (`WAITCNT`), also referred to as `waitctl` in some documentation.

```cpp
struct waitcnt {
    unsigned short sram : 2{3};
    unsigned short ws0_first : 2{1};
    unsigned short ws0_second : 1{1};
    unsigned short ws1_first : 2{};
    unsigned short ws1_second : 1{};
    unsigned short ws2_first : 2{3};
    unsigned short ws2_second : 1{};
    unsigned short phi : 2{};
    short : 1;
    bool prefetch : 1{true};
    const bool is_cgb : 1{};
};
```

Default-initializing with `{}` sets optimal ROM access timings and enables the prefetch buffer:

```cpp
gba::reg_waitcnt = {};
```
