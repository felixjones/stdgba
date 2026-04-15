"""
stdgba GDB Pretty Printers

This script loads all stdgba pretty printers for GDB.

Usage in GDB:
    source /path/to/stdgba/gdb/stdgba.py

Or add to your .gdbinit:
    source /path/to/stdgba/gdb/stdgba.py

Individual printers can also be loaded separately:
    source /path/to/stdgba/gdb/fixed_point.py
    source /path/to/stdgba/gdb/angle.py
    source /path/to/stdgba/gdb/format.py
    source /path/to/stdgba/gdb/log.py
    source /path/to/stdgba/gdb/video.py
    source /path/to/stdgba/gdb/keyinput.py
    source /path/to/stdgba/gdb/registral.py
    source /path/to/stdgba/gdb/memory.py
    source /path/to/stdgba/gdb/key.py
    source /path/to/stdgba/gdb/benchmark.py
    source /path/to/stdgba/gdb/interrupt.py
    source /path/to/stdgba/gdb/timer.py

Supported types:
    - gba::fixed<Rep, FracBits>      -> "3.5 [raw=0x380]"
    - gba::angle                     -> "90.00deg [raw=0x40000000]"
    - gba::packed_angle<Bits>        -> "45.00deg [raw=0x2000]"
    - gba::format::compiled_format   -> format("HP: {hp}")
    - gba::arg_binder                -> arg("hp")
    - gba::bound_arg                 -> = 42
    - gba::format::format_generator  -> generator("...") [seg=0, char=3]
    - gba::log::level                -> "info"
    - gba::color                     -> rgb(31, 15, 0)
    - gba::object                    -> obj(x=120, y=80, tile=0)
    - gba::key                       -> "A|B"
    - gba::keypad                    -> held=A|RIGHT pressed=B
    - gba::registral<T>              -> @0x4000000 = {value...}
    - gba::plex<...>                 -> {100, 200}
    - gba::unique<T>                 -> -> 42
    - gba::bitpool                   -> pool(base=0x06010000, chunk=1024, used=5/32)
    - gba::benchmark::cycle_counter  -> "12345 cycles (~0.735ms)"
    - gba::irq                       -> "{vblank|timer0|dma0}"
    - gba::irq_handler               -> "handler=0x8000000 (installed)"
    - gba::timer::compiled_timer     -> "[0] prescaler=1024, reload=16384, enable, irq"
"""

import os
import sys

# Get the directory containing this script
_script_dir = os.path.dirname(os.path.abspath(__file__))

# Add script directory to Python path for imports
if _script_dir not in sys.path:
    sys.path.insert(0, _script_dir)

# Import and register all pretty printers
print("Loading stdgba pretty printers...")

try:
    import fixed_point
except ImportError as e:
    print(f"  Warning: Could not load fixed_point printer: {e}")

try:
    import angle
except ImportError as e:
    print(f"  Warning: Could not load angle printer: {e}")

try:
    import format
except ImportError as e:
    print(f"  Warning: Could not load format printer: {e}")

try:
    import music
except ImportError as e:
    print(f"  Warning: Could not load music printer: {e}")

try:
    import log
except ImportError as e:
    print(f"  Warning: Could not load log printer: {e}")

try:
    import video
except ImportError as e:
    print(f"  Warning: Could not load video printer: {e}")

try:
    import keyinput
except ImportError as e:
    print(f"  Warning: Could not load keyinput printer: {e}")

try:
    import registral
except ImportError as e:
    print(f"  Warning: Could not load registral printer: {e}")

try:
    import memory
except ImportError as e:
    print(f"  Warning: Could not load memory printer: {e}")

try:
    import key
except ImportError as e:
    print(f"  Warning: Could not load key printer: {e}")

try:
    import benchmark
except ImportError as e:
    print(f"  Warning: Could not load benchmark printer: {e}")

try:
    import interrupt
except ImportError as e:
    print(f"  Warning: Could not load interrupt printer: {e}")

try:
    import timer
except ImportError as e:
    print(f"  Warning: Could not load timer printer: {e}")

print("stdgba pretty printers loaded successfully")
