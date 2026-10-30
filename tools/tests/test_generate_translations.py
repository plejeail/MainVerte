import importlib.util
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "generate_translations.py"
SPEC = importlib.util.spec_from_file_location("generate_translations", SCRIPT)
GENERATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GENERATOR)


class GeneratorTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.output = self.root / "generated"

    def dictionary(self, text, language="en"):
        path = self.root / f"{language}.dict"
        with path.open("w", encoding="utf-8", newline="") as destination:
            destination.write(text)
        return path

    def test_reference_order_and_language_registry(self):
        en = self.dictionary("App_Title,App\nCollection_Title,Collection\n")
        fr = self.dictionary("Collection_Title,Collection\nApp_Title,Application\n", "fr")
        GENERATOR.generate([en, fr], self.output)
        header = (self.output / "generated_translations.hpp").read_text()
        self.assertEqual([path.name for path in self.output.iterdir()],
                         ["generated_translations.hpp"])
        self.assertIn("enum class TextId : int32_t {", header)
        self.assertIn("Q_NAMESPACE", header)
        self.assertIn("QML_NAMED_ELEMENT(TextId)", header)
        self.assertIn("Q_ENUM_NS(TextId)", header)
        self.assertLess(header.index("App_Title"), header.index("Collection_Title"))
        self.assertLess(header.index("enum class TextId"), header.index("strings_en"))
        self.assertIn('Language{.code="en", .texts=strings_en}', header)
        self.assertIn('Language{.code="fr", .texts=strings_fr}', header)
        self.assertIn("inline const std::array<QString,", header)
        self.assertIn("std::span<const QString> texts;", header)
        french = header[header.index("strings_fr"):]
        self.assertLess(french.index("Application"), french.index("Collection"))

    def test_csv_escaping_and_unicode(self):
        source = self.dictionary('Text,"Été, ""bonjour""\nC:\\garden"\n')
        entries = GENERATOR.read_dictionary(source)
        self.assertEqual(entries["Text"], 'Été, "bonjour"\nC:\\garden')
        GENERATOR.generate([source], self.output)
        header = (self.output / "generated_translations.hpp").read_text()
        self.assertIn('QStringLiteral("\\u00c9t\\u00e9, \\042bonjour\\042\\012C:\\134garden")', header)

    def test_non_bmp_and_escape_boundaries(self):
        self.assertEqual(GENERATOR.cpp_string("🌱a\t7\r\n"),
                         'QStringLiteral("\\U0001f331a\\0117\\015\\012")')

    def test_invalid_inputs(self):
        cases = [
            ("A,a\nA,b\n", "duplicate ID"),
            ("app.title,a\n", "invalid C++ ID"),
            ("Count,a\n", "invalid C++ ID"),
            ("class,a\n", "invalid C++ ID"),
            ("Bad__Id,a\n", "invalid C++ ID"),
            ("A,a,b\n", "two CSV columns"),
            ('A,"unterminated', "unexpected end of data"),
            ("", "dictionary is empty"),
        ]
        for content, message in cases:
            with self.subTest(content=content):
                path = self.dictionary(content)
                with self.assertRaisesRegex(GENERATOR.DictionaryError, re.escape(message)):
                    GENERATOR.generate([path], self.output)
                self.assertFalse(self.output.exists())

    def test_missing_and_unknown_ids_preserve_outputs(self):
        en = self.dictionary("A,a\nB,b\n")
        fr = self.dictionary("B,b\nA,a\n", "fr")
        GENERATOR.generate([en, fr], self.output)
        before = {path: path.read_bytes() for path in self.output.iterdir()}
        fr.write_text("A,a\nC,c\n", encoding="utf-8")
        with self.assertRaisesRegex(GENERATOR.DictionaryError, "missing IDs: B; unknown IDs: C"):
            GENERATOR.generate([en, fr], self.output)
        self.assertEqual(before, {path: path.read_bytes() for path in self.output.iterdir()})

    def test_unchanged_outputs_keep_timestamps(self):
        en = self.dictionary("A,a\n")
        GENERATOR.generate([en], self.output)
        before = {path: path.stat().st_mtime_ns for path in self.output.iterdir()}
        GENERATOR.generate([en], self.output)
        self.assertEqual(before, {path: path.stat().st_mtime_ns for path in self.output.iterdir()})

    def test_duplicate_language(self):
        en = self.dictionary("A,a\n")
        with self.assertRaisesRegex(GENERATOR.DictionaryError, "duplicate language"):
            GENERATOR.generate([en, en], self.output)

    def test_legacy_dictionary_name_is_rejected(self):
        path = self.root / "MainVerte_en.dict"
        path.write_text("A,a\n", encoding="utf-8")
        with self.assertRaisesRegex(GENERATOR.DictionaryError, "expected <language>\\.dict"):
            GENERATOR.generate([path], self.output)
        self.assertFalse(self.output.exists())

    def test_utf8_bom_and_empty_translation(self):
        en = self.dictionary("\ufeffApp_Title,\n")
        GENERATOR.generate([en], self.output)
        header = (self.output / "generated_translations.hpp").read_text()
        self.assertIn('QStringLiteral("")', header)

    def test_invalid_utf8_reports_dictionary(self):
        en = self.dictionary("A,a\n")
        en.write_bytes(b"A,\xff\n")
        with self.assertRaises(GENERATOR.DictionaryError) as result:
            GENERATOR.generate([en], self.output)
        self.assertIn(str(en), str(result.exception))
        self.assertIn("invalid UTF-8", str(result.exception))
        self.assertFalse(self.output.exists())

    def test_text_change_preserves_ids(self):
        en = self.dictionary("A,a\n")
        GENERATOR.generate([en], self.output)
        header = self.output / "generated_translations.hpp"
        before = header.read_text().split("inline const", 1)[0]
        en.write_text("A,changed\n", encoding="utf-8")
        GENERATOR.generate([en], self.output)
        after = header.read_text()
        self.assertEqual(before, after.split("inline const", 1)[0])
        self.assertIn("changed", after)

    def test_cli_reports_file_and_line(self):
        en = self.dictionary("A,a\nA,b\n")
        result = subprocess.run(
            [sys.executable, "-B", str(SCRIPT), "--output-dir", str(self.output), str(en)],
            capture_output=True, text=True, check=False,
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn(f"{en}:2: duplicate ID", result.stderr)
        self.assertNotIn("Traceback", result.stderr)

    def test_cli_custom_header_destination(self):
        en = self.dictionary("A,a\n")
        result = subprocess.run(
            [sys.executable, "-B", str(SCRIPT), "--output-dir", str(self.output),
             "--output-name", "custom/translations.hpp", str(en)],
            capture_output=True, text=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("QStringLiteral", (self.output / "custom/translations.hpp").read_text())
        self.assertFalse((self.output / "generated_translations.hpp").exists())


if __name__ == "__main__":
    unittest.main()
