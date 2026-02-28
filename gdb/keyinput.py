"""
GDB pretty printer for gba::keyinput types

Displays key masks and keypad state in readable form.

Example output:
    (gba::key) A|B
    (gba::keypad) held=A|RIGHT pressed=B
"""

import gdb

KEY_NAMES = [
    (0x0001, 'A'),
    (0x0002, 'B'),
    (0x0004, 'SELECT'),
    (0x0008, 'START'),
    (0x0010, 'RIGHT'),
    (0x0020, 'LEFT'),
    (0x0040, 'UP'),
    (0x0080, 'DOWN'),
    (0x0100, 'R'),
    (0x0200, 'L'),
]


def keys_to_string(mask):
    """Convert a key bitmask to a string of key names."""
    if mask == 0:
        return "none"

    names = [name for bit, name in KEY_NAMES if mask & bit]
    return '|'.join(names) if names else f"0x{mask:03x}"


class KeyPrinter:
    """Pretty printer for gba::key"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            raw = int(self.val['m_data'])
            return keys_to_string(raw)
        except Exception as e:
            return f"<error: {e}>"


class KeypadPrinter:
    """Pretty printer for gba::keypad

    m_data layout:
    - Bits 0-9: Current key state (0 = pressed, from active-low hardware)
    - Bits 16-25: Edge detection (XOR of previous and current)

    Default m_data is 0x3ff (no keys pressed).
    """

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            raw = int(self.val['m_data'])

            # Low 10 bits: current keys (0 = pressed due to active-low)
            current_raw = raw & 0x3FF
            # Invert to get "1 = pressed" semantics
            held = (~current_raw) & 0x3FF

            # High 16 bits: edge detection mask (XOR of prev and current)
            edges = (raw >> 16) & 0x3FF

            # Pressed this frame: edge detected AND currently held
            pressed = edges & held
            # Released this frame: edge detected AND NOT currently held
            released = edges & ~held

            parts = []

            if held:
                parts.append(f"held={keys_to_string(held)}")

            if pressed:
                parts.append(f"pressed={keys_to_string(pressed)}")

            if released:
                parts.append(f"released={keys_to_string(released)}")

            if not parts:
                return "no keys"

            return ' '.join(parts)

        except Exception as e:
            return f"<error: {e}>"


class KeyControlPrinter:
    """Pretty printer for gba::key_control"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            raw = int(self.val.cast(gdb.lookup_type('unsigned short')))
            # Hardware register is active-low
            active = (~raw) & 0x3FF
            return keys_to_string(active)
        except Exception as e:
            return f"<error: {e}>"


def keyinput_lookup(val):
    """Lookup function for keyinput types."""
    type_name = str(val.type.strip_typedefs())

    if type_name == 'gba::key':
        return KeyPrinter(val)

    if type_name == 'gba::keypad':
        return KeypadPrinter(val)

    if type_name == 'gba::key_control':
        return KeyControlPrinter(val)

    return None


def register_printers(objfile=None):
    """Register the keyinput pretty printers."""
    if objfile is None:
        objfile = gdb

    objfile.pretty_printers.append(keyinput_lookup)


# Auto-register when sourced
register_printers()
print("Loaded stdgba keyinput pretty printer")
