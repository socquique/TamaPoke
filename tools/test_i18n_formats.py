#!/usr/bin/env python3
"""Consistency checks for i18n.cpp's translation tables.

Several strings in STRINGS[][] are used as snprintf() format strings
(e.g. T(S_FAREWELL_BTN) in a snprintf() call). Since they're not
literals at the call site, the compiler can't validate them with
-Wformat: a translation with a wrong, missing or extra %-specifier is
undefined behavior that only shows up in the affected language. This
script parses the four [language][id] tables straight out of the .cpp
source (no build step needed) and checks:

  - every language row has the same number of entries as every other
    row, and that count matches the number of enumerators in StrId
    (STR_COUNT) resp. MED_COUNT for the medal tables. STRINGS is
    declared with both array dimensions fixed
    ([LANG_COUNT][STR_COUNT]), so a row with too FEW strings does not
    fail to compile -- the remaining slots are silently
    zero-initialized (null pointers), which crashes or reads garbage
    at runtime the first time that StrId is used in the affected
    language. Too many *is* a compile error, so this mainly guards
    the silent case, but checks the count outright either way.
  - for every StrId, the sequence of %-format specifiers is identical
    across all 6 languages.

  python3 tools/test_i18n_formats.py
"""
import os
import re
import sys

HERE = os.path.dirname(__file__)
I18N_CPP = os.path.join(HERE, '..', 'i18n.cpp')

LANGS = ['ES', 'EN', 'FR', 'DE', 'IT', 'PT']
STRING_RE = re.compile(r'"(?:[^"\\]|\\.)*"')
SPEC_RE = re.compile(
    r'%[-+ #0]*[\d.]*(?:l|h|ll|hh)?[diouxXeEfFgGaAcspn%]')


def extract_balanced_blocks(text, count):
    """Return the `count` top-level {...} blocks found in `text`.

    Assumes no nested braces inside a block (true for STRINGS/MED_*
    here: each block is a flat, comma-separated list of string
    literals).
    """
    blocks = []
    depth = 0
    start = None
    for i, ch in enumerate(text):
        if ch == '{':
            if depth == 0:
                start = i
            depth += 1
        elif ch == '}':
            depth -= 1
            if depth == 0:
                blocks.append(text[start + 1:i])
                if len(blocks) == count:
                    return blocks
    raise ValueError(
        f'expected {count} top-level blocks, found {len(blocks)}')


def extract_table(source, var_name, count):
    """Find the `var_name[...][...] = { ... };` table and return its
    `count` per-language string lists."""
    m = re.search(
        rf'\b{re.escape(var_name)}\s*\[[^;{{]*\]\s*=\s*\{{', source)
    if not m:
        raise ValueError(f'table {var_name} not found')
    # body starts at the '{' the regex matched; extract_balanced_blocks
    # wants to see that opening brace as depth 0 -> 1, so start there.
    body_start = m.end() - 1
    depth = 0
    for i in range(body_start, len(source)):
        if source[i] == '{':
            depth += 1
        elif source[i] == '}':
            depth -= 1
            if depth == 0:
                body = source[body_start:i + 1]
                break
    else:
        raise ValueError(f'unterminated table {var_name}')
    lang_blocks = extract_balanced_blocks(body[1:-1], count)
    return [STRING_RE.findall(block) for block in lang_blocks]


def check_table(source, var_name, expected_len, label_for):
    """Validate one [LANG_COUNT][N] table. Returns error strings."""
    errors = []
    rows = extract_table(source, var_name, len(LANGS))
    for lang, row in zip(LANGS, rows):
        if len(row) != expected_len:
            errors.append(
                f'{var_name}[{lang}] has {len(row)} entries, '
                f'expected {expected_len}')
    if errors:
        return errors  # zip() below assumes equal-length rows

    for idx, texts in enumerate(zip(*rows)):
        specs = {tuple(SPEC_RE.findall(t)) for t in texts}
        if len(specs) != 1:
            per_lang = {
                lang: SPEC_RE.findall(t) for lang, t in zip(LANGS, texts)
            }
            errors.append(
                f'{var_name}[*][{label_for(idx)}] specifiers differ '
                f'across languages: {per_lang}')
    return errors


def count_enumerators(header_path, enum_name, terminator):
    """Count comma-separated enumerators before `terminator`."""
    src = open(header_path, encoding='utf-8').read()
    m = re.search(
        rf'enum\s+{enum_name}\s*(?::\s*\w+\s*)?\{{(.*?)\}};', src, re.S)
    if not m:
        raise ValueError(f'enum {enum_name} not found in {header_path}')
    # strip line comments first: they sit between enumerators, not just
    # at line ends, so a plain comma-split would swallow them into names
    body = re.sub(r'//[^\n]*', '', m.group(1))
    names = [
        n.strip().split('=')[0].strip()
        for n in body.split(',') if n.strip()
    ]
    if terminator not in names:
        raise ValueError(f'{terminator} not found in enum {enum_name}')
    return names.index(terminator)


def main():
    source = open(I18N_CPP, encoding='utf-8').read()
    i18n_hpp = os.path.join(HERE, '..', 'i18n.h')

    str_count = count_enumerators(i18n_hpp, 'StrId', 'STR_COUNT')
    med_count = 8  # MED_COUNT, #define'd in include/pet.hpp

    errors = []
    errors += check_table(
        source, 'STRINGS', str_count, lambda i: f'StrId {i}')
    errors += check_table(
        source, 'MED_NAME', med_count, lambda i: f'medal {i}')
    errors += check_table(
        source, 'MED_LBL', med_count, lambda i: f'medal {i}')
    errors += check_table(
        source, 'MED_DSC', med_count, lambda i: f'medal {i}')

    if errors:
        print(f'{len(errors)} i18n consistency error(s):')
        for e in errors:
            print(f'  - {e}')
        return 1

    print(
        f'OK: STRINGS ({str_count} ids) and the 3 medal tables '
        f'({med_count} medals) are consistent across all '
        f'{len(LANGS)} languages.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
