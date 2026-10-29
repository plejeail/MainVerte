"""Validate UTF-8 CSV dictionaries and generate embedded C++ translations."""

import argparse
import csv
from pathlib import Path
import re
import sys


CPP_KEYWORDS = set("""
alignas alignof and and_eq asm auto bitand bitor bool break case catch char
char8_t char16_t char32_t class compl concept const consteval constexpr constinit
const_cast continue co_await co_return co_yield decltype default delete do double
dynamic_cast else enum explicit export extern false float for friend goto if
inline int long mutable namespace new noexcept not not_eq nullptr operator or
or_eq private protected public register reinterpret_cast requires return short
signed sizeof static static_assert static_cast struct switch template this
thread_local throw true try typedef typeid typename union unsigned using virtual
void volatile wchar_t while xor xor_eq
""".split())


class DictionaryError(ValueError):
    pass


def read_dictionary(path):
    entries = {}
    with path.open(encoding="utf-8-sig", newline="") as source:
        reader = csv.reader(source, strict=True)
        try:
            for row in reader:
                line = reader.line_num
                if len(row) != 2:
                    raise DictionaryError(f"{path}:{line}: expected two CSV columns")
                key, text = row
                if (not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*", key)
                        or "__" in key or key in CPP_KEYWORDS or key == "Count"):
                    raise DictionaryError(f"{path}:{line}: invalid C++ ID {key!r}")
                if key in entries:
                    raise DictionaryError(f"{path}:{line}: duplicate ID {key!r}")
                entries[key] = text
        except csv.Error as error:
            raise DictionaryError(f"{path}:{reader.line_num}: {error}") from error
    if not entries:
        raise DictionaryError(f"{path}: dictionary is empty")
    return entries


def cpp_string(text):
    # ASCII-only C++ source; QStringLiteral converts Unicode escapes to UTF-16
    # at compile time, including surrogate pairs for characters above U+FFFF.
    escaped = "".join(
        char if 32 <= ord(char) < 127 and char not in ('"', "\\")
        else f"\\{ord(char):03o}" if ord(char) < 128
        else f"\\u{ord(char):04x}" if ord(char) <= 0xffff
        else f"\\U{ord(char):08x}"
        for char in text
    )
    return f'QStringLiteral("{escaped}")'


def generate(paths, output_dir, output_name="generated_translations.hpp"):
    dictionaries = []
    languages = set()
    reference = None
    for path in paths:
        match = re.fullmatch(r"MainVerte_([a-z]{2,3}(?:_[A-Za-z0-9]+)*)\.dict", path.name)
        if not match:
            raise DictionaryError(f"{path}: expected MainVerte_<language>.dict")
        language = match[1]
        if language in languages:
            raise DictionaryError(f"{path}: duplicate language {language!r}")
        languages.add(language)
        try:
            entries = read_dictionary(path)
        except UnicodeError as error:
            raise DictionaryError(f"{path}: invalid UTF-8: {error}") from error
        if reference is None:
            reference = list(entries)
        else:
            missing = sorted(set(reference) - entries.keys())
            unknown = sorted(entries.keys() - set(reference))
            if missing or unknown:
                details = []
                if missing:
                    details.append("missing IDs: " + ", ".join(missing))
                if unknown:
                    details.append("unknown IDs: " + ", ".join(unknown))
                raise DictionaryError(f"{path}: " + "; ".join(details))
        dictionaries.append((language, entries))

    dictionary = """// Generated file. Do not edit.
#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <span>
#include <string_view>

#include <QString>
#include <QObject>
#include <QtQmlIntegration/qqmlintegration.h>

namespace mv::i18n {
Q_NAMESPACE
QML_NAMED_ELEMENT(TextId)

enum class TextId : int32_t {  // NOLINT(performance-enum-size)
"""
    dictionary += "".join(f"    {key},\n" for key in reference)
    dictionary += "    Count\n};\nQ_ENUM_NS(TextId)\n\n"
    for language, entries in dictionaries:
        dictionary += ("inline const std::array<QString, "
                       "static_cast<std::size_t>(TextId::Count)> "
                       f"strings_{language}{{    // NOLINT(bugprone-throwing-static-initialization)\n")
        dictionary += "".join(f"    {cpp_string(entries[key])},\n" for key in reference)
        dictionary += "};\n\n"
    dictionary += """struct Language {
    std::string_view code;
    std::span<const QString> texts;
};

inline const std::array languages{
"""
    dictionary += "".join(
        f'    Language{{.code="{language}", .texts=strings_{language}}},\n'
        for language, _ in dictionaries
    )
    dictionary += "};\n\n} // namespace mv::i18n\n"

    # Validate and render every dictionary before touching any output.
    destination = output_dir / output_name
    destination.parent.mkdir(parents=True, exist_ok=True)
    data = dictionary.encode("utf-8")
    if not destination.exists() or destination.read_bytes() != data:
        temporary = destination.with_suffix(".hpp.tmp")
        temporary.write_bytes(data)
        temporary.replace(destination)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--output-name", default="generated_translations.hpp",
                        help="header filename relative to the output directory")
    parser.add_argument("dictionaries", type=Path, nargs="+",
                        help="reference dictionary first")
    args = parser.parse_args()
    try:
        generate(args.dictionaries, args.output_dir, args.output_name)
    except (DictionaryError, OSError, UnicodeError) as error:
        print(f"Translation generation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
