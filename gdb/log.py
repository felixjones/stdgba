# AI Generated file! TODO: Replace with human code
"""
GDB pretty printer for gba::log types

Displays log levels in readable form.

Example output:
    (gba::log::level) info
"""

import gdb

LEVEL_NAMES = {
    0: 'fatal',
    1: 'error',
    2: 'warn',
    3: 'info',
    4: 'debug',
}


class LogLevelPrinter:
    """Pretty printer for gba::log::level"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            raw = int(self.val)

            if raw in LEVEL_NAMES:
                return LEVEL_NAMES[raw]

            return f"level({raw})"

        except Exception as e:
            return f"<error: {e}>"


def log_lookup(val):
    """Lookup function for log types."""
    type_name = str(val.type.strip_typedefs())

    if type_name == 'gba::log::level':
        return LogLevelPrinter(val)

    return None


def register_printers(objfile=None):
    """Register the log pretty printers."""
    if objfile is None:
        objfile = gdb

    objfile.pretty_printers.append(log_lookup)


# Auto-register when sourced
register_printers()
print("Loaded stdgba log pretty printer")
