#!/usr/bin/env python3
# libxmq - Copyright 2026 Fredrik Öhrström (spdx: MIT)
#
# Compare the result of an external ixml engine (XMQ_IXML_ENGINE, eg markup-blitz) with the
# expected result of xmq's own engine, by meaning instead of exact text:
#
#   - A successful parse must give the same tree.
#   - An ambiguous parse must be ambiguous (ixml:state contains ambiguous) for both. If xmq
#     lists the alternatives (an AMBIGUOUS root), the tree of the engine must be one of them.
#   - A failed parse (ixml:state="failed") must fail for both. The engines report the failure
#     in their own words, but where xmq gives the line and column of the failure, the engine
#     must report the same position ("at line N, column M"), and where xmq gives a dynamic
#     error (eg D02), the engine must report the same error code (ixml:error-code).
#
# Usage: tests/compare_ixml_engine.py <xmq binary> <expected file> <actual file>
# The files are xmq, xml or json, anything xmq reads. Exits 0 if they are equivalent,
# otherwise 1 with the reason on stdout.

import re
import subprocess
import sys
import xml.etree.ElementTree as ET

IXML_NS = '{http://invisiblexml.org/NS}'


def load(xmq, path):
    # The exit code is not checked, xmq exits with 1 for a document of a failed ixml parse.
    out = subprocess.run([xmq, '--trim=none', path, 'to-xml', '--omit-decl'], capture_output=True)
    if not out.stdout.strip():
        return None
    try:
        return ET.fromstring(out.stdout)
    except ET.ParseError:
        return None


def state(e):
    return e.get(IXML_NS + 'state', '')


def canonical(e):
    """The element with its attributes (without the ixml ones) and its content, as a tuple."""
    attrs = tuple(sorted((k, v) for k, v in e.attrib.items() if not k.startswith(IXML_NS)))
    children = tuple(canonical(c) for c in e)
    tails = tuple((c.tail or '') for c in e)
    return (e.tag, attrs, e.text or '', children, tails)


def failure_position(e):
    """The line and column of a failure, from xmq's line/column elements or from the
    message of another engine: ... at line N, column M."""
    line = e.find('line')
    column = e.find('column')
    if line is not None and column is not None:
        return (line.text.strip(), column.text.strip())
    text = ''.join(e.itertext())
    m = re.search(r'at line (\d+), column (\d+)', text)
    if m:
        return (m.group(1), m.group(2))
    return None


def error_code(e):
    code = e.get(IXML_NS + 'error-code')
    if code:
        return code
    m = re.search(r'\b(D\d\d)\b', ''.join(e.itertext()))
    return m.group(1) if m else None


def compare(expected, actual):
    if expected is None:
        return 'cannot read the expected result'
    if actual is None:
        return 'the engine gave no result (did it crash?)'

    if 'failed' in state(expected):
        if 'failed' not in state(actual):
            return 'xmq fails but the engine does not'
        exp_code = error_code(expected)
        if exp_code and exp_code != error_code(actual):
            return f'xmq fails with {exp_code} but the engine with {error_code(actual)}'
        exp_pos = failure_position(expected)
        act_pos = failure_position(actual)
        if exp_code is None and exp_pos and act_pos and exp_pos != act_pos:
            return f'xmq fails at line {exp_pos[0]} column {exp_pos[1]} but the engine at line {act_pos[0]} column {act_pos[1]}'
        return None
    if 'failed' in state(actual):
        return 'the engine fails but xmq does not'

    if 'ambiguous' in state(expected):
        if 'ambiguous' not in state(actual):
            return 'xmq is ambiguous but the engine is not'
        if expected.tag == 'AMBIGUOUS':
            if canonical(actual) not in [canonical(alt) for alt in expected]:
                return 'the tree of the engine is not one of the ambiguous trees of xmq'
        return None
    if 'ambiguous' in state(actual):
        return 'the engine is ambiguous but xmq is not'

    if canonical(expected) != canonical(actual):
        return 'the trees differ'
    return None


def main():
    if len(sys.argv) != 4:
        print('Usage: tests/compare_ixml_engine.py <xmq binary> <expected file> <actual file>')
        sys.exit(2)
    xmq, exp_path, act_path = sys.argv[1:]
    reason = compare(load(xmq, exp_path), load(xmq, act_path))
    if reason:
        print(reason)
        sys.exit(1)
    sys.exit(0)


if __name__ == '__main__':
    main()
