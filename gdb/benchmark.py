"""
GDB pretty printer for gba::benchmark::cycle_counter

Displays cycle count as both cycles and milliseconds (16.78 MHz GBA clock)

Example output:
    (gba::benchmark::cycle_counter) 12345 cycles (~0.735ms)
    (gba::benchmark::cycle_counter) 67890 cycles (~4.043ms)
"""

import gdb

class CycleCounterPrinter:
    """Pretty printer for gba::benchmark::cycle_counter"""

    # GBA runs at 16.78 MHz (16777216 cycles per second)
    GBA_CLOCK_HZ = 16777216

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            pair = self.val['m_pair']
            low = int(pair['low'])
            high = int(pair['high'])
            return f"timer_pair=TM{low}+TM{high}"

        except Exception as e:
            return f"<error: {e}>"


def benchmark_lookup(val):
    """Lookup function for benchmark types."""
    type_name = str(val.type.strip_typedefs())
    if type_name == 'gba::benchmark::cycle_counter':
        return CycleCounterPrinter(val)
    return None


def register_printers(objfile=None):
    """Register the benchmark pretty printers."""
    if objfile is None:
        objfile = gdb

    objfile.pretty_printers.append(benchmark_lookup)


# Auto-register when sourced
register_printers()
print("Loaded stdgba benchmark pretty printer")
