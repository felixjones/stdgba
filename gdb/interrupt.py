"""
GDB pretty printers for gba::interrupt types

Displays:
- gba::irq: bitfield of interrupt sources
- gba::irq_handler: handler function pointer and state
"""

import gdb

class IrqPrinter:
    """Pretty printer for gba::irq"""

    IRQ_FLAGS = [
        ("vblank", 0),
        ("hblank", 1),
        ("vcount", 2),
        ("timer0", 3),
        ("timer1", 4),
        ("timer2", 5),
        ("timer3", 6),
        ("serial", 7),
        ("dma0", 8),
        ("dma1", 9),
        ("dma2", 10),
        ("dma3", 11),
        ("keypad", 12),
        ("gamepak", 13),
    ]

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            flags = []
            for name, _ in self.IRQ_FLAGS:
                field_name = 'vcounter' if name == 'vcount' else name
                if bool(self.val[field_name]):
                    flags.append(name)

            if flags:
                return "{" + "|".join(flags) + "}"
            else:
                return "{(none)}"

        except Exception as e:
            return f"<error: {e}>"


class IrqHandlerPrinter:
    """Pretty printer for gba::isr / gba::irq_handler"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            return "irq_handler singleton"

        except Exception as e:
            return f"<error: {e}>"


def interrupt_lookup(val):
    """Lookup function for interrupt types."""
    type_name = str(val.type.strip_typedefs())

    if type_name == 'gba::irq':
        return IrqPrinter(val)

    if type_name == 'gba::isr':
        return IrqHandlerPrinter(val)

    return None


def register_printers(objfile=None):
    """Register the interrupt pretty printers."""
    if objfile is None:
        objfile = gdb

    objfile.pretty_printers.append(interrupt_lookup)


# Auto-register when sourced
register_printers()
print("Loaded stdgba interrupt pretty printer")
