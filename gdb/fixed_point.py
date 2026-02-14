# AI Generated file! TODO: Replace with human code
"""
GDB pretty printer for gba::fixed<Rep, FracBits, IntermediateRep>

Displays fixed-point values as decimal numbers with their underlying representation.

Example output:
    (gba::fixed<int, 8, long long>) 3.5 [raw=0x380]
"""

import gdb
import re


class FixedPointPrinter:
    """Pretty printer for gba::fixed<Rep, FracBits, IntermediateRep>"""

    def __init__(self, val):
        self.val = val
        self.type = val.type.strip_typedefs()

    def to_string(self):
        try:
            type_name = str(self.type)

            # Parse gba::fixed<Rep, FracBits, IntermediateRep>
            # Handle various formats GDB might use
            match = re.search(r'gba::fixed<[^,]+,\s*(\d+)', type_name)
            if match:
                frac_bits = int(match.group(1))
            else:
                # Default fallback
                frac_bits = 8

            # Try to get raw value - could be m_data or the value itself
            try:
                raw = int(self.val['m_data'])
            except:
                # Maybe it's directly accessible
                try:
                    raw = int(self.val)
                except:
                    return f"<cannot read value>"

            # Convert to decimal
            scale = 1 << frac_bits
            value = raw / scale

            # Format output
            return f"{value:.6g} [raw={raw:#x}]"

        except Exception as e:
            return f"<error: {e}>"


def fixed_point_lookup(val):
    """Lookup function for fixed-point types."""
    type_name = str(val.type.strip_typedefs())

    if 'gba::fixed<' in type_name:
        return FixedPointPrinter(val)

    # Also handle fixed_literal
    if 'gba::literals::fixed_literal' in type_name:
        return FixedLiteralPrinter(val)

    return None


class FixedLiteralPrinter:
    """Pretty printer for gba::literals::fixed_literal"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            value = float(self.val['value'])
            return f"{value:.6g}_fx"
        except Exception as e:
            return f"<error: {e}>"


def register_printers(objfile=None):
    """Register the fixed-point pretty printer."""
    if objfile is None:
        objfile = gdb

    objfile.pretty_printers.append(fixed_point_lookup)


# Auto-register when sourced
register_printers()
print("Loaded stdgba fixed_point pretty printer")
