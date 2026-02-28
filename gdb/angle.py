"""
GDB pretty printer for gba::angle and gba::packed_angle<Bits>

Displays angle values in degrees with their underlying representation.

Example output:
    (gba::angle) 90.00deg [raw=0x40000000]
    (gba::packed_angle<16>) 45.00deg [raw=0x2000]
"""

import gdb
import re

class AnglePrinter:
    """Pretty printer for gba::angle"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            raw = int(self.val['m_data'])

            # Full 32-bit range = 360 degrees
            # Handle as unsigned
            if raw < 0:
                raw = raw + (1 << 32)

            degrees = (raw / (2**32)) * 360.0

            return f"{degrees:.2f}deg [raw={raw:#010x}]"

        except Exception as e:
            return f"<error: {e}>"


class PackedAnglePrinter:
    """Pretty printer for gba::packed_angle<Bits>"""

    def __init__(self, val, bits):
        self.val = val
        self.bits = bits

    def to_string(self):
        try:
            raw = int(self.val['m_data'])

            # Handle as unsigned
            if raw < 0:
                raw = raw + (1 << self.bits)

            # Full range for this bit width = 360 degrees
            max_val = 2 ** self.bits
            degrees = (raw / max_val) * 360.0

            hex_width = (self.bits + 3) // 4 + 2  # +2 for "0x"
            return f"{degrees:.2f}deg [raw={raw:#0{hex_width}x}]"

        except Exception as e:
            return f"<error: {e}>"


class AngleLiteralPrinter:
    """Pretty printer for gba::literals::angle_literal"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            turns = float(self.val['turns'])
            degrees = turns * 360.0
            return f"{degrees:.2f}deg (literal)"
        except Exception as e:
            return f"<error: {e}>"


def angle_lookup(val):
    """Lookup function for angle types."""
    type_name = str(val.type.strip_typedefs())

    if type_name == 'gba::angle':
        return AnglePrinter(val)

    # Match gba::packed_angle<N>
    match = re.match(r'gba::packed_angle<(\d+)>', type_name)
    if match:
        bits = int(match.group(1))
        return PackedAnglePrinter(val, bits)

    if 'angle_literal' in type_name:
        return AngleLiteralPrinter(val)

    return None


def register_printers(objfile=None):
    """Register the angle pretty printers."""
    if objfile is None:
        objfile = gdb

    objfile.pretty_printers.append(angle_lookup)


# Auto-register when sourced
register_printers()
print("Loaded stdgba angle pretty printer")
