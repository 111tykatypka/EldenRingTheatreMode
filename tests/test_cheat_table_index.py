import contextlib
import importlib.util
import io
from pathlib import Path
import sqlite3
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("ct_index", Path(__file__).parents[1] / "scripts/index_cheat_table.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class IndexTests(unittest.TestCase):
    def test_nested_unicode_and_scripts_are_only_text(self):
        with tempfile.TemporaryDirectory(dir=Path(__file__).parents[1]) as temp:
            root = Path(temp)
            source = root / "источник.CT"
            source.write_text('''<CheatTable CheatEngineTableVersion="52"><CheatEntries>
              <CheatEntry><ID>100</ID><Description>"World"</Description><CheatEntries>
              <CheatEntry><ID>101</ID><Description>"Место"</Description><Address>WorldChrMan</Address>
              <Offsets><Offset>E8</Offset><Offset>190</Offset></Offsets>
              <AssemblerScript>DO_NOT_EXECUTE Warp</AssemblerScript>
              <DropDownList>1042362951:The First Step</DropDownList></CheatEntry>
              </CheatEntries></CheatEntry></CheatEntries>
              <Files><File Name="binary" Encoding="Ascii85">opaque</File></Files></CheatTable>''', encoding="utf-8")
            original = source.read_bytes()
            db_path = root / "index.sqlite"
            with contextlib.redirect_stdout(io.StringIO()):
                module.index(source, db_path)
            self.assertEqual(original, source.read_bytes())
            with contextlib.closing(sqlite3.connect(db_path)) as db:
                self.assertEqual(db.execute("SELECT count(*) FROM entries").fetchone()[0], 2)
                row = db.execute("SELECT path,parent,offsets FROM entries WHERE ce_id='101'").fetchone()
                self.assertEqual(row, ('/World/Место', 1, '["E8", "190"]'))
                self.assertEqual(db.execute("SELECT count(*) FROM search WHERE search MATCH 'Warp'").fetchone()[0], 1)
                self.assertEqual(db.execute("SELECT count(*) FROM texts WHERE text='opaque'").fetchone()[0], 0)
            with contextlib.redirect_stdout(io.StringIO()) as out:
                module.query(db_path, "Warp", 10, True)
            self.assertIn("DO_NOT_EXECUTE", out.getvalue())
            before = db_path.read_bytes()
            with self.assertRaises(FileExistsError):
                module.index(source, db_path)
            self.assertEqual(before, db_path.read_bytes())


if __name__ == "__main__":
    unittest.main()
