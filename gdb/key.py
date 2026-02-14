"""
GDB pretty printer for gba::key

Displays key bitmask as a string of button names (A|B|SELECT|START|etc)

Example output:
    (gba::key) A|B|RIGHT
    (gba::key) (empty)
"""

import gdb

class KeyPrinter:
    """Pretty printer for gba::key"""

    BUTTON_NAMES = [
        ("A", 0),
        ("B", 1),
        ("SELECT", 2),
        ("START", 3),
        ("RIGHT", 4),
        ("LEFT", 5),
        ("UP", 6),
        ("DOWN", 7),
        ("R", 8),
        ("L", 9),
    ]

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            data = int(self.val['m_data'])

            buttons = []
            for name, bit in self.BUTTON_NAMES:
                if data & (1 << bit):
                    buttons.append(name)

            if buttons:
                return "|".join(buttons)
            else:
                return "(empty)"

        except Exception as e:
            return f"<error: {e}>"


def key_lookup(val):
    """Lookup function for gba::key."""
    type_name = str(val.type.strip_typedefs())
    if type_name == 'gba::key':
        return KeyPrinter(val)
    return None


def register_printers(objfile=None):
    """Register the key pretty printer."""
    if objfile is None:
        objfile = gdb

    objfile.pretty_printers.append(key_lookup)


# Auto-register when sourced
register_printers()
print("Loaded stdgba key pretty printer")
