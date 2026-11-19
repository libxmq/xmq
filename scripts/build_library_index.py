#!/usr/bin/env python3
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Make the versioned copies of the library grammars and stylesheets and write the library
# index, index.xmq, used by xmq list-library, describe-library, check-library and
# refresh-library.
#
# The first comment of an ixml grammar or xslq stylesheet has its version, a major number
# and a minor letter (a=1 b=2 ... z=26), eg v2c is version 2.3.0. There is no patch number,
# a patch must be compatible and does not need a new version.
#
#   { v2c Create a table/tr/td from a csv file (comma separated values). }
#   // v1a Render a table/tr/td document as a web page.
#
# data/csv.ixml with version 2c is copied to data/csv_v2c.ixml (this version, never changed
# again) and data/csv_v2.ixml (the latest 2.x). Then ixml:data/csv is always the latest,
# ixml:data/csv_v2 the latest compatible with 2 and ixml:data/csv_v2c exactly 2c. The copies
# are kept in the library forever. A grammar that has changed without a new version is an
# error, since its copy for the version is already published.
#
# The index lists every file (also the copies and other xml/xmq files) with its size,
# sha256 and modification time (UTC) and for the versioned ones the version and the first
# comment, without the version, as description:
#
#   library(created = 2026-11-19T10:08:55Z)
#   {
#       file(path        = data/csv.ixml
#            size        = 293
#            sha256      = 9a2b...
#            mtime       = 2026-10-20T10:08:55Z
#            version     = 2c
#            description = 'Create a table/tr/td from a csv file (comma separated values).')
#       ...
#   }
#
# Publish the library directory with index.xmq on https://libxmq.org/library/ and keep the
# modification times of the files (eg rsync -t), xmq compares them with the time of the
# local files to decide which is newer.
#
# Usage: scripts/build_library_index.py [library_dir]
# The default is library, the index is written to library/index.xmq.

import datetime
import filecmp
import hashlib
import os
import re
import shutil
import sys
import textwrap

VERSIONED = ('.ixml', '.xslq')
INDEXED = ('.ixml', '.xslq', '.xml', '.xmq')
VERSION = re.compile(r'(?<![\w.])v([0-9]+)([a-z])(?![\w.])')
COPY = re.compile(r'^(.*)_v([0-9]+)([a-z]?)$')


def first_comment(text, ext):
    """The first comment of an ixml grammar { ... } or xslq stylesheet // ... or /* ... */,
    or None if the file does not start with a comment."""
    s = text.lstrip()
    if ext == '.ixml':
        if not s.startswith('{'):
            return None
        depth = 0
        for i, c in enumerate(s):
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0:
                    return s[1:i]
        return None
    if s.startswith('/*'):
        end = s.find('*/')
        return s[2:end] if end > 0 else None
    if s.startswith('//'):
        lines = []
        for line in s.split('\n'):
            line = line.strip()
            if not line.startswith('//'):
                break
            line = line[2:]
            lines.append(line[1:] if line.startswith(' ') else line)
        return '\n'.join(lines)
    return None


def version_and_description(file, ext):
    """The version (major, letter) and the description from the first comment, or None."""
    with open(file, encoding='utf-8') as f:
        comment = first_comment(f.read(), ext)
    if comment is None:
        return None
    m = VERSION.search(comment)
    if not m:
        return None
    comment = comment[:m.start()] + comment[m.end():].lstrip(' ')
    lines = comment.split('\n')
    first = lines[0].strip()
    rest = textwrap.dedent('\n'.join(lines[1:])).split('\n')
    lines = [first] + [line.rstrip() for line in rest]
    description = '\n'.join(lines).strip('\n')
    # A first line that was empty, eg {\n   v1a Text...
    description = textwrap.dedent(description).strip()
    return (int(m.group(1)), m.group(2)), description


def library_files(library):
    """The paths, relative to the library, of the files that are installed and published."""
    found = []
    for root, dirs, files in os.walk(library):
        dirs[:] = sorted(d for d in dirs if not d.startswith('test-') and d != 'c-tests')
        for name in files:
            path = os.path.relpath(os.path.join(root, name), library).replace(os.sep, '/')
            if name.endswith(INDEXED) and path != 'index.xmq':
                found.append(path)
    return sorted(found)


def split_copy(path):
    """For data/csv_v2c.ixml (data/csv.ixml, 2, 'c'), for data/csv_v2.ixml
    (data/csv.ixml, 2, ''), else None."""
    stem, ext = os.path.splitext(path)
    m = COPY.match(stem)
    if ext not in VERSIONED or not m:
        return None
    return m.group(1) + ext, int(m.group(2)), m.group(3)


def make_copies(library, paths, errors):
    """Copy every versioned grammar/stylesheet to its exact and major version names."""
    copies = {}
    for p in paths:
        c = split_copy(p)
        if c and c[2]:
            copies.setdefault(c[0], []).append((c[1], c[2]))
    for p in paths:
        stem, ext = os.path.splitext(p)
        if ext not in VERSIONED or split_copy(p):
            continue
        file = os.path.join(library, p)
        vd = version_and_description(file, ext)
        if vd is None:
            print(f'Warning: {p} has no version (eg v1a) in its first comment, no versioned copies',
                  file=sys.stderr)
            continue
        (major, letter), _ = vd
        name = f'{major}{letter}'
        newer = [f'{M}{L}' for M, L in copies.get(p, []) if (M, L) > (major, letter)]
        if newer:
            errors.append(f'{p} has version {name}, but {", ".join(sorted(newer))} '
                          f'is already in the library, the version must increase')
            continue
        exact = os.path.join(library, f'{stem}_v{name}{ext}')
        if os.path.exists(exact):
            if not filecmp.cmp(file, exact, shallow=False):
                nxt = chr(ord(letter) + 1) if letter < 'z' else 'z'
                errors.append(f'{p} differs from {stem}_v{name}{ext} but has the same version {name}, '
                              f'a published version never changes, give it a new version (eg v{major}{nxt} '
                              f'or v{major + 1}a)')
                continue
        else:
            shutil.copy2(file, exact)
            print(f'Added {library}/{stem}_v{name}{ext}')
        latest = os.path.join(library, f'{stem}_v{major}{ext}')
        if not os.path.exists(latest) or not filecmp.cmp(file, latest, shallow=False):
            shutil.copy2(file, latest)
            print(f'Updated {library}/{stem}_v{major}{ext}')


def quote(value, column):
    """value as an xmq attribute value that starts at column."""
    if re.match(r'^[A-Za-z0-9_.:+-][A-Za-z0-9_.:/+-]*$', value) and '//' not in value and '/*' not in value:
        return value
    if "'" not in value:
        q = "'"
    elif '"' not in value:
        q = '"'
    else:
        q = "'''"
        while q in value:
            q += "'"
        if value.startswith("'") or value.endswith("'"):
            raise ValueError(f'Cannot quote {value!r}')
    indent = ' ' * (column + len(q))
    lines = value.split('\n')
    return q + '\n'.join([lines[0]] + [indent + line if line else '' for line in lines[1:]]) + q


def utc(t):
    return datetime.datetime.fromtimestamp(t, datetime.timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')


def sha256(file):
    h = hashlib.sha256()
    with open(file, 'rb') as f:
        for block in iter(lambda: f.read(65536), b''):
            h.update(block)
    return h.hexdigest()


def main():
    library = sys.argv[1] if len(sys.argv) > 1 else 'library'
    if not os.path.isdir(library):
        print(f'No library directory {library}', file=sys.stderr)
        sys.exit(1)

    errors = []
    make_copies(library, library_files(library), errors)
    if errors:
        for e in errors:
            print(f'Error: {e}', file=sys.stderr)
        sys.exit(1)

    out = ['// The library index, made by scripts/build_library_index.py, see xmq help list-library.',
           f'library(created = {utc(datetime.datetime.now(datetime.timezone.utc).timestamp())})',
           '{']
    prefix = '    file('
    keys = ['path', 'size', 'sha256', 'mtime', 'version', 'description']
    width = max(len(k) for k in keys)
    for p in library_files(library):
        file = os.path.join(library, p)
        attrs = [('path', p), ('size', str(os.path.getsize(file))), ('sha256', sha256(file)),
                 ('mtime', utc(os.path.getmtime(file)))]
        ext = os.path.splitext(p)[1]
        vd = version_and_description(file, ext) if ext in VERSIONED else None
        if vd:
            (major, letter), description = vd
            attrs.append(('version', f'{major}{letter}'))
            if description:
                attrs.append(('description', description))
        lines = []
        for i, (k, v) in enumerate(attrs):
            start = prefix if i == 0 else ' ' * len(prefix)
            lines.append(f'{start}{k.ljust(width)} = {quote(v, len(prefix) + width + 3)}')
        out.append('\n'.join(lines) + ')')
    out.append('}')

    index = os.path.join(library, 'index.xmq')
    with open(index + '.tmp', 'w', encoding='utf-8') as f:
        f.write('\n'.join(out) + '\n')
    os.replace(index + '.tmp', index)
    print(f'Wrote {index}')


if __name__ == '__main__':
    main()
