# Research integration utilities

Standard-library Python 3.10+. References/game remain read-only. No injection or engine calls.

`python tools/research_integration/integrate_sources.py`

Scans only executable sections of the exact hash-verified analysis copy using short source anchor patterns; saves source revision, match sites, RIP targets, confidence and unresolved ABI/lifetime in `research/symbols_2_7_0_0.json`. Singleton values are slot addresses, not current instance addresses. Pattern uniqueness never enables runtime calls automatically.

`python tools/research_integration/analyze_runtime_trace.py <trace.jsonl> --output summary.json`

Streams JSONL with bounded per-actor previous-target storage, reports stage/failure counts, non-finite/invalid quaternion samples, queue drops, immediate/next-callback displacement and player PostPhysics sampling rate. A difference on the next callback can be normal simulation. Host logs provide ReplayEntityId → native handle/session correspondence; the control packet carries native identity, not a raw pointer. Caller must compare that correspondence when diagnosing lookup failures. Missing transforms are explicit, not zero samples.

`python -m unittest discover -s tools/research_integration -p test_tools.py`

Three synthetic **utility-only** tests cover wildcard ambiguity, compact CE wildcard tokens, differential math and corrupt rows. They are never described as game samples.

Local full disassembly remains outside the repository under the research `targeted` folder; no original executable, decompiled game code or third-party tool binaries are committed or bundled.
