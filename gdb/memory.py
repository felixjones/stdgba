"""
GDB pretty printer for gba::memory types

Displays plex, unique, and bitpool types in readable form.

Example output:
    (gba::plex<short, short>) {100, 200}
    (gba::unique<int>) -> 42
    (gba::bitpool) pool(base=0x06010000, chunk=1024, used=5/32)
"""

import gdb
import re


class PlexPrinter:
    """Pretty printer for gba::plex<...>"""

    def __init__(self, val):
        self.val = val
        self.type = val.type.strip_typedefs()

    def to_string(self):
        try:
            # Determine number of elements from type
            elements = []

            # Try to read each member
            try:
                elements.append(str(self.val['first']))
            except:
                pass

            try:
                elements.append(str(self.val['second']))
            except:
                pass

            try:
                elements.append(str(self.val['third']))
            except:
                pass

            try:
                elements.append(str(self.val['fourth']))
            except:
                pass

            # Get raw 32-bit value by casting
            try:
                raw = int(self.val.cast(gdb.lookup_type('unsigned int')))
                raw_str = f" [raw={raw:#010x}]"
            except:
                raw_str = ""

            if elements:
                return '{' + ', '.join(elements) + '}' + raw_str

            return '{}' + raw_str

        except Exception as e:
            return f"<error: {e}>"


class UniquePrinter:
    """Pretty printer for gba::unique<T>"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            ptr = self.val['m_ptr']

            if int(ptr) == 0:
                return "nullptr"

            # Try to dereference and show value
            try:
                deref = ptr.dereference()
                return f"-> {deref}"
            except:
                return f"-> @{int(ptr):#x}"

        except Exception as e:
            return f"<error: {e}>"


class BitpoolPrinter:
    """Pretty printer for gba::bitpool"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            base = int(self.val['m_base'])
            chunk_size = int(self.val['m_chunkSize'])
            bitmask = int(self.val['m_bitmask'])

            # Count used chunks (popcount)
            used = bin(bitmask).count('1')

            return f"pool(base={base:#010x}, chunk={chunk_size}, used={used}/32)"

        except Exception as e:
            return f"<error: {e}>"

    def children(self):
        """Show bitmask as binary for visualization."""
        try:
            bitmask = int(self.val['m_bitmask'])
            # Show as 32-bit binary string with chunks marked
            binary = format(bitmask, '032b')
            yield ('allocation_map', f"0b{binary}")
        except:
            pass


def memory_lookup(val):
    """Lookup function for memory types."""
    type_name = str(val.type.strip_typedefs())

    if 'gba::plex<' in type_name:
        return PlexPrinter(val)

    if 'gba::unique<' in type_name:
        return UniquePrinter(val)

    if type_name == 'gba::bitpool':
        return BitpoolPrinter(val)

    return None


def register_printers(objfile=None):
    """Register the memory pretty printers."""
    if objfile is None:
        objfile = gdb

    objfile.pretty_printers.append(memory_lookup)


# Auto-register when sourced
register_printers()
print("Loaded stdgba memory pretty printer")
