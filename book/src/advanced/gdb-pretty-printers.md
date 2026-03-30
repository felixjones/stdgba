# GDB Pretty Printers

stdgba ships Python pretty-printers under `gdb/` so common library types are shown in a readable form while debugging.

Instead of raw storage fields, GDB can show decoded values such as fixed-point numbers, angles in degrees, key masks, timer configuration, and music tokens.

## Quick start

Load the aggregate script once per GDB session:

```gdb
source D:/CLionProjects/stdgba/gdb/stdgba.py
```

To load them automatically, add the same `source ...` line to your `.gdbinit`.

When loaded successfully, GDB prints status lines including:

- `Loading stdgba pretty printers...`
- `stdgba pretty printers loaded successfully`

## Available printers

The aggregate loader `gdb/stdgba.py` imports and registers these printer modules:

| Module | Example types |
|---|---|
| `gdb/fixed_point.py` | `gba::fixed<Rep, FracBits>` |
| `gdb/angle.py` | `gba::angle`, `gba::packed_angle<Bits>` |
| `gdb/format.py` | `gba::format::compiled_format`, `arg_binder`, `bound_arg`, `format_generator` |
| `gdb/music.py` | `gba::music::note`, `bpm_value`, `token_type`, `ast_type`, `token`, pattern types |
| `gdb/log.py` | `gba::log::level` |
| `gdb/video.py` | `gba::color`, `gba::object` |
| `gdb/keyinput.py` | `gba::keypad` |
| `gdb/key.py` | `gba::key` |
| `gdb/registral.py` | `gba::registral<T>` |
| `gdb/memory.py` | `gba::plex<...>`, `gba::unique<T>`, `gba::bitpool` |
| `gdb/benchmark.py` | `gba::benchmark::cycle_counter` |
| `gdb/interrupt.py` | `gba::irq`, `gba::irq_handler` |
| `gdb/timer.py` | `gba::timer::compiled_timer` |

You can also source any individual module directly if you only want one printer.

## Practical workflow

`tests/debug/test_pretty_printers.cpp` constructs representative values for all supported printer categories and includes a dedicated breakpoint marker comment.

Build the manual test target:

```bash
cmake --build build --target test_pretty_printers - -j 8
```

Start GDB with the produced ELF:

```bash
arm-none-eabi-gdb build/tests/test_pretty_printers.elf
```

Inside GDB:

```gdb
source D:/CLionProjects/stdgba/gdb/stdgba.py
break main
run
# Step/next until the BREAKPOINT HERE marker in test_pretty_printers.cpp
print fix8_val
print angle_90
print key_combo
print test_pool
```

Expected output is human-readable (for example fixed-point decimal form and decoded key masks), rather than only raw integer fields.

## Notes

- `test_pretty_printers` is listed in `tests/CMakeLists.txt` under `MANUAL_TESTS`, so it is intentionally excluded from CTest automation.
- Pretty-printers are a debugger convenience only; they do not affect generated ROM code or runtime behaviour.
- If GDB warns about auto-load restrictions, allow the script path in your local GDB security settings before sourcing the file.
