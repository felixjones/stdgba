"""
GDB pretty printers for stdgba backup tables.

Displays directory metadata only; it never reads or decodes backup-chip payloads.
"""

import gdb


def array_elements(array):
    return array["_M_elems"]


def array_values(array):
    elements = array_elements(array)
    lower, upper = elements.type.strip_typedefs().range()
    for index in range(lower, upper + 1):
        yield elements[index]


def entry_summary(entry):
    start = int(entry["start"])
    count = int(entry["count"])
    pending_start = int(entry["pending_start"])
    pending_count = int(entry["pending_count"])
    if pending_count:
        return f"committed=({start}, {count}), pending=({pending_start}, {pending_count})"
    if count:
        return f"committed=({start}, {count})"
    return "empty"


class BackupTablePrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            entries = tuple(array_values(self.val["m_entries"]))
            stored = sum(1 for entry in entries if int(entry["count"]))
            pending = sum(1 for entry in entries if int(entry["pending_count"]))
            return f"backup_table(fields={len(entries)}, stored={stored}, pending={pending})"
        except (gdb.error, TypeError) as error:
            return f"<error: {error}>"

    def children(self):
        try:
            for index, entry in enumerate(array_values(self.val["m_entries"])):
                yield (f"field_{index}", entry_summary(entry))
        except (gdb.error, TypeError):
            return


class BackupSlotPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            pointer = int(self.val["m_held"])
            return "backup_slot(empty)" if pointer == 0 else f"backup_slot(active=@{pointer:#010x})"
        except gdb.error as error:
            return f"<error: {error}>"


def backup_lookup(val):
    type_name = str(val.type.strip_typedefs())

    if type_name.startswith("gba::bits::backup::table<"):
        return BackupTablePrinter(val)
    if type_name == "gba::bits::backup::backup_slot":
        return BackupSlotPrinter(val)
    return None


def register_printers(objfile=None):
    if objfile is None:
        objfile = gdb
    objfile.pretty_printers.append(backup_lookup)


register_printers()
print("Loaded stdgba backup pretty printer")
