"""Read-only Cheat Engine XML index. Never evaluates Lua/assembler or attaches to a process."""
import argparse
from contextlib import closing
import hashlib
import json
from pathlib import Path
import sqlite3
import xml.etree.ElementTree as ET


def index(source, output):
    raw = source.read_bytes()
    root = ET.fromstring(raw)
    if root.tag != "CheatTable":
        raise ValueError("Not a Cheat Engine table")
    output.parent.mkdir(parents=True, exist_ok=True)
    # Exclusive creation prevents accidental replacement of an existing research index.
    with output.open("xb"):
        pass
    with closing(sqlite3.connect(output)) as db, db:
        db.executescript("""
        CREATE TABLE metadata(key TEXT PRIMARY KEY, value TEXT NOT NULL);
        CREATE TABLE entries(id INTEGER PRIMARY KEY, ce_id TEXT, parent INTEGER,
          path TEXT NOT NULL, description TEXT, variable_type TEXT, address TEXT, offsets TEXT);
        CREATE TABLE texts(id INTEGER PRIMARY KEY, entry_id INTEGER, kind TEXT, text TEXT);
        CREATE VIRTUAL TABLE search USING fts5(path, kind, text, tokenize='unicode61');
        """)
        for key, value in {"source": str(source.resolve()), "sha256": hashlib.sha256(raw).hexdigest(),
                           "bytes": str(len(raw)), "table_version": root.get("CheatEngineTableVersion", ""),
                           "safety": "Static XML only; scripts and embedded files not executed"}.items():
            db.execute("INSERT INTO metadata VALUES (?,?)", (key, value))

        def add_text(entry, path, kind, text):
            if text:
                db.execute("INSERT INTO texts(entry_id,kind,text) VALUES (?,?,?)", (entry, kind, text))
                db.execute("INSERT INTO search VALUES (?,?,?)", (path, kind, text))

        def walk(container, parent=None, path=""):
            if container is None:
                return
            for node in container.findall("CheatEntry"):
                description = node.findtext("Description", "").strip('"')
                current = path + "/" + description
                offsets = [n.text for n in node.findall("Offsets/Offset")]
                cur = db.execute("INSERT INTO entries(ce_id,parent,path,description,variable_type,address,offsets) "
                                 "VALUES (?,?,?,?,?,?,?)", (node.findtext("ID"), parent, current, description,
                                  node.findtext("VariableType"), node.findtext("Address"), json.dumps(offsets)))
                eid = cur.lastrowid
                add_text(eid, current, "description", description)
                for tag in ("AssemblerScript", "LuaScript", "DropDownList", "Address"):
                    add_text(eid, current, tag, node.findtext(tag))
                walk(node.find("CheatEntries"), eid, current)

        walk(root.find("CheatEntries"))
        add_text(None, "/", "LuaScript", root.findtext("LuaScript"))
        add_text(None, "/", "Comments", root.findtext("Comments"))
        # Omit encoded Forms/Files from search, but inventory their names and encoded lengths.
        embedded = [{"kind": section, "tag": n.tag, "attributes": n.attrib,
                     "encoded_chars": len(n.text or "")} for section in ("Forms", "Files")
                    for n in root.findall(section + "/*")]
        db.execute("INSERT INTO metadata VALUES (?,?)", ("embedded_inventory", json.dumps(embedded)))
        print(json.dumps({"database": str(output), "entries": db.execute("SELECT count(*) FROM entries").fetchone()[0],
                          "texts": db.execute("SELECT count(*) FROM texts").fetchone()[0],
                          "embedded": len(embedded), "sha256": hashlib.sha256(raw).hexdigest()}, indent=2))


def query(database, term, limit, full):
    uri = database.resolve().as_uri() + "?mode=ro"
    with closing(sqlite3.connect(uri, uri=True)) as db:
        rows = db.execute("SELECT path,kind,text FROM search WHERE search MATCH ? LIMIT ?", (term, limit))
        for path, kind, text in rows:
            print(json.dumps({"path": path, "kind": kind, "text": text if full else text[:600]}, ensure_ascii=False))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    build = sub.add_parser("index")
    build.add_argument("source", type=Path)
    build.add_argument("output", type=Path)
    search = sub.add_parser("query")
    search.add_argument("database", type=Path)
    search.add_argument("term", help="SQLite FTS5 query, e.g. Warp OR RideParam")
    search.add_argument("--limit", type=int, default=10)
    search.add_argument("--full", action="store_true")
    args = parser.parse_args()
    if args.command == "index":
        index(args.source, args.output)
    else:
        query(args.database, args.term, args.limit, args.full)


if __name__ == "__main__":
    main()
