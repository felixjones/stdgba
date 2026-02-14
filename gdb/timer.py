"""
GDB pretty printer for gba::timer::compiled_timer

Displays compile-time timer configuration (prescaler, reload, cascade)

Example output:
    (gba::timer::compiled_timer) [0] prescaler=1024, reload=16384, enable, irq
    (gba::timer::compiled_timer) [1] cascade, enable
"""

import gdb

class CompiledTimerPrinter:
    """Pretty printer for gba::timer::compiled_timer"""

    PRESCALER_NAMES = {
        0: "1",
        1: "64",
        2: "256",
        3: "1024",
    }

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            count = int(self.val['m_count'])
            entries = []
            timers = self.val['m_timers']
            for i in range(count):
                entries.append(f"[{i}] {self._format_timer_entry(timers[i])}")

            if entries:
                return " | ".join(entries)

            return "compiled_timer(count=0)"

        except Exception as e:
            return f"<error: {e}>"

    def _format_timer_entry(self, entry):
        """Format a single timer entry"""
        try:
            info = []

            reload = int(entry['reload']) & 0xFFFF
            info.append(f"reload={reload}")

            control = entry['control']
            if bool(control['cascade']):
                info.append("cascade")
            else:
                prescaler_val = int(control['cycles'])
                prescaler_name = self.PRESCALER_NAMES.get(prescaler_val, str(prescaler_val))
                info.append(f"prescaler={prescaler_name}")

            if bool(control['enabled']):
                info.append("enable")

            if bool(control['overflow_irq']):
                info.append("irq")

            if info:
                return ", ".join(info)
            else:
                return "timer_config"

        except:
            return "timer"


def timer_lookup(val):
    """Lookup function for timer types."""
    type_name = str(val.type.strip_typedefs())
    if type_name == 'gba::compiled_timer':
        return CompiledTimerPrinter(val)
    return None


def register_printers(objfile=None):
    """Register the timer pretty printers."""
    if objfile is None:
        objfile = gdb

    objfile.pretty_printers.append(timer_lookup)


# Auto-register when sourced
register_printers()
print("Loaded stdgba timer pretty printer")
