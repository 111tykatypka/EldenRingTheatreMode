#!/bin/sh
# Pure-logic unit tests that need neither the game nor native libraries.
# Full adapter tests: THEATER_NATIVE_LIBRARY_ROOT=<native build dir> cargo test --lib --release (see docs).
set -e
cd "$(dirname "$0")"
out=${TMPDIR:-/tmp}/tm_rust_unit
mkdir -p "$out"
rustc --edition 2024 --test lifetime_harness.rs -o "$out/lifetime.exe" && "$out/lifetime.exe"
rustc --edition 2024 --test ../../adapter/src/replay_interpolation.rs -o "$out/interp.exe" && "$out/interp.exe"
