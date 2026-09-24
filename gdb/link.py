"""
GDB pretty printers for stdgba link transports.

Displays link error masks and transport state without decoding message payloads.
"""

import gdb


def array_elements(array):
    return array["_M_elems"]


def error_names(value):
    flags = (
        ("receive_overflow", 1 << 0),
        ("malformed_frame", 1 << 1),
        ("integrity_failure", 1 << 2),
        ("decode_failure", 1 << 3),
        ("hardware_failure", 1 << 4),
        ("topology_changed", 1 << 5),
    )
    return "|".join(name for name, bit in flags if value & bit) or "none"


def stream_summary(stream):
    send_index = int(stream["m_sendIndex"])
    send_size = int(stream["m_sendSize"])
    return f"sending={send_index < send_size} wire={send_index}/{send_size}"


def peer_summary(peer):
    pending = int(peer["wire"]["m_size"])
    errors = int(peer["errors"]["m_errors"])
    return f"pending_wire={pending}, errors={error_names(errors)}"


class LinkErrorPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        return error_names(int(self.val))


class LinkMultiPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            stream = self.val["m_stream"]
            connected = array_elements(self.val["m_connected"])
            peers = ",".join(str(index) for index in range(4) if bool(connected[index]))
            return f"link_multi({stream_summary(stream)}, busy={bool(self.val['m_busy'])}, connected={{{peers}}})"
        except gdb.error as error:
            return f"<error: {error}>"

    def children(self):
        try:
            peers = array_elements(self.val["m_stream"]["m_peers"])
            for index in range(4):
                yield (f"player_{index}", peer_summary(peers[index]))
        except gdb.error:
            return


class LinkNormalPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            return f"link_normal({stream_summary(self.val['m_stream'])}, busy={bool(self.val['m_busy'])})"
        except gdb.error as error:
            return f"<error: {error}>"

    def children(self):
        try:
            yield ("peer", peer_summary(self.val["m_stream"]["m_peers"][0]))
        except gdb.error:
            return


class LinkSlotPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            pointer = int(self.val["m_link"])
            return "link_slot(empty)" if pointer == 0 else f"link_slot(active=@{pointer:#010x})"
        except gdb.error as error:
            return f"<error: {error}>"


def link_lookup(val):
    type_name = str(val.type.strip_typedefs())

    if type_name == "gba::bits::link::link_error":
        return LinkErrorPrinter(val)
    if type_name.startswith("gba::bits::link::link_multi<"):
        return LinkMultiPrinter(val)
    if type_name.startswith("gba::bits::link::link_normal<"):
        return LinkNormalPrinter(val)
    if type_name == "gba::bits::link::link_slot":
        return LinkSlotPrinter(val)
    return None


def register_printers(objfile=None):
    if objfile is None:
        objfile = gdb
    objfile.pretty_printers.append(link_lookup)


register_printers()
print("Loaded stdgba link pretty printer")
