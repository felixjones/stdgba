# Undocumented Registers

These are functional but not part of the community-documented register set. Access via the `gba::undocumented` namespace.

| Address | stdgba | Access | Type | Common Name |
|---------|--------|--------|------|-------------|
| `0x4000002` | `undocumented::reg_stereo_3d` | RW | `bool` | GREENSWAP |
| `0x4000300` | `undocumented::reg_postflg` | RW | `bool` | POSTFLG |
| `0x4000301` | `undocumented::reg_haltcnt` | RW | `halt_control` | HALTCNT |
| `0x4000410` | `undocumented::reg_obj_center` | W | `volatile char` | - |
| `0x4000800` | `undocumented::reg_memcnt` | RW | `memory_control` | Internal Memory Control |
