# AI Generated file! TODO: Replace with human code
"""
GDB pretty printer for gba::format types

Displays format strings, generators, and argument binders in readable form.

Example output:
    (gba::format::fixed_string<15>) "HP: {hp}/{max}"
    (gba::format::compiled_format<...>) format("HP: {hp}/{max}")
    (gba::format::arg_binder<...>) arg("hp")
"""

import gdb
import re


def extract_string_from_type(type_name):
    """Extract the format/arg string from template parameter in type name."""
    # Match patterns like {"..."} in the type name
    match = re.search(r'\{"([^"]*)"', type_name)
    if match:
        return match.group(1)
    return None


class FixedStringPrinter:
    """Pretty printer for gba::format::fixed_string<N>"""

    def __init__(self, val):
        self.val = val
        self.type = val.type.strip_typedefs()

    def to_string(self):
        try:
            # First try to get from type name (for NTTP instances)
            type_name = str(self.type)
            extracted = extract_string_from_type(type_name)
            if extracted is not None:
                return f'"{extracted}"'

            # Try to read from the data array member
            try:
                # Get the array field
                data_field = self.val['data']
                chars = []
                i = 0
                while i < 256:  # reasonable limit
                    try:
                        c = int(data_field[i])
                        if c == 0:
                            break
                        chars.append(chr(c))
                        i += 1
                    except:
                        break
                if chars:
                    return f'"{("".join(chars))}"'
            except:
                pass

            # Fallback: extract size from type and show placeholder
            match = re.search(r'fixed_string<(\d+)>', type_name)
            if match:
                size = int(match.group(1))
                return f'"<string[{size}]>"'

            return '"<...>"'

        except Exception as e:
            return f"<error: {e}>"


class CompiledFormatPrinter:
    """Pretty printer for gba::format::compiled_format<Fmt>

    This is a stateless type - the format string is in the type itself.
    """

    def __init__(self, val):
        self.type = val.type.strip_typedefs()

    def to_string(self):
        try:
            type_name = str(self.type)
            fmt_str = extract_string_from_type(type_name)
            if fmt_str is not None:
                return f'format("{fmt_str}")'
            return 'format(<...>)'
        except Exception as e:
            return f"<error: {e}>"


class ArgBinderPrinter:
    """Pretty printer for gba::format::arg_binder<Name>

    This is a stateless type - the arg name is in the type itself.
    """

    def __init__(self, val):
        self.type = val.type.strip_typedefs()

    def to_string(self):
        try:
            type_name = str(self.type)
            name = extract_string_from_type(type_name)
            if name is not None:
                return f'arg("{name}")'
            return 'arg(<...>)'
        except Exception as e:
            return f"<error: {e}>"


class BoundArgPrinter:
    """Pretty printer for gba::format::bound_arg<Hash, T>"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            stored = self.val['stored']
            return f'= {stored}'
        except Exception as e:
            return f"<error: {e}>"


class FormatGeneratorPrinter:
    """Pretty printer for gba::format::format_generator<Fmt, ArgPack>"""

    def __init__(self, val):
        self.val = val
        self.type = val.type.strip_typedefs()

    def to_string(self):
        try:
            segment_idx = int(self.val['segment_idx'])
            char_idx = int(self.val['char_idx'])

            type_name = str(self.type)
            fmt_str = extract_string_from_type(type_name)
            if fmt_str is None:
                fmt_str = "..."

            return f'generator("{fmt_str}") [seg={segment_idx}, char={char_idx}]'

        except Exception as e:
            return f"<error: {e}>"


def format_lookup(val):
    """Lookup function for format types."""
    type_name = str(val.type.strip_typedefs())

    if 'gba::format::fixed_string<' in type_name:
        return FixedStringPrinter(val)

    if 'gba::format::compiled_format<' in type_name:
        return CompiledFormatPrinter(val)

    if 'gba::format::arg_binder<' in type_name:
        return ArgBinderPrinter(val)

    if 'gba::format::bound_arg<' in type_name:
        return BoundArgPrinter(val)

    if 'gba::format::format_generator<' in type_name:
        return FormatGeneratorPrinter(val)

    return None


def register_printers(objfile=None):
    """Register the format pretty printers."""
    if objfile is None:
        objfile = gdb

    objfile.pretty_printers.append(format_lookup)


# Auto-register when sourced
register_printers()
print("Loaded stdgba format pretty printer")
