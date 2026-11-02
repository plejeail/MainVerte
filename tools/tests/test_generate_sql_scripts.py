import importlib.util
from pathlib import Path
import sqlite3
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("generate_sql", ROOT / "tools/generate_sql.py")
GENERATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GENERATOR)


class SqlGeneratorTests(unittest.TestCase):
    def test_header_generation_and_validation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            first = root / "0000_init.sql"
            second = root / "0001_next.sql"
            first.write_text("SELECT  1; -- comment", encoding="utf-8")
            second.write_text("SELECT 2;", encoding="utf-8")
            output = root / "generated/sql.hpp"
            GENERATOR.generate([second, first], output)
            header = output.read_text(encoding="utf-8")
            self.assertIn("std::array<std::string_view, 2>", header)
            self.assertLess(header.index("SELECT 1;"), header.index("SELECT 2;"))
            timestamp = output.stat().st_mtime_ns
            GENERATOR.generate([first, second], output)
            self.assertEqual(output.stat().st_mtime_ns, timestamp)
            with self.assertRaisesRegex(ValueError, "order"):
                GENERATOR.generate([second], output)
            first.write_text('SELECT \')MV_SQL"\';', encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "delimiter"):
                GENERATOR.generate([first], output)
            self.assertEqual(output.read_text(encoding="utf-8"), header)

    def test_whitespace_and_line_comments(self):
        self.assertEqual(
            GENERATOR.compact_sql("-- heading\r\n\nSELECT   1;\n\n SELECT\t2; -- end"),
            "SELECT 1;SELECT 2;",
        )
        self.assertEqual(GENERATOR.compact_sql("SELECT-- comment\n 1;"), "SELECT 1;")

    def test_quoted_content_and_block_comments(self):
        sql = "SELECT 'a  -- b;\n''c', \"a  -- b\", `x  y`, [a  b] /* -- keep */;"
        self.assertEqual(GENERATOR.compact_sql(sql), sql)

    def test_preserves_sql_values(self):
        sql = "CREATE TABLE t(v TEXT);\nINSERT INTO t VALUES ('a  -- b;\n''c'); -- end\n"
        with sqlite3.connect(":memory:") as connection:
            connection.executescript(GENERATOR.compact_sql(sql))
            self.assertEqual(connection.execute("SELECT v FROM t").fetchone()[0], "a  -- b;\n'c")

    def test_initial_schema(self):
        sql = (ROOT / "core/sql/0000_init.sql").read_text(encoding="utf-8")
        compact = GENERATOR.compact_sql(sql)
        self.assertNotIn("\n", compact)
        self.assertNotIn("--", compact)
        with sqlite3.connect(":memory:") as connection:
            connection.executescript(compact)
            self.assertEqual(connection.execute("SELECT language, dark_theme, log_level, debug_enabled FROM settings").fetchone(), ("", 0, 0, 0))


if __name__ == "__main__":
    unittest.main()
