# Colour Effects

| Address | stdgba | Access | Type | tonclib |
|---------|--------|--------|------|---------|
| `0x4000050` | `reg_bldcnt` | RW | `blend_control` | `REG_BLDCNT` |
| `0x4000052` | `reg_bldalpha[2]` | RW | `fixed<unsigned char>[2]` | `REG_BLDALPHA` |
| `0x4000054` | `reg_bldy` | RW | `fixed<unsigned char>` | `REG_BLDY` |

## `blend_control`

```cpp
struct blend_control {
    bool dest_bg0 : 1;    // 2nd target layers
    bool dest_bg1 : 1;
    bool dest_bg2 : 1;
    bool dest_bg3 : 1;
    bool dest_obj : 1;
    bool dest_backdrop : 1;
    blend_op blend_op : 2; // none / alpha / brighten / darken
    bool src_bg0 : 1;     // 1st target layers
    bool src_bg1 : 1;
    bool src_bg2 : 1;
    bool src_bg3 : 1;
    bool src_obj : 1;
    bool src_backdrop : 1;
};
```

```cpp
gba::reg_bldcnt = {
    .src_bg0 = true,
    .dest_bg1 = true,
    .blend_op = gba::blend_op_alpha
};
gba::reg_bldalpha[0] = 0.5_fx; // EVA (source weight)
gba::reg_bldalpha[1] = 0.5_fx; // EVB (target weight)
```
