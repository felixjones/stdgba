# AI Generated file! TODO: Replace with human code
"""
GDB pretty printer for gba::registral types

Displays memory-mapped register wrappers with their address and current value.

Example output:
    (gba::registral<gba::display_control>) @0x4000000 = {video_mode=3, enable_bg2=true}
    (gba::registral<short[256]>) @0x5000000[256]
"""

import gdb
import re


class RegistralPrinter:
    """Pretty printer for gba::registral<ValueType>"""

    def __init__(self, val):
        self.val = val
        self.type = val.type.strip_typedefs()

    def to_string(self):
        try:
            # Get the address from m_address member
            addr = int(self.val['m_address'])

            type_name = str(self.type)

            # Extract the value type from gba::registral<ValueType>
            match = re.search(r'gba::registral<(.+)>$', type_name)
            if match:
                value_type_str = match.group(1)
            else:
                value_type_str = "?"

            # Try to read the actual memory value
            try:
                # Get the value_type from the registral
                # We need to dereference the address as the appropriate type
                value_type = self.type.template_argument(0)

                # Create a pointer to that type and read
                ptr_type = value_type.pointer()
                ptr = gdb.Value(addr).cast(ptr_type)
                value = ptr.dereference()

                # Format based on type
                if value_type.code == gdb.TYPE_CODE_STRUCT:
                    return f"@{addr:#010x} = {value}"
                elif value_type.code == gdb.TYPE_CODE_INT:
                    int_val = int(value)
                    return f"@{addr:#010x} = {int_val} ({int_val:#x})"
                elif value_type.code == gdb.TYPE_CODE_ARRAY:
                    # For arrays, just show address and size
                    size = value_type.sizeof // value_type.target().sizeof
                    return f"@{addr:#010x}[{size}]"
                else:
                    return f"@{addr:#010x} = {value}"

            except Exception:
                # If we can't read memory (e.g., not connected to target),
                # just show the address
                return f"@{addr:#010x} ({value_type_str})"

        except Exception as e:
            return f"<error: {e}>"


class RegistralArrayPrinter:
    """Pretty printer for gba::registral<T[N]> array types"""

    def __init__(self, val):
        self.val = val
        self.type = val.type.strip_typedefs()

    def to_string(self):
        try:
            addr = int(self.val['m_address'])

            type_name = str(self.type)

            # Extract array info
            match = re.search(r'gba::registral<(.+)\[(\d+)\]', type_name)
            if match:
                elem_type_str = match.group(1).strip()
                size = int(match.group(2))

                # Try to read the first few elements
                try:
                    # Look up the element type
                    elem_type = gdb.lookup_type(elem_type_str)
                    ptr_type = elem_type.pointer()
                    ptr = gdb.Value(addr).cast(ptr_type)

                    # Read first few elements (up to 4)
                    preview_count = min(size, 4)
                    elements = []
                    for i in range(preview_count):
                        elem = ptr[i]
                        elements.append(str(int(elem)))

                    preview = ', '.join(elements)
                    if size > preview_count:
                        preview += ', ...'

                    return f"@{addr:#010x}[{size}] = {{{preview}}}"
                except:
                    pass

                return f"@{addr:#010x}[{size}] ({elem_type_str})"

            return f"@{addr:#010x}"

        except Exception as e:
            return f"<error: {e}>"


def registral_lookup(val):
    """Lookup function for registral types."""
    type_name = str(val.type.strip_typedefs())

    if 'gba::registral<' in type_name:
        # Check if it's an array type
        if re.search(r'gba::registral<.+\[\d+\]', type_name):
            return RegistralArrayPrinter(val)
        return RegistralPrinter(val)

    return None


def register_printers(objfile=None):
    """Register the registral pretty printers."""
    if objfile is None:
        objfile = gdb

    objfile.pretty_printers.append(registral_lookup)


# Auto-register when sourced
register_printers()
print("Loaded stdgba registral pretty printer")
