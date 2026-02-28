"""
GDB pretty printer for gba::video types

Displays colors and sprite objects in readable form.

Example output:
    (gba::color) rgb(31, 15, 0)
    (gba::object) obj(x=120, y=80, tile=0)
"""

import gdb
import re


class ColorPrinter:
    """Pretty printer for gba::color"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            # color is a bitfield struct: red:5, green:5, blue:5
            # Read the underlying short value
            raw = int(self.val.cast(gdb.lookup_type('unsigned short')))

            red = raw & 0x1F
            green = (raw >> 5) & 0x1F
            blue = (raw >> 10) & 0x1F

            return f"rgb({red}, {green}, {blue})"

        except Exception as e:
            return f"<error: {e}>"


class ObjectPrinter:
    """Pretty printer for gba::object (sprite)"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            # Read as array of shorts for reliability
            raw = self.val.cast(gdb.lookup_type('unsigned short').array(2))

            attr0 = int(raw[0])
            attr1 = int(raw[1])
            attr2 = int(raw[2])

            y = attr0 & 0xFF
            x = attr1 & 0x1FF
            tile = attr2 & 0x3FF

            disabled = (attr0 >> 9) & 1
            flip_x = (attr1 >> 12) & 1
            flip_y = (attr1 >> 13) & 1

            parts = [f"x={x}", f"y={y}", f"tile={tile}"]

            if disabled:
                parts.append("disabled")
            if flip_x:
                parts.append("flip_x")
            if flip_y:
                parts.append("flip_y")

            return f"obj({', '.join(parts)})"

        except Exception as e:
            return f"<error: {e}>"


class ModePrinter:
    """Pretty printer for gba::mode enum"""

    MODES = {0: 'normal', 1: 'alpha', 2: 'window', 3: 'forbidden'}

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            raw = int(self.val)
            return self.MODES.get(raw, f"mode({raw})")
        except Exception as e:
            return f"<error: {e}>"


class DepthPrinter:
    """Pretty printer for gba::depth enum"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            raw = int(self.val)
            return "8bpp" if raw else "4bpp"
        except Exception as e:
            return f"<error: {e}>"


class ShapePrinter:
    """Pretty printer for gba::shape enum"""

    SHAPES = {0: 'square', 1: 'wide', 2: 'tall'}

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            raw = int(self.val)
            return self.SHAPES.get(raw, f"shape({raw})")
        except Exception as e:
            return f"<error: {e}>"


def video_lookup(val):
    """Lookup function for video types."""
    type_name = str(val.type.strip_typedefs())

    if type_name == 'gba::color':
        return ColorPrinter(val)

    if type_name == 'gba::object':
        return ObjectPrinter(val)

    if type_name == 'gba::mode':
        return ModePrinter(val)

    if type_name == 'gba::depth':
        return DepthPrinter(val)

    if type_name == 'gba::shape':
        return ShapePrinter(val)

    return None


def register_printers(objfile=None):
    """Register the video pretty printers."""
    if objfile is None:
        objfile = gdb

    objfile.pretty_printers.append(video_lookup)


# Auto-register when sourced
register_printers()
print("Loaded stdgba video pretty printer")
