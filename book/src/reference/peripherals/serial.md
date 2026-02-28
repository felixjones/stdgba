# Serial Communication

| Address | stdgba | Access | Type | tonclib |
|---------|--------|--------|------|---------|
| `0x4000120` | `reg_siodata32` | RW | `unsigned int` | `REG_SIODATA32` |
| `0x4000120` | `reg_siomulti[4]` | RW | `unsigned short[4]` | `REG_SIOMULTI0`..`3` |
| `0x4000128` | `reg_siocnt` | RW | `sio_control` | `REG_SIOCNT` |
| `0x4000128` | `reg_siocnt_multi` | RW | `sio_multi_control` | `REG_SIOCNT` |
| `0x400012A` | `reg_siodata8` | RW | `unsigned char` | `REG_SIODATA8` |
| `0x400012A` | `reg_siomlt_send` | RW | `unsigned short` | `REG_SIOMLT_SEND` |
| `0x4000134` | `reg_rcnt` | RW | `rcnt_control` | `REG_RCNT` |
| `0x4000140` | `reg_joycnt` | RW | `joycnt_control` | `REG_JOYCNT` |
| `0x4000150` | `reg_joy_recv` | R | `const unsigned int` | `REG_JOY_RECV` |
| `0x4000154` | `reg_joy_trans` | W | `volatile unsigned int` | `REG_JOY_TRANS` |
| `0x4000158` | `reg_joystat` | RW | `joystat_status` | `REG_JOYSTAT` |

The serial registers at `0x4000120`-`0x400012A` are aliased for different modes. Use `reg_siocnt` for Normal mode and `reg_siocnt_multi` for Multi-Player mode. Likewise `reg_siodata32` / `reg_siomulti` share the same address.
