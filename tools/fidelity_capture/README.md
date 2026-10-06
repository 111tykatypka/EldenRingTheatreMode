# Fidelity capture tools

Generate catalog/readers: `python generate_schema.py` (developer repo only).
Read-only real file inspection: `python inspect_capture.py path/to/replay.erplay`.
Use `--time 10 --output report.json` or `--export-jsonl all_records.jsonl`.
No executable, game memory, reference source or injector is touched by the inspector.
Schema1 immutable ERPLAY03 track IDs5..13; unavailable masks preserve distinction from zero; raw NaN/Inf bits retained.
See notes/PHASE8_CAPTURE_FIDELITY.md and PHASE8_CAPTURE_FIELDS.md for limitations/runtime test.
