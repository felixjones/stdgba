# Video Memory

Palette memory symbols are declared in `<gba/color>`. VRAM and OAM symbols are declared in `<gba/video>`.

| Address | stdgba | Type | tonclib |
|---------|--------|------|---------|
| `0x5000000` | `mem_pal` | `short[512]` | `pal_mem` |
| `0x5000000` | `mem_pal_bg` | `short[256]` | `pal_bg_mem` |
| `0x5000200` | `mem_pal_obj` | `short[256]` | `pal_obj_mem` |
| `0x5000000` | `pal_bg_mem` | `color[256]` | `pal_bg_mem` |
| `0x5000200` | `pal_obj_mem` | `color[256]` | `pal_obj_mem` |
| `0x5000000` | `pal_bg_bank` | `color[16][16]` | `pal_bg_bank` |
| `0x5000200` | `pal_obj_bank` | `color[16][16]` | `pal_obj_bank` |
| `0x6000000` | `mem_vram` | `short[0xC000]` | `vid_mem` |
| `0x6000000` | `mem_vram_bg` | `short[0x8000]` | `vid_mem` |
| `0x6010000` | `mem_vram_obj` | `short[0x4000]` | `tile_mem_obj` |
| `0x6000000` | `mem_tile_4bpp` | `tile4bpp[4][512]` | `tile_mem` |
| `0x6000000` | `mem_tile_8bpp` | `tile8bpp[4][256]` | `tile8_mem` |
| `0x6000000` | `mem_se` | `screen_entry[32][1024]` | `se_mem` |
| `0x7000000` | `mem_oam` | `short[128][3]` | `oam_mem` |
| `0x7000000` | `obj_mem` | [`object[128]`](../object.md) | `obj_mem` |
| `0x7000000` | `obj_aff_mem` | [`object_affine[128]`](../object-affine.md) | `obj_aff_mem` |
| `0x7000006` | `mem_obj_aff` | `fixed<short>[128]` | - |
| `0x7000006` | `mem_obj_affa` | `fixed<short>[32]` | `obj_aff_mem[n].pa` |
| `0x700000E` | `mem_obj_affb` | `fixed<short>[32]` | `obj_aff_mem[n].pb` |
| `0x7000016` | `mem_obj_affc` | `fixed<short>[32]` | `obj_aff_mem[n].pc` |
| `0x700001E` | `mem_obj_affd` | `fixed<short>[32]` | `obj_aff_mem[n].pd` |
