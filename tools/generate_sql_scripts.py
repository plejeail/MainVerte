"""Compact SQL resources without changing quoted values or identifiers."""

import argparse
from pathlib import Path
import re
import sys


def compact_sql(source):
    result = []
    index = 0
    pending_space = False
    while index < len(source):
        char = source[index]
        if source.startswith("--", index):
            end = source.find("\n", index)
            index = len(source) if end == -1 else end
            pending_space = True
            continue
        if char.isspace():
            pending_space = True
            index += 1
            continue
        if pending_space and result and result[-1] != ";":
            result.append(" ")
        pending_space = False
        if char in "'\"`[":
            closing = "]" if char == "[" else char
            start = index
            index += 1
            while index < len(source):
                if source[index] == closing:
                    index += 1
                    if closing != "]" and index < len(source) and source[index] == closing:
                        index += 1
                        continue
                    break
                index += 1
            result.append(source[start:index])
        elif source.startswith("/*", index):
            # Keep block comments intact, including any embedded line markers.
            end = source.find("*/", index + 2)
            end = len(source) if end == -1 else end + 2
            result.append(source[index:end])
            index = end
        else:
            result.append(char)
            index += 1
    return "".join(result)


def generate(sources, destination, pattern=r"^([0-9]+)_.+\.sql$"):
    entries = []
    for source in sorted(sources):
        match = re.fullmatch(pattern, source.name)
        if not match:
            continue
        if int(match[1]) != len(entries):
            raise ValueError(f"Invalid SQL order: expected {len(entries)}, got {source.name}")
        sql = compact_sql(source.read_text(encoding="utf-8"))
        if ')MV_SQL"' in sql:
            raise ValueError(f"Invalid SQL delimiter in {source}")
        entries.append(f'    R"MV_SQL({sql})MV_SQL",\n')
    header = """#pragma once

#include <array>
#include <string_view>

namespace mv::sql {

"""
    header += f"inline constexpr std::array<std::string_view, {len(entries)}> scripts{{{{\n"
    header += "".join(entries)
    header += "}};\n\n} //  namespace mv::sql\n"
    data = header.encode("utf-8")
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists() or destination.read_bytes() != data:
        temporary = destination.with_suffix(".hpp.tmp")
        temporary.write_bytes(data)
        temporary.replace(destination)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--pattern", default=r"^([0-9]+)_.+\.sql$")
    parser.add_argument("sources", type=Path, nargs="*")
    args = parser.parse_args()
    try:
        generate(args.sources, args.output, args.pattern)
    except (ValueError, OSError, UnicodeError) as error:
        print(f"SQL generation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
