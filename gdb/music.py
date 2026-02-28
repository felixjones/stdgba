"""
GDB pretty printer for gba::music types

Displays music notes, patterns, tokens, and BPM values in readable form.

Example output:
    (gba::music::note) C4
    (gba::music::bpm_value) 120 BPM
    (gba::music::token_type) note
    (gba::music::ast_type) sequence
"""

import gdb
import re

# MIDI note number to name mapping
NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']

# Drum sound names (values 200+)
DRUM_NAMES = {
    200: 'bd',   # Bass drum
    201: 'sd',   # Snare drum
    202: 'hh',   # Hi-hat closed
    203: 'oh',   # Open hi-hat
    204: 'cp',   # Clap
    205: 'lt',   # Low tom
    206: 'mt',   # Mid tom
    207: 'ht',   # High tom
    208: 'cb',   # Cowbell
    209: 'cr',   # Crash cymbal
    210: 'rd',   # Ride cymbal
    211: 'rs',   # Rimshot
}

TOKEN_TYPES = {
    0: 'end',
    1: 'note',
    2: 'rest',
    3: 'number',
    4: '[',
    5: ']',
    6: '<',
    7: '>',
    8: ',',
    9: '*',
    10: '/',
    11: '@',
    12: '!',
    13: '%',
    14: '(',
    15: ')',
    16: 'error',
}

AST_TYPES = {
    0: 'atom',
    1: 'sequence',
    2: 'subdivision',
    3: 'stack',
    4: 'timeline',
    5: 'fast',
    6: 'slow',
    7: 'weight',
    8: 'replicate',
    9: 'euclidean',
    10: 'timeline_fast',
    11: 'timeline_slow',
}


class NotePrinter:
    """Pretty printer for gba::music::note"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            raw = int(self.val)

            # Rest
            if raw == 255:
                return "rest (~)"

            # Drum sounds
            if raw in DRUM_NAMES:
                return f"{DRUM_NAMES[raw]} (drum)"

            # MIDI note
            if 0 <= raw < 128:
                octave = (raw // 12) - 1
                note_idx = raw % 12
                note_name = NOTE_NAMES[note_idx]
                return f"{note_name}{octave}"

            return f"note({raw})"

        except Exception as e:
            return f"<error: {e}>"


class BpmPrinter:
    """Pretty printer for gba::music::bpm_value"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            # bpm_value stores 'value' member (std::uint16_t)
            # Try direct member access first
            try:
                bpm_val = int(self.val['value'])
                return f"{bpm_val} BPM"
            except:
                pass

            # Try casting to underlying type
            try:
                bpm_val = int(self.val.cast(gdb.lookup_type('unsigned short')))
                return f"{bpm_val} BPM"
            except:
                pass

            # Last resort: try to read as int
            bpm_val = int(self.val)
            return f"{bpm_val} BPM"
        except Exception as e:
            return f"<error: {e}>"


class TokenTypePrinter:
    """Pretty printer for gba::music::token_type"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            raw = int(self.val)
            return TOKEN_TYPES.get(raw, f"token_type({raw})")
        except Exception as e:
            return f"<error: {e}>"


class AstTypePrinter:
    """Pretty printer for gba::music::ast_type"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            raw = int(self.val)
            return AST_TYPES.get(raw, f"ast_type({raw})")
        except Exception as e:
            return f"<error: {e}>"


class TokenPrinter:
    """Pretty printer for gba::music::token"""

    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            token_type = int(self.val['type'])
            type_name = TOKEN_TYPES.get(token_type, '?')

            if token_type == 1:  # note
                note_val = int(self.val['note_value'])
                if 0 <= note_val < 128:
                    octave = (note_val // 12) - 1
                    note_name = NOTE_NAMES[note_val % 12]
                    return f"token({type_name}: {note_name}{octave})"
                return f"token({type_name}: {note_val})"
            elif token_type == 3:  # number
                num = int(self.val['number_value'])
                return f"token({type_name}: {num})"
            else:
                return f"token({type_name})"
        except Exception as e:
            return f"<error: {e}>"


class PatternPrinter:
    """Pretty printer for gba::music::pattern types"""

    def __init__(self, val):
        self.val = val
        self.type = val.type.strip_typedefs()

    def to_string(self):
        try:
            type_name = str(self.type)

            # Extract pattern string from template
            match = re.search(r'\{"([^"]+)"\}', type_name)
            if match:
                pattern_str = match.group(1)

                # Determine channel
                if 'sq1' in type_name.lower():
                    return f'sq1("{pattern_str}")'
                elif 'sq2' in type_name.lower():
                    return f'sq2("{pattern_str}")'
                elif 'wav' in type_name.lower():
                    return f'wav("{pattern_str}")'
                elif 'noise' in type_name.lower():
                    return f'noise("{pattern_str}")'
                else:
                    return f'pattern("{pattern_str}")'

            return 'pattern(<...>)'

        except Exception as e:
            return f"<error: {e}>"


def music_lookup(val):
    """Lookup function for music types."""
    type_name = str(val.type.strip_typedefs())

    if type_name == 'gba::music::note':
        return NotePrinter(val)

    if 'bpm_value' in type_name:
        return BpmPrinter(val)

    if type_name == 'gba::music::token_type':
        return TokenTypePrinter(val)

    if type_name == 'gba::music::ast_type':
        return AstTypePrinter(val)

    if type_name == 'gba::music::token':
        return TokenPrinter(val)

    if 'gba::music::pattern<' in type_name or 'gba::music::parsed_pattern<' in type_name:
        return PatternPrinter(val)

    return None


def register_printers(objfile=None):
    """Register the music pretty printers."""
    if objfile is None:
        objfile = gdb

    objfile.pretty_printers.append(music_lookup)


# Auto-register when sourced
register_printers()
print("Loaded stdgba music pretty printer")
